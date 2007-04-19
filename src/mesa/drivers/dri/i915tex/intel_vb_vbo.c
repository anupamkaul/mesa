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



#define VBO_SIZE (128*1024)



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
   vb->vbo.current_used = 0;

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
      
static void *intel_vb_alloc( struct intel_vb *vb, GLuint space )
{
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


   {
      struct intel_context *intel = vb->intel;
      intel->state.vbo = vb->vbo.current->buffer;
      intel->state.vbo_offset = vb->vbo.current_used;
      intel->state.dirty.intel |= INTEL_NEW_VBO;
   }


   ptr = vb->vbo.current_ptr + vb->vbo.current_used;
   vb->vbo.current_used += space;
   return ptr;
}


GLboolean intel_vb_copy_hw_vertices( struct intel_vb *vb )
{
   const void *src = vb->local.verts;
   GLuint size = vb->nr_verts * vb->vertex_size_bytes;
   void *dest = intel_vb_alloc( vb, size );

   if (dest) {
      /* Eventually (soon) will have to avoid changing state every time
       * we allocate more vertices.  In that case, will need to return a
       * non-zero offset to adjust primitives by.
       */
      __memcpy( dest, src, size );
      vb->vbo.offset = 0;
      vb->vbo.dirty = 0;
      return GL_TRUE;
   }
   else
      return GL_FALSE;
}


GLboolean intel_vb_emit_hw_vertices( struct intel_vb *vb )
{
   GLuint size = vb->nr_verts * vb->vertex_size_bytes;
   void *dest = intel_vb_alloc( vb, size );

   _mesa_printf("emit %d verts (each %d bytes) to %p\n", 
		vb->nr_verts, 
		vb->vertex_size_bytes,
		dest );


   if (dest) {
      _tnl_emit_vertices_to_buffer( &vb->intel->ctx, 
				    0, 
				    vb->nr_verts, 
				    dest );

#if 0
      {
	 GLuint i;
	 union { float f; int i; } *fi = dest;

	 for (i = 0; i < vb->nr_verts; i++) {
	    _mesa_printf("%d: %f %f %f %x\n",
			 i,
			 fi[0].f,
			 fi[1].f,
			 fi[2].f,
			 fi[3].i);
	    fi += 4;
	 }
      }
#endif


      vb->vbo.offset = 0;
      vb->vbo.dirty = 0;
      return GL_TRUE;
   }
   else
      return GL_FALSE;
}


GLuint intel_vb_get_vbo_index_offset( struct intel_vb *vb )
{
   assert(!vb->vbo.dirty);

   return vb->vbo.offset;
}



/* Callback from (eventually) intel_batchbuffer_flush().  Prepare for
 * submit to hardware.
 */
void intel_vb_flush( struct intel_vb *vb )
{
   DBG("%s\n", __FUNCTION__);

   if (vb->vbo.current_ptr) 
      intel_vb_unmap_current_vbo( vb );

   vb->vbo.current_used = vb->vbo.current_size;
   vb->vbo.wrap_vbo = vb->vbo.idx;
}
