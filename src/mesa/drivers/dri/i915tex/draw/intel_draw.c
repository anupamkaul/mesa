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
#include "macros.h"

#include "tnl/t_context.h"
#include "vf/vf.h"

#define INTEL_DRAW_PRIVATE
#include "draw/intel_draw.h"

#include "draw/intel_prim.h"
#include "draw/intel_draw_quads.h"


/* Called from swapbuffers:
 */
void intel_draw_finish_frame( struct intel_draw *draw )
{
   assert(!draw->in_vb);
//   assert(draw->in_frame);
   draw->in_frame = GL_FALSE;
}


/* Called from glFlush, and other places:
 */
void intel_draw_flush( struct intel_draw *draw )
{
   assert(!draw->in_vb);

   if (draw->hw)
      draw->hw->flush( draw->hw, !draw->in_frame );
}


/* Called when driver state tracker notices changes to the viewport
 * matrix:
 */
void intel_draw_set_viewport( struct intel_draw *draw,
			      const GLfloat *scale,
			      const GLfloat *trans )
{
   assert(!draw->in_vb);
   vf_set_vp_scale_translate( draw->hw_vf, scale, trans );
   vf_set_vp_scale_translate( draw->prim_vf, scale, trans );
}


void intel_draw_set_state( struct intel_draw *draw,
			   const struct intel_draw_state *state )
{
   memcpy( &draw->state, state, sizeof(*state) );

   /* Need to validate state.
    */
   draw->revalidate = 1;
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

   
   memcpy(draw->hw_attrs, attr, count * sizeof(*attr));
   draw->hw_attr_count = count;
   draw->hw_vertex_size = vertex_size;

   vf_set_vertex_attributes( draw->hw_vf,
			     draw->hw_attrs,
			     draw->hw_attr_count,
			     draw->hw_vertex_size );


   draw->revalidate = 1;
}


void intel_draw_set_prim_vertex_format( struct intel_draw *draw,
					const struct vf_attr_map *attr,
					GLuint count,
					GLuint vertex_size )
{
   assert(!draw->in_vb);
   
   memcpy(draw->prim_attrs, attr, count * sizeof(*attr));
   draw->prim_attr_count = count;
   draw->prim_vertex_size = vertex_size;

   vf_set_vertex_attributes( draw->prim_vf,
			     draw->prim_attrs,
			     draw->prim_attr_count,
			     draw->prim_vertex_size );


   draw->revalidate = 1;
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
   if (draw->hw)
      draw->hw->flush(draw->hw, !draw->in_frame );

   /* Install the new one.
    */
   draw->hw = hw;
   draw->revalidate = 1;
}

void intel_draw_set_userclip( struct intel_draw *draw,
			      const GLfloat (*ucp)[4],
			      GLuint nr )
{
   memcpy(&draw->plane[6], ucp, nr * sizeof(ucp[0]));
   draw->nr_planes = 6 + nr;
}


static void draw_validate_state( struct intel_draw *draw )
{
   /* Choose between simple and complex (quads vs. prim) pipelines.
    */
   draw->prim_pipe_active = intel_prim_validate_state( draw->prim );

   if (draw->prim_pipe_active) {
      intel_prim_set_hw_render( draw->prim, draw->hw );

      draw->vb.render = draw->prim;
      draw->vb.attrs = draw->prim_attrs;
      draw->vb.attr_count = draw->prim_attr_count;
      draw->vb.vertex_size = draw->prim_vertex_size;
      draw->vb.vf = draw->prim_vf;
   }
   else {
      intel_quads_set_hw_render( draw->quads, draw->hw );

      draw->vb.render = draw->quads;
      draw->vb.attrs = draw->hw_attrs;
      draw->vb.attr_count = draw->hw_attr_count;
      draw->vb.vertex_size = draw->hw_vertex_size;
      draw->vb.vf = draw->hw_vf;
   }

   draw->vb.render->set_prim( draw->vb.render, 
			      draw->vb.render_prim );

   draw->revalidate = 0;
}


struct vertex_fetch *intel_draw_get_hw_vf( struct intel_draw *draw )
{
   return draw->hw_vf;
}


struct intel_draw *intel_draw_create( const struct intel_draw_callbacks *callbacks )
{
   struct intel_draw *draw = CALLOC_STRUCT(intel_draw);

   memcpy( &draw->callbacks, callbacks, sizeof(*callbacks) );

   draw->prim = intel_create_prim_render( draw );
   draw->quads = intel_create_quads_render( draw );

   draw->hw_vf = vf_create( GL_TRUE );
   draw->prim_vf = vf_create( GL_TRUE );

   ASSIGN_4V( draw->plane[0], -1,  0,  0, 1 );
   ASSIGN_4V( draw->plane[1],  1,  0,  0, 1 );
   ASSIGN_4V( draw->plane[2],  0, -1,  0, 1 );
   ASSIGN_4V( draw->plane[3],  0,  1,  0, 1 );
   ASSIGN_4V( draw->plane[4],  0,  0, -1, 1 );
   ASSIGN_4V( draw->plane[5],  0,  0,  1, 1 );
   draw->nr_planes = 6;

   return draw;
}


void intel_draw_destroy( struct intel_draw *draw )
{
   if (draw->header.storage)
      ALIGN_FREE( draw->header.storage );

   draw->prim->destroy( draw->prim );
   draw->quads->destroy( draw->quads );

   vf_destroy( draw->hw_vf );
   vf_destroy( draw->prim_vf );

   FREE( draw );
}







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
      struct vertex_header *header = (struct vertex_header *)draw->header.storage;

      /* yes its a hack
       */
      assert(sizeof(*header) == sizeof(GLfloat));

      draw->header.count = VB->Count;

      if (draw->state.fill_cw != FILL_TRI ||
	  draw->state.fill_ccw != FILL_TRI) {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = VB->ClipMask[i];
	    header[i].edgeflag = VB->EdgeFlag[i]; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
      else if (VB->ClipOrMask) {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = VB->ClipMask[i];
	    header[i].edgeflag = 0; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
      else {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = 0;
	    header[i].edgeflag = 0; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
   }

   VB->AttribPtr[VF_ATTRIB_VERTEX_HEADER] = &draw->header;
}

static void 
update_vb_state( struct intel_draw *draw,
		 struct vertex_buffer *VB )
{
   struct intel_draw_vb_state vb_state;
   GLuint i;

   vb_state.clipped_prims = (VB->ClipOrMask != 0);
   vb_state.pad = 0;
   vb_state.active_prims = 0;

   /* Build a bitmask of the reduced primitives in this vb:
    */
   for (i = 0; i < VB->PrimitiveCount; i++)
      vb_state.active_prims |= 1 << reduced_prim[VB->Primitive[i].mode];

   if (memcmp(&vb_state, &draw->vb_state, sizeof(vb_state)) != 0) {
      
      memcpy(&draw->vb_state, &vb_state, sizeof(vb_state));


      /* Tell driver about active primitives, let it update render before
       * starting the vb.
       */
      draw->callbacks.set_vb_state( draw->callbacks.driver,
				    &vb_state );

      intel_prim_set_vb_state( draw->prim, &vb_state );
   }
}




void intel_draw_vb(struct intel_draw *draw,
		   struct vertex_buffer *VB )
{
   GLuint i;

   update_vb_state( draw, VB );

   draw->callbacks.validate_state( draw->callbacks.driver );
   
   /* Validate our render pipeline: 
    */
   if (draw->revalidate)
      draw_validate_state( draw );


   /* Delay this so that we don't start a frame on a renderer that
    * gets swapped out in the validation above.
    */
   if (!draw->in_frame) {
/*       draw->hw->start_render( draw->hw, GL_TRUE ); */
      draw->in_frame = 1;
   }
   
   /* Maybe build vertex headers: 
    */
   if (draw->prim_pipe_active) {
      VB->AttribPtr[VF_ATTRIB_BFC0] = VB->ColorPtr[1];
      VB->AttribPtr[VF_ATTRIB_BFC1] = VB->SecondaryColorPtr[1];
      VB->AttribPtr[VF_ATTRIB_CLIP_POS] = VB->ClipPtr;

      build_vertex_headers( draw, VB );
   }

   draw->in_vb = 1;

   /* Allocate the vertices:
    */
   draw->vb.verts = draw->vb.render->allocate_vertices( draw->vb.render,
							draw->vb.vertex_size,
							VB->Count );

   /* Bind the vb outputs:
    */
   vf_set_sources( draw->vb.vf, VB->AttribPtr, 0 );

   /* Build the hardware or prim-pipe vertices: 
    */
   vf_emit_vertices( draw->vb.vf,
		     VB->Count,
		     draw->vb.verts );


   for (i = 0; i < VB->PrimitiveCount; i++) {

      GLenum mode = VB->Primitive[i].mode;
      GLuint start = VB->Primitive[i].start;
      GLuint length = VB->Primitive[i].count;

      if (!length)
	 continue;

      if (draw->vb.render_prim != mode) {
	 draw->vb.render_prim = mode;
	 draw->vb.render->set_prim( draw->vb.render, mode );
      }

      if (VB->Elts) {
	 draw->vb.render->draw_indexed_prim( draw->vb.render, 
					     VB->Elts + start,
					     length );
      }
      else {
	 draw->vb.render->draw_prim( draw->vb.render, 
				     start,
				     length );
      }	 
   }

   draw->vb.render->release_vertices( draw->vb.render, draw->vb.verts );
   draw->vb.verts = NULL;
   draw->in_vb = 0;
}


