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
#include "context.h"
#include "macros.h"
#include "imports.h"
#include "mtypes.h"
#include "enums.h"

#include "tnl/t_context.h"
#include "tnl/t_vertex.h"

#include "intel_screen.h"
#include "intel_context.h"
#include "intel_tris.h"
#include "intel_batchbuffer.h"
#include "intel_buffer_objects.h"
#include "intel_reg.h"
#include "intel_idx_render.h"

/* XXX: NAUGHTY:
 */
#include "i915_reg.h"

static GLboolean check_idx_render(GLcontext *ctx, 
				  struct vertex_buffer *VB)
{
   struct intel_context *intel = intel_context(ctx);
   GLuint i;

   if (intel->Fallback != 0 ||
       intel->RenderIndex != 0)
      return GL_FALSE;

   for (i = 0; i < VB->PrimitiveCount; i++) {
      if (VB->Primitive[i].mode == GL_POINTS)
	 return GL_FALSE;
   }

   return GL_TRUE;
}

      
static GLuint get_max_vb_size( GLcontext *ctx )
{
   /* Basically arbitarily large. 
    */
   return 16 * 1024 * 1024;
}

static void
build_and_emit_vertices(GLcontext * ctx, GLuint nr)
{
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   void *ptr;

   /* Clear out buffer contents and break any hardware dependency on
    * the old memory:
    */
   ctx->Driver.BufferData( ctx,
			   GL_ARRAY_BUFFER_ARB,
			   nr * intel->vertex_size,
			   NULL,
			   GL_DYNAMIC_DRAW_ARB,
			   &intel->vertex_buffer_obj->Base );

   ptr = ctx->Driver.MapBuffer( ctx,
				GL_ARRAY_BUFFER_ARB,
				GL_WRITE_ONLY,
				&intel->vertex_buffer_obj->Base );
				
   tnl->clipspace.new_inputs |= VERT_BIT_POS;
   _tnl_emit_vertices_to_buffer( ctx, 0, nr, ptr );
   
   ctx->Driver.UnmapBuffer( ctx, 
			    GL_ARRAY_BUFFER_ARB,
			    &intel->vertex_buffer_obj->Base );
}

/* Emits vertices previously built by a call to BuildVertices.
 */
static void emit_built_vertices( GLcontext *ctx, GLuint nr )
{
   struct intel_context *intel = intel_context(ctx);

   ctx->Driver.BufferData( ctx,
			   GL_ARRAY_BUFFER_ARB,
			   nr * intel->vertex_size,
			   _tnl_get_vertex( ctx, 0 ),
			   GL_DYNAMIC_DRAW_ARB,
			   &intel->vertex_buffer_obj->Base );
}

/* Emit primitives and indices referencing the previously emitted
 * vertex buffer.
 */
static void emit_prims( GLcontext *ctx,
			const struct _mesa_prim *prim,
			GLuint nr_prims,
			const GLuint *indices,
			GLuint nr_indices )
{
   struct intel_context *intel = intel_context(ctx);
   GLuint i;

   assert(indices);

   /* State has already been emitted by the RenderStart callback. 
    */

   /* Additionally, emit our vertex buffer state.
    * 
    * XXX: need to push into the regular state mechanism.
    */
   BEGIN_BATCH(3, 0);

   OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	     I1_LOAD_S(0) |
	     I1_LOAD_S(1) |
	     2);

   OUT_RELOC(intel->vertex_buffer_obj->buffer,
	     DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
	     DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
	     0);


   /* Currently size==pitch for vertices, dword alignment, is this
    * ok???
    */
   OUT_BATCH((intel->vertex_size << 24) |
	     (intel->vertex_size << 16));

   ADVANCE_BATCH();
   
   

   for (i = 0; i < nr_prims; i++) {
      GLuint nr, hw_prim;

      switch (prim[i].mode) {
      case GL_TRIANGLES:
	 hw_prim = PRIM3D_TRILIST;
	 nr = prim[i].count - prim[i].count % 3;
	 break;
      case GL_LINES:
	 hw_prim = PRIM3D_LINELIST;
	 nr = prim[i].count - prim[i].count % 2;
	 break; 
      default:
	 assert(0);
	 continue;
      }

      if (nr == 0)
	 continue;
	  
      /* I'm assuming the elts are dwords, but are they?? 
       */
      BEGIN_BATCH(nr + 2, 0);
      OUT_BATCH(0);
      OUT_BATCH( _3DPRIMITIVE | 
		 hw_prim | 
		 PRIM_INDIRECT | 
		 PRIM_INDIRECT_ELTS | 
		 nr );
      
      /* XXX: is there a limit on this?  Need to split large prims?? 
       */
      memcpy(intel->batch->ptr, 
	     indices + prim->start,
	     nr * sizeof(GLuint));

      ADVANCE_BATCH();
   }
}


void intel_idx_init( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   tnl->Driver.Render.CheckIdxRender       = check_idx_render;
   tnl->Driver.Render.GetMaxVBSize         = get_max_vb_size;
   tnl->Driver.Render.BuildAndEmitVertices = build_and_emit_vertices;
   tnl->Driver.Render.EmitBuiltVertices    = emit_built_vertices;
   tnl->Driver.Render.EmitPrims            = emit_prims;


   /* Create the vbo:
    */
   intel->vertex_buffer_obj = 
      (struct intel_buffer_object *) ctx->Driver.NewBufferObject(ctx, 1, GL_ARRAY_BUFFER_ARB);

   assert(intel->vertex_buffer_obj);
}

void intel_idx_destroy( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   /* Destroy the vbo: 
    */
   if (intel->vertex_buffer_obj)
      ctx->Driver.DeleteBuffer( ctx, &intel->vertex_buffer_obj->Base );
}
