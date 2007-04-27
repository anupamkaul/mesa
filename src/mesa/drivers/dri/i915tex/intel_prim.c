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

#include "imports.h"

#define INTEL_DRAW_PRIVATE
#include "intel_draw.h"

#define INTEL_PRIM_PRIVATE
#include "intel_prim.h"


static INLINE struct prim_pipeline *prim_pipeline( struct intel_render *render )
{
   return (struct prim_pipeline *)render;
}


static struct vertex_header *get_vertex( struct prim_pipeline *pipe,
					       GLuint i )
{
   return (struct vertex_header *)(pipe->verts + i * pipe->vertex_size);
}



static void *pipe_allocate_vertices( struct intel_render *render,
				     GLuint vertex_size,
				     GLuint nr_vertices )
{
   struct prim_pipeline *pipe = prim_pipeline( render );

   pipe->vertex_size = vertex_size;
   pipe->nr_vertices = nr_vertices;
   pipe->verts = MALLOC( nr_vertices * pipe->vertex_size );

   assert(pipe->need_validate == 0);

   pipe->first->begin( pipe->first );

   return pipe->verts;
}

static void pipe_set_prim( struct intel_render *render,
			   GLenum prim )
{
   struct prim_pipeline *pipe = prim_pipeline( render );

   pipe->prim = prim;
}
			  

static const GLuint pipe_prim[GL_POLYGON+1] = {
   PRIM_POINT,
   PRIM_LINE,
   PRIM_LINE,
   PRIM_LINE,
   PRIM_TRI,
   PRIM_TRI,
   PRIM_TRI,
   PRIM_TRI,
   PRIM_TRI,
   PRIM_TRI
};


static void do_tri( struct prim_stage *first,
		    struct vertex_header *v0,
		    struct vertex_header *v1,
		    struct vertex_header *v2 )
{
   struct prim_header prim;
   prim.det = 0;
   prim.v[0] = v0;
   prim.v[1] = v1;
   prim.v[2] = v2;
   first->tri( first, &prim );
}


static void do_quad( struct prim_stage *first,
		     struct vertex_header *v0,
		     struct vertex_header *v1,
		     struct vertex_header *v2,
		     struct vertex_header *v3 )
{

   {
      GLubyte tmp = v1->edgeflag;
      v1->edgeflag = 0;
      do_tri( first, v0, v1, v3 );
      v1->edgeflag = tmp;
   }

   {
      GLubyte tmp = v3->edgeflag;
      v3->edgeflag = 0;
      do_tri( first, v1, v2, v3 );
      v3->edgeflag = tmp;
   }
}






static void pipe_draw_indexed_prim( struct intel_render *render,
				    const GLuint *elts,
				    GLuint count )
{
   struct prim_pipeline *pipe = prim_pipeline( render );
   struct prim_stage * const first = pipe->first;
   struct prim_header prim;
   GLuint i;

   prim.det = 0;		/* valid from cull stage onwards */
   prim.v[0] = 0;
   prim.v[1] = 0;
   prim.v[2] = 0;

   switch (pipe->prim) {
   case GL_POINTS:
      for (i = 0; i < count; i ++) {
	 prim.v[0] = get_vertex( pipe, elts[i] );

	 first->point( first, &prim );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < count; i += 2) {
	 prim.v[0] = get_vertex( pipe, elts[i + 0] );
	 prim.v[1] = get_vertex( pipe, elts[i + 1] );
      
	 first->line( first, &prim );
      }
      break;

   case GL_LINE_LOOP:    /* Just punt on loops for now. */
   case GL_LINE_STRIP:
      /* I'm guessing it will be necessary to have something like a
       * render->reset_line_stipple() method to properly support
       * splitting strips into primitives like this.  Alternately we
       * could just scan ahead to find individual clipped lines and
       * otherwise leave the strip intact - that might be better, but
       * require more complex code here.
       */
      if (count >= 2) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, elts[0] );
	 
	 for (i = 1; i < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = get_vertex( pipe, elts[i] );
	    
	    first->line( first, &prim );
	 }
      }
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < count; i += 3) {
	 prim.v[0] = get_vertex( pipe, elts[i + 0] );
	 prim.v[1] = get_vertex( pipe, elts[i + 1] );
	 prim.v[2] = get_vertex( pipe, elts[i + 2] );
      
	 first->tri( first, &prim );
      }
      break;

   case GL_TRIANGLE_STRIP:
      if (count >= 3) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, elts[0] );
	 prim.v[2] = get_vertex( pipe, elts[1] );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = prim.v[2];
	    prim.v[2] = get_vertex( pipe, elts[i+2] );

	    first->tri( first, &prim );
	 }
      }
      break;

   case GL_TRIANGLE_FAN:
      if (count >= 3) {
	 prim.v[0] = get_vertex( pipe, elts[0] );
	 prim.v[1] = 0;
	 prim.v[2] = get_vertex( pipe, elts[1] );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[1] = prim.v[2];
	    prim.v[2] = get_vertex( pipe, elts[i+2] );
      
	    first->tri( first, &prim );
	 }
      }
      break;

   case GL_QUADS:
      for (i = 0; i+3 < count; i += 4) {
	 do_quad( first,
		  get_vertex( pipe, elts[i + 0] ),
		  get_vertex( pipe, elts[i + 1] ),
		  get_vertex( pipe, elts[i + 2] ),
		  get_vertex( pipe, elts[i + 3] ));
      }
      break;

   case GL_QUAD_STRIP:
      for (i = 0; i+3 < count; i += 2) {
	 do_quad( first,
		  get_vertex( pipe, elts[i + 0] ),
		  get_vertex( pipe, elts[i + 1] ),
		  get_vertex( pipe, elts[i + 3] ),
		  get_vertex( pipe, elts[i + 2] ));
      }
      break;


   case GL_POLYGON:
      if (count >= 3) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, elts[1] );
	 prim.v[2] = get_vertex( pipe, elts[0] );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = get_vertex( pipe, elts[i+2] );
      
	    first->tri( first, &prim );
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
   struct prim_stage * const first = pipe->first;
   struct prim_header prim;
   GLuint i;

   prim.det = 0;		/* valid from cull stage onwards */
   prim.v[0] = 0;
   prim.v[1] = 0;
   prim.v[2] = 0;

   switch (pipe->prim) {
   case GL_POINTS:
      for (i = 0; i < count; i ++) {
	 prim.v[0] = get_vertex( pipe, start + i );
	 first->point( first, &prim );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < count; i += 2) {
	 prim.v[0] = get_vertex( pipe, start + i + 0 );
	 prim.v[1] = get_vertex( pipe, start + i + 1 );
      
	 first->line( first, &prim );
      }
      break;

   case GL_LINE_STRIP:
      if (count >= 2) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, start + 0 );
	 
	 for (i = 1; i < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = get_vertex( pipe, start + i );
	    
	    first->line( first, &prim );
	 }
      }
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < count; i += 3) {
	 prim.v[0] = get_vertex( pipe, start + i + 0 );
	 prim.v[1] = get_vertex( pipe, start + i + 1 );
	 prim.v[2] = get_vertex( pipe, start + i + 2 );
      
	 first->tri( first, &prim );
      }
      break;

   case GL_TRIANGLE_STRIP:
      if (count >= 3) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, start + 0 );
	 prim.v[2] = get_vertex( pipe, start + 1 );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = prim.v[2];
	    prim.v[2] = get_vertex( pipe, start + i + 2 );

	    first->tri( first, &prim );
	 }
      }
      break;

   case GL_TRIANGLE_FAN:
      if (count >= 3) {
	 prim.v[0] = get_vertex( pipe, start + 0 );
	 prim.v[1] = 0;
	 prim.v[2] = get_vertex( pipe, start + 1 );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[1] = prim.v[2];
	    prim.v[2] = get_vertex( pipe, start + i + 2 );
      
	    first->tri( first, &prim );
	 }
      }
      break;


   case GL_QUADS:
      for (i = 0; i+3 < count; i += 4) {
	 do_quad( first,
		  get_vertex( pipe, start + i + 0 ),
		  get_vertex( pipe, start + i + 1 ),
		  get_vertex( pipe, start + i + 2 ),
		  get_vertex( pipe, start + i + 3 ));
      }
      break;

   case GL_QUAD_STRIP:
      for (i = 0; i+3 < count; i += 2) {
	 do_quad( first,
		  get_vertex( pipe, start + i + 0 ),
		  get_vertex( pipe, start + i + 1 ),
		  get_vertex( pipe, start + i + 3 ),
		  get_vertex( pipe, start + i + 2 ));
      }
      break;

   case GL_POLYGON:
      if (count >= 3) {
	 prim.v[0] = 0;
	 prim.v[1] = get_vertex( pipe, start + 1 );
	 prim.v[2] = get_vertex( pipe, start + 0 );
	 
	 for (i = 0; i+2 < count; i++) {
	    prim.v[0] = prim.v[1];
	    prim.v[1] = get_vertex( pipe, start + i + 2 );
      
	    first->tri( first, &prim );
	 }
      }
      break;

   default:
      assert(0);
      break;
   }
}


static void pipe_release_vertices( struct intel_render *render,
				   void *vertices )
{
   struct prim_pipeline *pipe = prim_pipeline( render );

   pipe->first->end( pipe->first );

   FREE(pipe->verts);
   pipe->verts = NULL;
}

static void pipe_destroy( struct intel_render *render )
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
   pipe->render.start_render = NULL;
   pipe->render.allocate_vertices = pipe_allocate_vertices;
   pipe->render.set_prim = pipe_set_prim;
   pipe->render.draw_prim = pipe_draw_prim;
   pipe->render.draw_indexed_prim = pipe_draw_indexed_prim;
   pipe->render.release_vertices = pipe_release_vertices;
   pipe->render.flush = NULL;
   pipe->render.destroy = pipe_destroy;

   pipe->draw = draw;
   pipe->prim = 0;

   pipe->emit = intel_prim_emit( pipe );
#if 0
   pipe->unfilled = intel_prim_unfilled( pipe );
   pipe->twoside = intel_prim_twoside( pipe );
   pipe->clip = intel_prim_clip( pipe );
   pipe->flatshade = intel_prim_flatshade( pipe );
#endif
   pipe->cull = intel_prim_cull( pipe );

   return &pipe->render;
}



GLboolean intel_prim_validate_state( struct intel_render *render )
{
   struct prim_pipeline *pipe = prim_pipeline( render );
   
   /* Dependent on driver state and primitive:
    */
   struct prim_stage *next = pipe->emit;

   pipe->need_validate = 0;
   

#if 0
   if (pipe->draw->vb_state.active_prims & (1 << GL_TRIANGLES)) 
   {   
      if (pipe->draw->state.fill_cw != FILL_TRI ||
	  pipe->draw->state.fill_ccw != FILL_TRI) {

	 output_prims &= ~(1<<FILL_TRI);
	 output_prims |= (1 << pipe->draw->state.front_fill);
	 output_prims |= (1 << pipe->draw->state.back_fill);

	 pipe->unfilled.base.next = next;
	 next = &pipe->unfilled.base;
	 
	 if (pipe->draw->state.offset_point ||
	     pipe->draw->state.offset_line) {
	    pipe->offset.base.next = next;
	    next = &pipe->offset.base;
	 }
      }

      if (pipe->draw->state.light_twoside) {
	 pipe->twoside.base.next = next;
	 next = &pipe->twoside.base;
      }

#if 0
      if (pipe->draw->state.front_cull ||
	  pipe->draw->state.back_cull) {
	 pipe->cull.base.next = next;
	 next = &pipe->cull.base;
      }
#endif
   }
#endif


   {
      pipe->cull->next = next;
      next = pipe->cull;
   }


#if 0
   if (pipe->draw->vb_state.clipped_prims) {
      pipe->clipper.base.next = next;
      next = &pipe->clipper.base;

      /* 
       */
      if (pipe->draw->state.flatshade) {
	 pipe->flatshade.base.next = next;
	 next = &pipe->flatshade.base;
      }
   }
#endif

   /* Copy the hardware vertex payload here:
    */
   if (1 || next != pipe->emit) {
      pipe->first = next;
      return GL_TRUE;
   }

   
   /* Empty pipeline, nothing for us to do...
    */
   return GL_FALSE;
}


void intel_prim_set_hw_render( struct intel_render *render,
			       struct intel_render *hw )
{
   struct prim_pipeline *pipe = prim_pipeline( render ); 
   pipe->need_validate = 1;
}


void intel_prim_set_draw_state( struct intel_render *render,
				struct intel_draw_state *state )
{
   struct prim_pipeline *pipe = prim_pipeline( render ); 
   pipe->need_validate = 1;
}

void intel_prim_set_vb_state( struct intel_render *render,
			      struct intel_draw_vb_state *state )
{
   struct prim_pipeline *pipe = prim_pipeline( render ); 
   pipe->need_validate = 1;
}


void intel_prim_clear_vertex_indices( struct prim_pipeline *prim )
{
   GLuint i;

   for (i = 0; i < prim->nr_vertices; i++) {
      struct vertex_header *v0 = get_vertex( prim, i );
      v0->index = ~0;
   }
}
