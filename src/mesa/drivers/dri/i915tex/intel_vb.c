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

static void intel_vb_build_local_verts( struct intel_vb *vb )
{
   /* Build window space vertex buffer in local memory.
    */   
   _tnl_emit_vertices_to_buffer( &vb->intel->ctx, 
				 0, 
				 vb->nr_verts, 
				 vb->local.verts );
   
   vb->local.dirty = 0;
}


GLboolean intel_vb_validate_vertices( struct intel_vb *vb,
				      GLuint flags )
{
   GLboolean success = GL_TRUE;

   if (flags & VB_LOCAL_VERTS) {
      if (vb->local.dirty)
	 intel_vb_build_local_verts( vb );
   }

   if (flags & VB_HW_VERTS) {
      if (vb->vbo.dirty) {
	 if (!vb->local.dirty) 
	    success = intel_vb_copy_hw_vertices( vb );
	 else 
	    success = intel_vb_emit_hw_vertices( vb );
      }
   }

   return success;
}



void intel_vb_set_inputs( struct intel_vb *vb,
			  const struct tnl_attr_map *attrs,
			  GLuint count )
{
   struct intel_context *intel = vb->intel;

   GLuint vertex_size = _tnl_install_attrs( &intel->ctx, 
					    attrs, 
					    count,
					    intel->ViewportMatrix.m, 0 ); 

   if (vertex_size != vb->vertex_size_bytes) {
      vb->vertex_size_bytes = vertex_size;
      vb->intel->state.dirty.intel |= INTEL_NEW_VERTEX_SIZE;
   }
}




/* Don't emit or build yet as we don't know what sort of vertices the
 * renderer wants.
 */
void intel_vb_new_vertices( struct intel_vb *vb )
{
   /* Always have to do this:
    */
   vb->tnl->clipspace.new_inputs |= VERT_BIT_POS;
   vb->tnl->vb.AttribPtr[VERT_ATTRIB_POS] = vb->tnl->vb.NdcPtr;

   vb->local.dirty = 1;
   vb->vbo.dirty = 1;
   vb->nr_verts = vb->tnl->vb.Count;
}


void intel_vb_release_vertices( struct intel_vb *vb )
{
   vb->local.dirty = 1;
   vb->vbo.dirty = 1;
   vb->nr_verts = 0;

#if 0
   if (intel->state.vbo) {      
      intel->state.vbo = 0;
      intel->state.vbo_offset = 0;
   }
#endif

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

   vb->tnl = TNL_CONTEXT(ctx);

   _tnl_init_vertices(ctx, vb->tnl->vb.Size, MAX_VERTEX_SIZE );

   vb->local.verts = vb->tnl->clipspace.vertex_buf;


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
