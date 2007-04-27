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

#include "intel_draw_quads.h"

/* Optimize quadstrip->tristrip where possible.  Needs a hook
 * somewhere to find out if flatshading is active.  Currently disabled
 * as it doesn't seem to speed things up.
 */
#define QUADSTRIP_OPT 0

#if QUADSTRIP_OPT
#define INTEL_DRAW_PRIVATE
#endif

#include "intel_draw.h"


struct quads_render {
   struct intel_render render;
   struct intel_render *hw;
   GLenum hw_prim;
   GLenum gl_prim;

#if QUADSTRIP_OPT
   struct intel_draw *draw;
#endif
};

static INLINE struct quads_render *quads_render( struct intel_render *render )
{
   return (struct quads_render *)render;
}



static void *quads_allocate_vertices( struct intel_render *render,
				      GLuint vertex_size,
				      GLuint nr_vertices )
{
   struct quads_render *quads = quads_render( render );
   return quads->hw->allocate_vertices( quads->hw, vertex_size, nr_vertices );
}


static void quads_set_hw_prim( struct quads_render *quads,
			       GLenum prim )
{
   if (prim != quads->hw_prim) {
      quads->hw_prim = prim;
      quads->hw->set_prim( quads->hw, prim );
   }
}

static void quads_set_prim( struct intel_render *render,
			      GLenum mode )
{
   struct quads_render *quads = quads_render( render );

   quads->gl_prim = mode;

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




static void quads_draw_indexed_prim( struct intel_render *render,
				     const GLuint *indices,
				     GLuint length )
{
   struct quads_render *quads = quads_render( render );

   switch (quads->gl_prim) {
   case GL_LINE_LOOP: {
      GLuint tmp_indices[2] = { indices[length],
				indices[0] };

      quads_set_hw_prim( quads, GL_LINE_STRIP );

      quads->hw->draw_indexed_prim( quads->hw, 
				    indices, 
				    length );

      quads->hw->draw_indexed_prim( quads->hw,
				    tmp_indices, 
				    2 );
      break;
   }


   case GL_QUAD_STRIP:
#if QUADSTRIP_OPT
      if (!quads->draw->state.flatshade) {
	 length -= length % 2;
	 quads_set_hw_prim( quads, GL_TRIANGLE_STRIP );
	 quads->hw->draw_indexed_prim( quads->hw, indices, length );
      }
      else 
#endif
      {
	 GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 2 * 6) );
	 GLuint i, j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = indices[i+0];
	    tmp[j+1] = indices[i+1];
	    tmp[j+2] = indices[i+3];

	    tmp[j+3] = indices[i+2];
	    tmp[j+4] = indices[i+0];
	    tmp[j+5] = indices[i+3];
	 }

	 quads_set_hw_prim( quads, GL_TRIANGLES );
	 quads->hw->draw_indexed_prim( quads->hw, tmp, j );
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
      quads->hw->draw_indexed_prim( quads->hw, tmp, j );
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
   struct quads_render *quads = quads_render( render );

   switch (quads->gl_prim) {

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
#if QUADSTRIP_OPT
      /* Not really any faster:
       */
      if (!quads->draw->state.flatshade) {
	 length -= length % 2;
	 quads_set_hw_prim( quads, GL_TRIANGLE_STRIP );
	 quads->hw->draw_prim( quads->hw, start, length );
      }
      else 
#endif
      {
	 GLuint *tmp = _mesa_malloc( sizeof(GLuint) * (length / 2 * 6) );
	 GLuint i,j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = start+i+0;
	    tmp[j+1] = start+i+1;
	    tmp[j+2] = start+i+3;

	    tmp[j+3] = start+i+2;
	    tmp[j+4] = start+i+0;
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


static void quads_release_vertices( struct intel_render *render, 
				    void *hw_verts)
{
   struct quads_render *quads = quads_render( render );
   quads->hw->release_vertices( quads->hw, hw_verts );
}


static void quads_destroy_context( struct intel_render *render )
{
   struct quads_render *quads = quads_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(quads);
}

struct intel_render *intel_create_quads_render( struct intel_draw *draw )
{
   struct quads_render *quads = CALLOC_STRUCT(quads_render);

   quads->render.destroy = quads_destroy_context;
   quads->render.start_render = NULL;
   quads->render.allocate_vertices = quads_allocate_vertices;
   quads->render.set_prim = quads_set_prim;
   quads->render.draw_prim = quads_draw_prim;
   quads->render.draw_indexed_prim = quads_draw_indexed_prim;
   quads->render.release_vertices = quads_release_vertices;
   quads->render.flush = NULL;

#if QUADSTRIP_OPT
   quads->draw = draw;
#endif

   quads->hw_prim = GL_TRIANGLES;
   quads->gl_prim = GL_TRIANGLES;

   return &quads->render;
}

/* Or, could just peer into the draw struct and update these values on
 * allocate vertices.
 */
void intel_quads_set_hw_render( struct intel_render *render,
				struct intel_render *hw )
{
   struct quads_render *quads = quads_render( render );

   quads->hw = hw;
   quads->hw->set_prim( quads->hw, quads->hw_prim );
}
