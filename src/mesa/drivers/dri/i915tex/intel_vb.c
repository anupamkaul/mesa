/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

/* Manage hardware format vertices.  In part this is a wrapper around
 * the tnl/t_vertex.[ch] functionality.
 */
#include "glheader.h"
#include "mtypes.h"

#include "intel_context.h"
#include "intel_buffer_objects.h"
#include "intel_vb.h"


#define FILE_DEBUG_FLAG DEBUG_VBO

#define MAX_VERTEX_SIZE (36 * sizeof(GLfloat))

#define VBO_SIZE (128*1024)



static void 
start_new_block( struct intel_vb *vb )
{
   struct intel_context *intel = vb->intel;
   
   vb->vbo.current_block_start = vb->vbo.current_used;

   intel->state.vbo = vb->vbo.current->buffer;   
   intel->state.vbo_offset = vb->vbo.current_block_start;
   intel->state.dirty.intel |= INTEL_NEW_VBO;
}



void intel_vb_set_vertex_size( struct intel_vb *vb,
			       GLuint vertex_size )
{
   if (vertex_size != vb->vertex_size_bytes) {
      vb->vertex_size_bytes = vertex_size;
      vb->intel->state.dirty.intel |= INTEL_NEW_VERTEX_SIZE;

      if (vb->vbo.current_used < vb->vbo.current_size)
	 start_new_block( vb );
   }
}






void
intel_vb_unmap_current_vbo( struct intel_vb *vb )
{
   GLcontext *ctx = &vb->intel->ctx;

   DBG("%s\n", __FUNCTION__);

   assert(vb->vbo.current_ptr);

   ctx->Driver.UnmapBuffer( ctx, 
			    GL_ARRAY_BUFFER_ARB,
			    &vb->vbo.current->Base );

   vb->vbo.current_ptr = NULL;
}



static GLboolean 
get_next_vbo( struct intel_vb *vb, GLuint size )
{
   GLcontext *ctx = &vb->intel->ctx;
   GLuint next_vbo;

   DBG("%s\n", __FUNCTION__);

   /* Unmap current vbo:
    */
   if (vb->vbo.current_ptr) 
      intel_vb_unmap_current_vbo( vb );

   if (size < VBO_SIZE) 
      size = VBO_SIZE;

   next_vbo = (vb->vbo.idx + 1) % MAX_VBO;

   if (next_vbo == vb->vbo.wrap_vbo)
      return GL_FALSE;
   
   vb->vbo.idx = next_vbo;
   vb->vbo.current = vb->vbo.vbo[vb->vbo.idx];
   vb->vbo.current_size = size;
   vb->vbo.current_block_start = 0;
   vb->vbo.current_used = 0;

   start_new_block( vb );

   /* Clear out buffer contents and break any hardware dependency on
    * the old memory:
    */
   ctx->Driver.BufferData( ctx,
			   GL_ARRAY_BUFFER_ARB,
			   size,
			   NULL,
			   GL_DYNAMIC_DRAW_ARB,
			   &vb->vbo.current->Base );

   
   return GL_TRUE;
}
      

void *intel_vb_alloc_vertices( struct intel_vb *vb, 
			       GLuint nr_vertices,
			       GLuint *offset_return )
{
   GLuint space = nr_vertices * vb->vertex_size_bytes;

   GLcontext *ctx = &vb->intel->ctx;
   void *ptr;

   DBG("%s %d, vbo %d\n", __FUNCTION__, space, vb->vbo.idx);

   if (vb->vbo.current_used + space > vb->vbo.current_size) {
      if (!get_next_vbo( vb, space ))
	 return NULL;
   }
      
   assert(vb->vbo.current_used + space <= vb->vbo.current_size);

   if (!vb->vbo.current_ptr) {
      DBG("%s map vbo %d\n", __FUNCTION__, vb->vbo.idx);

      /* Map the vbo now, will be unmapped in unmap_current_vbo, above.
       */
      vb->vbo.current_ptr = ctx->Driver.MapBuffer( ctx,
						   GL_ARRAY_BUFFER_ARB,
						   GL_WRITE_ONLY,
						   &vb->vbo.current->Base );
   }

   if (!vb->vbo.current_ptr) 
      return NULL;


   *offset_return = (vb->vbo.current_used - vb->vbo.current_block_start) / vb->vertex_size_bytes;

   ptr = vb->vbo.current_ptr + vb->vbo.current_used;
   vb->vbo.current_used += space;   
   return ptr;
}




/* Callback from (eventually) intel_batchbuffer_flush().  Prepare for
 * submit to hardware.
 */
void intel_vb_flush( struct intel_vb *vb )
{
   DBG("%s\n", __FUNCTION__);

   if (vb->vbo.current_ptr) 
      intel_vb_unmap_current_vbo( vb );

   if (vb->vbo.current_used) {
      vb->vbo.current_used = vb->vbo.current_size;
      vb->vbo.wrap_vbo = vb->vbo.idx;
      get_next_vbo( vb, 0 );
   }   
}




struct intel_vb *intel_vb_init( struct intel_context *intel )
{
   struct intel_vb *vb = CALLOC_STRUCT( intel_vb );
   GLcontext *ctx = &intel->ctx;
   GLuint i;

   vb->intel = intel;

   for (i = 0; i < MAX_VBO; i++) {
      vb->vbo.vbo[i] = (struct intel_buffer_object *) 
	 ctx->Driver.NewBufferObject(ctx, 1, GL_ARRAY_BUFFER_ARB);

      /* We get a segfault if we try and delete buffer objects without
       * supplying some data for them, even if it is null.
       */
      ctx->Driver.BufferData( ctx,
			      GL_ARRAY_BUFFER_ARB,
			      128*1024,
			      NULL,
			      GL_DYNAMIC_DRAW_ARB,
			      &vb->vbo.vbo[i]->Base );


   }

   get_next_vbo( vb, 0 );
   return vb;
}

void intel_vb_destroy( struct intel_vb *vb )
{
   GLcontext *ctx = &vb->intel->ctx;
   GLuint i;

   if (vb) {
      if (vb->vbo.current_ptr)
	 intel_vb_unmap_current_vbo( vb );

      /* Destroy the vbo: 
       */
      for (i = 0; i < MAX_VBO; i++)
	 if (vb->vbo.vbo[i])
	    ctx->Driver.DeleteBuffer( ctx, &vb->vbo.vbo[i]->Base );
      
      FREE( vb );
   }
}



