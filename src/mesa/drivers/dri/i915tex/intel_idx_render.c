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
#include "intel_vb.h"
#include "intel_reg.h"
#include "intel_state.h"
#include "intel_idx_render.h"


#define FILE_DEBUG_FLAG DEBUG_IDX



/* Basically limited to what is addressable by the 16-bit indices,
 * and the number of indices we can fit in a batchbuffer after
 * making room for state.
 */
#define HW_MAX_INDEXABLE_VERTS   0xfffe 
#define HW_MAX_INDICES           ((BATCH_SZ - 1024) / 2)


/* Have to fallback for:
 *     - points (needs different viewport)
 *     - twoside light
 *     - z offset
 *     - unfilled prims
 *     - lines && linestipple
 *     - tris && tristipple && !hwstipple
 *     - point attenuation (bug!)
 *     - aa tris && strict-conformance
 *     - aa points && strict-conformance 
 *     - PLUS: any fallback-to-swrast condition (intel->Fallback)
 *
 * If binning, need to flush bins and fall
 */
static GLboolean check_idx_render(GLcontext *ctx, 
				  struct vertex_buffer *VB,
				  GLuint *max_nr_verts,
				  GLuint *max_nr_indices )

{
   struct intel_context *intel = intel_context(ctx);
   GLuint i;

   if (intel->Fallback != 0 ||
       intel->RenderIndex != 0)
      return GL_FALSE;

   /* These are constant, but for some hardware they might vary
    * depending on the state, eg. according to vertex size.
    */
   *max_nr_verts = HW_MAX_INDEXABLE_VERTS;
   *max_nr_indices = HW_MAX_INDICES;

   /* Fix points with different dstorg bias state??  or use different
    * viewport transform in this case only (requires flush at level
    * above).
    */
   for (i = 0; i < VB->PrimitiveCount; i++) {
      if (VB->Primitive[i].mode == GL_POINTS)
	 return GL_FALSE;
   }

   return GL_TRUE;
}

     



static void
build_and_emit_vertices(GLcontext * ctx, GLuint nr)
{
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   void *ptr = intel_vb_alloc( intel->vb, nr * intel->vertex_size * 4 );

   DBG("%s %d\n", __FUNCTION__, nr);

   assert(tnl->clipspace.vertex_size == intel->vertex_size * 4);

   tnl->clipspace.new_inputs |= VERT_BIT_POS;
   _tnl_emit_vertices_to_buffer( ctx, 0, nr, ptr );
}

/* Emits vertices previously built by a call to BuildVertices.
 *
 * XXX: have t_vertex.c use our buffer to build into and avoid the
 * copy (assuming our buffer is cached...)
 */
static void emit_built_vertices( GLcontext *ctx, GLuint nr )
{
   struct intel_context *intel = intel_context(ctx);
   void *ptr = intel_vb_alloc( intel->vb, nr * intel->vertex_size * 4 );

   DBG("%s %d\n", __FUNCTION__, nr);

   memcpy(ptr, _tnl_get_vertex(ctx, 0), 
	  nr * intel->vertex_size * sizeof(GLuint));
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
   GLuint i, j;

   assert(indices);

   DBG("%s - start\n", __FUNCTION__);
   

   for (i = 0; i < nr_prims; i++) {
      GLuint nr, hw_prim;
      GLuint start = prim[i].start;
      GLuint offset = 0;

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

      /* XXX: Need to ensure that both the state and the primitive
       * command below end up in the same batchbuffer, otherwise there
       * is a risk that another context might interpose a batchbuffer
       * containing different statesetting commands.  Using logical
       * contexts would fix this, as would the BRW scheme of only
       * emitting batch commands while holding the lock.
       */
/*       intelRenderPrimitive() */
      {
	 GLuint dwords = 1 + (nr+1)/2;
	 intel_emit_hardware_state(intel, dwords);

	 /* XXX: Can emit upto 64k indices, need to split larger prims
	  */
	 BEGIN_BATCH( dwords, INTEL_BATCH_CLIPRECTS );
	 OUT_BATCH( _3DPRIMITIVE | 
		    hw_prim | 
		    PRIM_INDIRECT | 
		    PRIM_INDIRECT_ELTS | 
		    nr );
      
	 /* Pack indices into 16bits 
	  */
	 for (j = 0; j < nr-1; j += 2) {
	    OUT_BATCH( (offset + indices[start+j]) | ((offset + indices[start+j+1])<<16) );
	 }

	 if (j < nr)
	    OUT_BATCH( (offset + indices[start+j]) );
	 
	 ADVANCE_BATCH();
      }
   }

   DBG("%s - done\n", __FUNCTION__);
}


void intel_idx_init( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   tnl->Driver.Render.CheckIdxRender       = check_idx_render;
   tnl->Driver.Render.BuildAndEmitVertices = build_and_emit_vertices;
   tnl->Driver.Render.EmitBuiltVertices    = emit_built_vertices;
   tnl->Driver.Render.EmitPrims            = emit_prims;
}

void intel_idx_destroy( struct intel_context *intel )
{
}
