/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

/* Author:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "imports.h"

//#include "intel_prim.h"
#include "intel_draw_quads.h"

#define INTEL_DRAW_PRIVATE
#include "intel_draw.h"



/* Called from swapbuffers:
 */
void intel_draw_finish_frame( struct intel_draw *draw )
{
   assert(!draw->in_vb);
   assert(draw->in_frame);
   draw->in_frame = GL_FALSE;
   draw->render->flush( draw->render, !draw->in_frame );
}


/* Called from glFlush, and other places:
 */
void intel_draw_flush( struct intel_draw *draw )
{
   assert(!draw->in_vb);
   draw->render->flush( draw->render, !draw->in_frame );
}


/* Called when driver state tracker notices changes to the viewport
 * matrix:
 */
void intel_draw_set_viewport( struct intel_draw *draw,
			      const GLfloat *scale,
			      const GLfloat *trans )
{
   assert(!draw->in_vb);
   vf_set_vp_scale_translate( draw->vb.vf, scale, trans );
}


void intel_draw_set_state( struct intel_draw *draw,
			   const struct intel_draw_state *state )
{
   memcpy( &draw->state, state, sizeof(*state) );
   /* Need to validate state.
    */
}


/* Called when driver state tracker notices changes to the hardware
 * vertex format:
 */
void intel_draw_set_hw_vertex_format( struct intel_draw *draw,
				      const struct vf_attr_map *attr,
				      GLuint count,
				      GLuint vertex_size )
{
   /* This is not allowed during the processing of a vertex buffer, ie
    * vertex format must be constant throughout a whole VB and cannot
    * change per-primitive.  This implies that the swrast fallback
    * code must be able to translate the hardware vertices into swrast
    * vertices on the fly.
    */
   assert(!draw->in_vb);

   /* Pass this through to the hardware renderer:  
    * XXX: just the size info???
    */
   draw->hw->set_hw_vertex_format( draw->hw,
				   attr, 
				   count,
				   vertex_size );

   /* Notify the cs pipe and allow it to update its vf instance also:
    */
#if 0
   draw->prim->set_hw_vertex_format( draw->prim,
				     attr,
				     count,
				     vertex_size );
#endif
}


/* Called when the driver state tracker notices a fallback condition
 * or other reason to change the renderer.  Note that per-primitive
 * fallbacks cannot be handled this way - use a multiplexing render
 * module instead.
 */
void intel_draw_set_render( struct intel_draw *draw,
			    struct intel_render *hw )
{
   /* This is not allowed during the processing of a vertex buffer.
    */
   assert( !draw->in_vb );

   /* Shut down the old rasterizerer:
    */
   draw->hw->flush(draw->hw, !draw->in_frame );

   /* Install the new one - potentially updating draw->render as well.
    */
   if (draw->render == draw->hw)
      draw->render = hw;

   draw->hw = hw;

   intel_quads_set_hw_render( draw->quads, draw->hw );
}

static void draw_validate_state( struct intel_draw *draw )
{
   draw->render = draw->hw;
}

static void draw_begin_vb( struct intel_draw *draw, 
			   GLvector4f * const sources[],
			   GLuint count )
{
   draw->in_vb = 1;

   draw->vb.verts = draw->render->allocate_vertices( draw->render,
						     /* draw->vb.vertex_stride_bytes, */
						     count );

   if (!draw->vb.verts) {
      assert(0);
   }

   /* Bind the vb outputs:
    */
   vf_set_sources( draw->vb.vf, sources, 0 );

   /* Build the hardware or prim-pipe vertices: 
    */
   vf_emit_vertices( draw->vb.vf,
		     count,
		     draw->vb.verts );
}




struct intel_draw *intel_draw_create( const struct intel_draw_callbacks *callbacks )
{
   struct intel_draw *draw = CALLOC_STRUCT(intel_draw);

   memcpy( &draw->callbacks, callbacks, sizeof(*callbacks) );

#if 0
   draw->prim = intel_create_prim_render( draw );
#endif

   draw->vb.vf = vf_create( GL_TRUE );

   draw->quads = intel_create_quads_render( draw );

   return draw;
}


void intel_draw_destroy( struct intel_draw *draw )
{
   if (draw->header.storage)
      ALIGN_FREE( draw->header.storage );

#if 0
   draw->prim->destroy( draw->prim );
#endif

   draw->quads->destroy( draw->quads );

   vf_destroy( draw->vb.vf );

   FREE( draw );
}





/***********************************************************************
 * TNL stage to glue the above onto the end of the pipeline.
 */

#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "intel_context.h"

static const GLenum reduced_prim[GL_POLYGON+1] = {
   GL_POINTS,
   GL_LINES,
   GL_LINES,
   GL_LINES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES
};

#if 0
static void 
build_vertex_headers( struct intel_draw *draw,
		      struct vertex_buffer *VB )
{
   if (draw->header.storage == NULL) {
      draw->header.stride = sizeof(GLfloat);
      draw->header.size = 1;
      draw->header.storage = ALIGN_MALLOC( VB->Size * sizeof(GLfloat), 32 );
      draw->header.data = draw->header.storage;
      draw->header.count = 0;
      draw->header.flags = VEC_SIZE_1 | VEC_MALLOC;
   }

   /* Build vertex header attribute.
    * 
    */

   {
      GLuint i;
      GLuint *id = (GLuint *)draw->header.storage;

      draw->header.count = VB->Count;

      if (draw->state.fill_cw != FILL_TRI ||
	  draw->state.fill_ccw != FILL_TRI) {
	 for (i = 0; i < VB->Count; i++) {
	    GLuint flag = VB->EdgeFlag[i] ? (1<<15) : 0;
	    id[i] = VB->ClipMask[i] | flag; 
	 }
      }
      else if (VB->ClipOrMask) {
	 for (i = 0; i < VB->Count; i++)
	    id[i] = VB->ClipMask[i];
      }
      else {
	 for (i = 0; i < VB->Count; i++)
	    id[i] = 0;
      }
   }
   VB->AttribPtr[VF_ATTRIB_VERTEX_HEADER] = &draw->header;
}
#endif



static GLboolean
run_draw(GLcontext *ctx, struct tnl_pipeline_stage *stage)
{
   struct intel_context *intel = intel_context(ctx);
   struct intel_draw *draw = intel->draw;

   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint i, prim_mask;


   /* Build a bitmask of all the primitives in this vb:
    */
   for (prim_mask = i = 0; i < VB->PrimitiveCount; i++)
      prim_mask |= 1 << reduced_prim[VB->Primitive[i].mode];


   /* Tell driver about active primitives, let it update render before
    * starting the vb.
    */
   draw->callbacks.validate_state( draw->callbacks.driver,
				   prim_mask );

   /* Validate our render pipeline: 
    */
   draw_validate_state( draw );


   /* Delay this so that we don't start a frame on a renderer that
    * gets swapped out in the validation above.
    */
   if (!draw->in_frame) {
      draw->render->start_render( draw->render, GL_TRUE );
      draw->in_frame = 1;
   }
   

   /* Always have to do this:
    */
   VB->AttribPtr[VF_ATTRIB_POS] = VB->NdcPtr;
   VB->AttribPtr[VF_ATTRIB_BFC0] = VB->ColorPtr[1];
   VB->AttribPtr[VF_ATTRIB_BFC1] = VB->SecondaryColorPtr[1];
   VB->AttribPtr[VF_ATTRIB_CLIP_POS] = VB->ClipPtr;

   /* Maybe build vertex headers: 
    */
#if 0
   if (draw->render == draw->prim) 
      build_vertex_headers( draw, VB );
#endif

   /* Build the vertices, set in_vb flag:
    */
   draw_begin_vb( draw, VB->AttribPtr, VB->Count );

   for (i = 0; i < VB->PrimitiveCount; i++) {

      GLenum mode = VB->Primitive[i].mode;
      GLuint start = VB->Primitive[i].start;
      GLuint length = VB->Primitive[i].count;

      if (!length)
	 continue;

      if (draw->render_prim != mode) {
	 draw->render_prim = mode;
	 draw->quads->set_prim( draw->quads, mode );
      }

      if (VB->Elts) {
	 draw->quads->draw_indexed_prim( draw->quads, 
					 VB->Elts + start,
					 length );
      }
      else {
	 draw->quads->draw_prim( draw->quads, 
				 start,
				 length );
      }	 
   }

   draw->render->release_vertices( draw->render, draw->vb.verts );
   draw->vb.verts = NULL;
   draw->in_vb = 0;

   return GL_FALSE;             /* finished the pipe */
}




const struct tnl_pipeline_stage _intel_render_stage =
{
   "intel draw",
   NULL,
   NULL,
   NULL,
   NULL,
   run_draw
};
