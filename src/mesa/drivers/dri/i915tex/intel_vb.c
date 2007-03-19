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

/*
 * Render vertex buffers by emitting vertices directly to dma buffers.
 */
#include "glheader.h"
#include "mtypes.h"

#include "intel_context.h"
#include "intel_buffer_objects.h"
#include "intel_vb.h"


#define FILE_DEBUG_FLAG DEBUG_VBO



#define MAX_VBO 32		/* XXX: make dynamic */
#define VBO_SIZE (128*1024)




struct intel_vb {
   struct intel_context *intel;

   struct intel_buffer_object *vbo[MAX_VBO];
   GLuint vbo_idx;

   struct intel_buffer_object *current_vbo;

   GLuint current_vbo_size;
   GLuint current_vbo_used;
   void *current_vbo_ptr;

   GLuint wrap_vbo;
   GLuint dynamic_start;
};



static void
unmap_current_vbo( struct intel_vb *vb )
{
   GLcontext *ctx = &vb->intel->ctx;

   DBG("%s\n", __FUNCTION__);

   assert(vb->current_vbo_ptr);

   ctx->Driver.UnmapBuffer( ctx, 
			    GL_ARRAY_BUFFER_ARB,
			    &vb->current_vbo->Base );

   vb->current_vbo_ptr = NULL;
}





static GLboolean 
get_next_vbo( struct intel_vb *vb, GLuint size )
{
   GLcontext *ctx = &vb->intel->ctx;
   GLuint next_vbo;

   DBG("%s\n", __FUNCTION__);

   /* Unmap current vbo:
    */
   if (vb->current_vbo_ptr) 
      unmap_current_vbo( vb );

   if (size < VBO_SIZE) 
      size = VBO_SIZE;

   next_vbo = (vb->vbo_idx + 1) % MAX_VBO;

   if (next_vbo == vb->wrap_vbo)
      return GL_FALSE;
   
   vb->vbo_idx = next_vbo;
   vb->current_vbo = vb->vbo[vb->vbo_idx];
   vb->current_vbo_size = size;
   vb->current_vbo_used = 0;

   /* Clear out buffer contents and break any hardware dependency on
    * the old memory:
    */
   ctx->Driver.BufferData( ctx,
			   GL_ARRAY_BUFFER_ARB,
			   size,
			   NULL,
			   GL_DYNAMIC_DRAW_ARB,
			   &vb->current_vbo->Base );

   
   return GL_TRUE;
}
      
void *intel_vb_alloc( struct intel_vb *vb, GLuint space )
{
   GLcontext *ctx = &vb->intel->ctx;
   void *ptr;

   DBG("%s %d, vbo %d\n", __FUNCTION__, space, vb->vbo_idx);

   assert(vb->dynamic_start == ~0);

   if (vb->current_vbo_used + space > vb->current_vbo_size) {
      if (!get_next_vbo( vb, space ))
	 return NULL;
   }
      
   assert(vb->current_vbo_used + space <= vb->current_vbo_size);

   if (!vb->current_vbo_ptr) {
      DBG("%s map vbo %d\n", __FUNCTION__, vb->vbo_idx);

      /* Map the vbo now, will be unmapped in unmap_current_vbo, above.
       */
      vb->current_vbo_ptr = ctx->Driver.MapBuffer( ctx,
						   GL_ARRAY_BUFFER_ARB,
						   GL_WRITE_ONLY,
						   &vb->current_vbo->Base );
   }

   if (!vb->current_vbo_ptr) 
      return NULL;


   {
      struct intel_context *intel = vb->intel;
      intel->state.vbo = vb->current_vbo->buffer;
      intel->state.vbo_offset = vb->current_vbo_used;
      intel->state.dirty.intel |= INTEL_NEW_VBO;
   }


   ptr = vb->current_vbo_ptr + vb->current_vbo_used;
   vb->current_vbo_used += space;
   return ptr;
}

#define MIN_DYNAMIC_FREE_SPACE (1024)


GLboolean intel_vb_begin_dynamic_alloc( struct intel_vb *vb, 
					GLuint min_free_space )
{
   /* Just make sure there is a certain amount of free space left in
    * this buffer:
    */
   void *ptr = intel_vb_alloc( vb,  min_free_space );
   if (ptr == NULL)
      return GL_FALSE;
   
   vb->current_vbo_used -= min_free_space;
   vb->dynamic_start = vb->current_vbo_used;
   return GL_TRUE;
}




void *intel_vb_extend_dynamic_alloc( struct intel_vb *vb, GLuint space )
{
   if (vb->current_vbo_used + space > vb->current_vbo_size)
      return NULL;
   else {
      void *ptr = vb->current_vbo_ptr + vb->current_vbo_used;
      vb->current_vbo_used += space;
      return ptr;
   }
}


GLuint intel_vb_end_dynamic_alloc( struct intel_vb *vb )
{
   GLuint start = vb->dynamic_start;

   assert(start <= vb->current_vbo_used);
   
   vb->dynamic_start = ~0;
   return vb->current_vbo_used - start;
}



/* Callback from (eventually) intel_batchbuffer_flush().  Prepare for
 * submit to hardware.
 */
void intel_vb_flush( struct intel_vb *vb )
{
   DBG("%s\n", __FUNCTION__);

   if (vb->current_vbo_ptr) 
      unmap_current_vbo( vb );

   vb->current_vbo_used = vb->current_vbo_size;
   vb->wrap_vbo = vb->vbo_idx;
}

struct intel_vb *intel_vb_init( struct intel_context *intel )
{
   struct intel_vb *vb = CALLOC_STRUCT( intel_vb );
   GLcontext *ctx = &intel->ctx;
   GLuint i;

   vb->intel = intel;
   vb->dynamic_start = ~0;

   for (i = 0; i < MAX_VBO; i++) {
      vb->vbo[i] = (struct intel_buffer_object *) 
	 ctx->Driver.NewBufferObject(ctx, 1, GL_ARRAY_BUFFER_ARB);
   }

   return vb;
}

void intel_vb_destroy( struct intel_vb *vb )
{
   GLcontext *ctx = &vb->intel->ctx;
   GLuint i;

   if (vb) {
      if (vb->current_vbo_ptr)
	 unmap_current_vbo( vb );

      /* Destroy the vbo: 
       */
      for (i = 0; i < MAX_VBO; i++)
	 if (vb->vbo[i])
	    ctx->Driver.DeleteBuffer( ctx, &vb->vbo[i]->Base );
      
      FREE( vb );
   }
}
