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
#include "intel_vb.h"
#include "intel_batchbuffer.h"
#include "intel_reg.h"
#include "intel_buffers.h"
#include "intel_state.h"
#include "intel_draw.h"


struct quads_render {
   struct intel_render render;
   struct intel_render hw;
   GLenum hw_prim;
};

static INLINE struct quads_render *quads_render( struct intel_render *render )
{
   return (struct quads_render *)render;
}


static void *quads_new_vertices( struct intel_render *render )
{
   return quads->hw->new_vertices( quads->hw );
}

static void quads_set_hw_prim( struct quads_render *quads,
			       GLenum prim )
{
   if (prim != quads->hw_prim) {
      quads->hw_prim = prim;
      quads->hw->set_prim( quads->hw, prim );
   }
}

static void quads_draw_indexed_prim( struct intel_render *render )
{
   switch (mode) {
   case GL_LINE_LOOP: {
      GLuint tmp_indices[2] = { indices[start + length],
				indices[start] };

      quads_set_hw_prim( quads, GL_LINE_STRIP );

      quads->hw->draw_indexed_prim( quads->hw, 
				    indices + start, 
				    length );

      quads->hw->draw_indexed_prim( quads->hw,
				    tmp_indices, 
				    2 );
      break;
   }


   case GL_QUAD_STRIP:
      if (intel->state.Light->ShadeModel != GL_FLAT) {
	 length -= length % 2;
	 quads_set_hw_prim( quads, GL_TRIANGLE_STRIP );
	 quads->hw->draw_prim( quads->hw, start, length );
      }
      else {
	 GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 2 * 6) );
	 GLuint i, j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = indices[i+0];
	    tmp[j+1] = indices[i+1]; /* this is wrong! */
	    tmp[j+2] = indices[i+3];

	    tmp[j+3] = indices[i+1];
	    tmp[j+4] = indices[i+2]; /* this is wrong! */
	    tmp[j+5] = indices[i+3];
	 }

	 quads_set_hw_prim( quads, GL_TRIANGLES );
	 quads->draw_indexed_prim( quads, tmp, j );
	 _mesa_free(tmp);
      }
      break;

   case GL_QUADS: {
      GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 4 * 6) );
      GLuint i, j;

      for (j = i = 0; i + 3 < length; i += 4, j += 6) {
	 tmp[j+0] = indices[i+0];
	 tmp[j+1] = indices[i+1];
	 tmp[j+2] = indices[i+3];

	 tmp[j+3] = indices[i+1];
	 tmp[j+4] = indices[i+2];
	 tmp[j+5] = indices[i+3];
      }

      quads_set_hw_prim( quads, GL_TRIANGLES );
      quads->draw_indexed_prim( quads, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      quads->hw->draw_indexed_prim( quads->hw, 
				   indices, 
				   length );
      break;
   }
}



static void quads_draw_prim( struct intel_render *render,
			     GLuint start,
			     GLuint length )
{
   switch (mode) {

      /* Lineloop just doesn't work as a concept.  Should get
       * translated away by the vbo module and never disgrace the rest
       * of the driver with its presence.  Am assuming here that we
       * get a full primitive with begin and end vertices, otherwise
       * there will be glitches.  Fix them in the vbo module!!!
       */
   case GL_LINE_LOOP: {
      GLuint indices[2] = { start + length - 1, start };

      quads_set_hw_prim( quads, GL_LINE_STRIP );
      quads->hw->draw_prim( quads->hw, start, length );
      quads->hw->draw_indexed_prim( quads->hw, indices, 2 );
      break;
   }


   case GL_QUAD_STRIP:
      if (!quads->draw->state.flatshade) {
	 length -= length % 2;
	 quads_set_hw_prim( quads, GL_TRIANGLE_STRIP );
	 quads->hw->draw_prim( quads->hw, start, length );
      }
      else {
	 GLuint *tmp = _mesa_malloc( sizeof(GLuint) * (length / 2 * 6) );
	 GLuint i,j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = start+i+0;
	    tmp[j+1] = start+i+1; /* this is wrong! */
	    tmp[j+2] = start+i+3;

	    tmp[j+3] = start+i+1;
	    tmp[j+4] = start+i+2; /* this is wrong! */
	    tmp[j+5] = start+i+3;
	 }

	 quads_set_hw_prim( quads, GL_TRIANGLES );
	 quads->hw->draw_indexed_prim( quads->hw, tmp, j );
	 _mesa_free(tmp);
      }
      break;

   case GL_QUADS: {
      GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 4 * 6) );
      GLuint i,j;

      for (j = i = 0; i + 3 < length; i += 4, j += 6) {
	 tmp[j+0] = start+i+0;
	 tmp[j+1] = start+i+1;
	 tmp[j+2] = start+i+3;

	 tmp[j+3] = start+i+1;
	 tmp[j+4] = start+i+2;
	 tmp[j+5] = start+i+3;
      }

      quads_set_hw_prim( quads, GL_TRIANGLES );
      quads->hw->draw_indexed_prim( quads->hw, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      quads->hw->draw_prim( quads->hw, start, length );
      break;
   }
}


static void quads_set_prim( struct intel_render *render,
			      GLenum mode )
{
   struct quads_render *quads = quads_render( render );

   switch (mode) {
   case GL_LINE_LOOP:
   case GL_QUADS:
   case GL_QUAD_STRIP:
      break;
   default:
      quads_set_hw_prim( quads, mode );
      break;
   }
}




static void quads_destroy_context( struct intel_render *render )
{
   struct quads_render *quads = quads_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(quads);
}

struct intel_render *intel_create_quads_render( struct intel_context *intel )
{
   struct quads_render *quads = CALLOC_STRUCT(quads_render);

   /* XXX: Add casts here to avoid the compiler messages:
    */
   quads->render.destroy_context = quads_destroy_context;
   quads->render.start_render = 0;
   quads->render.abandon_frame = 0;
   quads->render.flush = 0;
   quads->render.clear = 0;
   quads->render.new_vertices = 0;
   quads->render.set_prim = quads_set_prim;
   quads->render.draw_prim = quads_draw_prim;
   quads->render.draw_indexed_prim = quads_draw_prim_indexed;

   quads->intel = intel;
   quads->hw_prim = ~0;

   return &quads->render;
}


void intel_quads_set_hw_render( struct intel_render *quads_render,
				struct intel_render *hw )
{
   struct quads_render *quads = quads_render( render );

   quads->hw = hw;
   quads->render.start_render = hw->start_render;
   quads->render.abandon_frame = hw->abandon_frame;
   quads->render.flush = hw->flush;
   quads->render.clear = hw->clear;
   quads->render.new_vertices = hw->new_vertices;

   quads->hw->set_prim( quads->hw, quads->hw_prim );
}
