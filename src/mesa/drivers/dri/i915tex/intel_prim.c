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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */

#include "intel_context.h"
#include "intel_draw.h"
#include "intel_prim.h"
#include "intel_reg.h"
#include "intel_vb.h"
#include "intel_state.h"



static INLINE struct prim_pipeline *prim_pipeline( struct intel_render *render )
{
   return (struct prim_pipeline *)render;
}



static void pipe_draw_indexed_prim( struct intel_render *render,
				    const GLuint *elts,
				    GLuint count )
{
   struct prim_pipeline *pipe = prim_pipeline( render );
   struct prim_pipeline_stage * const first = pipe->input.first;
   struct intel_vb *vb = pipe->input.vb;
   GLuint i;

   switch (pipe->input.prim) {
   case GL_POINTS:
      for (i = 0; i < count; i ++) {
	 struct vertex_header *v0 = pipe_get_vertex( pipe, elts[i] );
	 first->point( first, v0 );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < count; i += 2) {
	 struct vertex_header *v0 = pipe_get_vertex( vb, elts[i + 0] );
	 struct vertex_header *v1 = pipe_get_vertex( vb, elts[i + 1] );
      
	 first->line( first, v0, v1 );
      }
      break;

   case GL_LINE_STRIP:
      if (count >= 2) {
	 struct vertex_header *v0 = 0;
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, elts[0] );
	 
	 for (i = 1; i < count; i++) {
	    v0 = v1;
	    v1 = intel_vb_get_vertex( vb, elts[i] );
	    
	    first->line( first, v0, v1 );
	 }
      }
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < count; i += 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, elts[i + 0] );
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, elts[i + 1] );
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, elts[i + 2] );
      
	 first->tri( first, v0, v1, v2 );
      }
      break;

   case GL_TRIANGLE_STRIP:
      if (count >= 3) {
	 struct vertex_header *v0 = 0;
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, elts[0] );
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, elts[1] );
	 
	 for (i = 0; i+2 < count; i++) {
	    v0 = v1;
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, elts[i+2] );

	    first->tri( first, v0, v1, v2 );
	 }
      }
      break;

   case GL_TRIANGLE_FAN:
      if (count >= 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, elts[0] );
	 struct vertex_header *v1 = 0;
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, elts[1] );
	 
	 for (i = 0; i+2 < count; i++) {
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, elts[i+2] );
      
	    first->tri( first, v0, v1, v2 );
	 }
      }
      break;

   case GL_POLYGON:
      if (count >= 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, elts[0] );
	 struct vertex_header *v1 = 0;
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, elts[1] );
	 
	 for (i = 0; i+2 < count; i++) {
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, elts[i+2] );
      
	    first->tri( first, v1, v2, v0 );
	 }
      }
      break;

   default:
      assert(0);
      break;
   }
}

static void pipe_draw_prim( struct intel_render *render,
			    GLuint start,
			    GLuint count )
{
   struct prim_pipeline *pipe = prim_pipeline( render );
   struct prim_pipeline_stage * const first = pipe->input.first;
   struct intel_vb *vb = pipe->input.vb;
   GLuint i;

   switch (pipe->input.prim) {
   case GL_POINTS:
      for (i = 0; i < count; i ++) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, start + i );
	 first->point( first, v0 );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < count; i += 2) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, start + i + 0 );
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, start + i + 1 );
      
	 first->line( first, v0, v1 );
      }
      break;

   case GL_LINE_STRIP:
      if (count >= 2) {
	 struct vertex_header *v0 = 0;
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, start + 0 );
	 
	 for (i = 1; i < count; i++) {
	    v0 = v1;
	    v1 = intel_vb_get_vertex( vb, start + i );
	    
	    first->line( first, v0, v1 );
	 }
      }
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < count; i += 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, start + i + 0 );
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, start + i + 1 );
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, start + i + 2 );
      
	 first->tri( first, v0, v1, v2 );
      }
      break;

   case GL_TRIANGLE_STRIP:
      if (count >= 3) {
	 struct vertex_header *v0 = 0;
	 struct vertex_header *v1 = intel_vb_get_vertex( vb, start + 0 );
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, start + 1 );
	 
	 for (i = 0; i+2 < count; i++) {
	    v0 = v1;
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, start + i + 2 );

	    first->tri( first, v0, v1, v2 );
	 }
      }
      break;

   case GL_TRIANGLE_FAN:
      if (count >= 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, start + 0 );
	 struct vertex_header *v1 = 0;
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, start + 1 );
	 
	 for (i = 0; i+2 < count; i++) {
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, start + i + 2 );
      
	    first->tri( first, v0, v1, v2 );
	 }
      }
      break;

   case GL_POLYGON:
      if (count >= 3) {
	 struct vertex_header *v0 = intel_vb_get_vertex( vb, start + 0 );
	 struct vertex_header *v1 = 0;
	 struct vertex_header *v2 = intel_vb_get_vertex( vb, start + 1 );
	 
	 for (i = 0; i+2 < count; i++) {
	    v1 = v2;
	    v2 = intel_vb_get_vertex( vb, start + i + 2 );
      
	    first->tri( first, v1, v2, v0 );
	 }
      }
      break;

   default:
      assert(0);
      break;
   }
}


static void *pipe_get_vertex_space( struct intel_render *render,
				    GLuint nr )
{
   struct prim_pipeline *pipe = prim_pipeline( render );

   
}

static void pipe_set_hw_vertex_format( struct prim_pipeline *pipe,
				       const struct vf_attr *attr,
				       GLuint count )
{
   /* ... */
}




static void pipe_set_hw_render( struct prim_pipeline *pipe,
				struct intel_render *render )
{
   pipe->emit.render = render;
}

static void pipe_set_gl_state( struct prim_pipeline *pipe,
			       const struct prim_gl_state *state )
{
}

static void pipe_set_draw_state( struct prim_pipeline *pipe,
				 const struct prim_draw_state *state )
{
}


static void pipe_validate_state( struct prim_pipeline *pipe )
{
   /* Dependent on driver state and primitive:
    */
   struct prim_pipeline_stage *next = pipe->emit;
   

   {
      /* Vertex header:
       *    index:16
       *    edgeflag:1;
       *    clipmask:12;
       *
       * XXX: this is only needed if we are using the software primitive
       * pipeline (clipping, offset, unfilled, etc).
       */
      EMIT_ATTR(_TNL_ATTRIB_VERTEX_HEADER, EMIT_1F, 0, 4);

      if (prim->draw_state.clipped_prims) {
	 EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_4F, 0, 16);
      }

      if (prim->state.twoside) {
	 if (inputsRead & FRAG_BIT_COL0) {
	    EMIT_ATTR(_TNL_ATTRIB_BFC0, EMIT_4UB_4F_BGRA, 0, 4);
	 }
	    
	 if (inputsRead & FRAG_BIT_COL1) {
	    EMIT_ATTR(_TNL_ATTRIB_BFC1, EMIT_3UB_3F_BGR, 0, 3);
	    EMIT_PAD(1);
	 }
      }

      /* Now append the hardware vertex:
       */


      /* Tell the drawing engine about it: 
       */
      intel_draw_set_cs_vertex_format( pipe->draw,
				       cs_attrs,
				       cs_attr_count );
   }
   

   if (prim->draw_state.active_prims & (1 << FILL_TRI)) 
   {   
      if (prim->state.front_fill != FILL_TRI ||
	  prim->state.back_fill != FILL_TRI) {

	 output_prims &= ~(1<<FILL_TRI);
	 output_prims |= (1 << prim->state.front_fill);
	 output_prims |= (1 << prim->state.back_fill);

	 pipe->unfilled.base.next = next;
	 next = &pipe->unfilled.base;
	 
	 if (prim->state.offset_point ||
	     prim->state.offset_line) {
	    pipe->offset.base.next = next;
	    next = &pipe->offset.base;
	 }
      }

      if (prim->state.light_twoside) {
	 pipe->twoside.base.next = next;
	 next = &pipe->twoside.base;
      }

#if 0
      if (prim->state.front_cull ||
	  prim->state.back_cull) {
	 pipe->cull.base.next = next;
	 next = &pipe->cull.base;
      }
#endif
   }


   if (prim->clipped_prims) {
      pipe->clipper.base.next = next;
      next = &pipe->clipper.base;

      /* 
       */
      if (prim->state.flatshade) {
	 pipe->flatshade.base.next = next;
	 next = &pipe->flatshade.base;
      }
   }

   /* Copy the hardware vertex payload here:
    */
   if (next != &pipe->emit) {
      pipe->first_stage = next;
      intel_draw_cs_active( pipe->draw, GL_TRUE );
   }
   else {
      /* Empty pipeline, nothing for us to do...
       */
      intel_draw_cs_active( pipe->draw, GL_FALSE );
   }
}


static void pipe_start_render( struct intel_render *render )
{
   /* Pass through:
    */
   struct prim_pipeline *pipe = prim_pipeline( render );
   pipe->draw->start_render( pipe->draw );
}


static void pipe_flush( struct intel_render *render,
			    GLboolean finished_frame )
{
   /* Pass through:
    */
   struct prim_pipeline *pipe = prim_pipeline( render );
   pipe->draw->flush( pipe->draw, finished_frame );
}


static void pipe_abandon_frame( struct intel_render *render )
{
   /* Pass through:
    */
   struct prim_pipeline *pipe = prim_pipeline( render );
   pipe->draw->abandon_frame( pipe->draw );
}

static void pipe_destroy_context( struct intel_render *render )
{
   struct prim_pipeline *pipe = prim_pipeline( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(pipe);
}

struct intel_render *intel_create_prim_render( struct intel_draw *draw )
{
   struct prim_pipeline *pipe = CALLOC_STRUCT(prim_pipeline);

   /* XXX: Add casts here to avoid the compiler messages:
    */
   pipe->render.destroy_context = pipe_destroy_context;
   pipe->render.start_render = pipe_start_render;
   pipe->render.flush = pipe_flush;
   pipe->render.abandon_frame = pipe_abandon_frame;
   pipe->render.clear = pipe_clear;
   pipe->render.set_prim = pipe_set_prim;
   pipe->render.new_vertices = pipe_new_vertices;
   pipe->render.draw_prim = pipe_draw_prim;
   pipe->render.draw_indexed_prim = pipe_draw_indexed_prim;

   pipe->draw = draw;
   pipe->prim = 0;

   return &pipe->render;
}
