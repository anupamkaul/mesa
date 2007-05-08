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
#include "macros.h"      
#include "draw/intel_draw_quads.h"
#include "draw/intel_draw.h"


struct quads_render {
   struct intel_render render;
   struct intel_render *hw;
   GLenum gl_prim;
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



static void quads_set_prim( struct intel_render *render,
			      GLenum mode )
{
   struct quads_render *quads = quads_render( render );

//   _mesa_printf("%s: %d\n", __FUNCTION__, mode);

   quads->gl_prim = mode;

   switch (mode) {
   case GL_LINE_LOOP:
      quads->hw->set_prim( quads->hw, GL_LINE_STRIP );
      break;
   case GL_QUADS:
   case GL_QUAD_STRIP:
      quads->hw->set_prim( quads->hw, GL_TRIANGLES );
      break;
   default:
      quads->hw->set_prim( quads->hw, mode );
      break;
   }
}


static GLboolean split_prim_inplace(GLenum mode, GLuint *first, GLuint *incr)
{
   switch (mode) {
   case GL_POINTS:
      *first = 1;
      *incr = 1;
      return 0;
   case GL_LINES:
      *first = 2;
      *incr = 2;
      return 0;
   case GL_LINE_STRIP:
      *first = 2;
      *incr = 1;
      return 0;
   case GL_LINE_LOOP:
      *first = 2;
      *incr = 1;
      return 1;
   case GL_TRIANGLES:
      *first = 3;
      *incr = 3;
      return 0;
   case GL_TRIANGLE_STRIP:
      *first = 3;
      *incr = 1;
      return 0;
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      *first = 3;
      *incr = 1;
      return 1;
   case GL_QUADS:
      *first = 4;
      *incr = 4;
      return 0;
   case GL_QUAD_STRIP:
      *first = 4;
      *incr = 2;
      return 0;
   default:
      assert(0);
      *first = 1;
      *incr = 1;
      return 0;
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

      quads->hw->draw_indexed_prim( quads->hw, 
				    indices, 
				    length );

      quads->hw->draw_indexed_prim( quads->hw,
				    tmp_indices, 
				    2 );
      break;
   }


   case GL_QUAD_STRIP:
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

static GLuint trim( GLuint count, GLuint first, GLuint incr )
{
   return count - (count - first) % incr; 
}



static void quads_split_indexed_prim( struct intel_render *render,
				      const GLuint *indices,
				      GLuint count )
{
   struct quads_render *quads = quads_render( render );
   GLuint first, incr;
   GLuint fan_verts;

   fan_verts = split_prim_inplace(quads->gl_prim, &first, &incr);
   count = trim( count, first, incr );

   if (count < quads->hw->limits.max_indices) 
   {
      quads_draw_indexed_prim( render, indices, count );
   }
   else 
   {
      GLuint replay = first - incr;
      GLuint max_step = quads->hw->limits.max_indices - (fan_verts + replay);
      GLuint start;

      for (start = 0 ; start < count ; ) {
	 GLuint remaining = count - start;
	 GLuint step = trim( MIN2( max_step, remaining ), first, incr );
	 
/* 	 outprim->begin = (j == 0 && prim->begin); */
/* 	 outprim->end = (step == remaining && prim->end); */

	 if (start && fan_verts) { 
	    GLuint *tmp = malloc( (fan_verts + step) * sizeof(GLuint) );
	    GLuint i;

	    for (i = 0; i < fan_verts; i++)
	       tmp[i] = indices[i];

	    for (i = 0 ; i < step ; i++)
	       tmp[i+fan_verts] = indices[start+i];

	    quads_draw_indexed_prim( render, tmp, fan_verts + step );

	    free(tmp);
	 }
	 else {
	    quads_draw_indexed_prim( render, indices + start, step );
	 }

	 start += step;

	 /* Do we need to replay some verts?
	  */
	 if (start < count) 
	    start -= replay;
      }
   }
}



static void quads_draw_prim( struct intel_render *render,
			     GLuint start,
			     GLuint length )
{
   struct quads_render *quads = quads_render( render );

//   _mesa_printf("%s (%s) %d/%d\n", __FUNCTION__, 
//		_mesa_lookup_enum_by_nr(quads->gl_prim),
//		start, length );

   switch (quads->gl_prim) {

      /* Lineloop just doesn't work as a concept.  Should get
       * translated away by the vbo module and never disgrace the rest
       * of the driver with its presence.  Am assuming here that we
       * get a full primitive with begin and end vertices, otherwise
       * there will be glitches.  Fix them in the vbo module!!!
       */
   case GL_LINE_LOOP: {
      GLuint indices[2] = { start + length - 1, start };

      quads->hw->draw_prim( quads->hw, start, length );
      quads->hw->draw_indexed_prim( quads->hw, indices, 2 );
      break;
   }


   case GL_QUAD_STRIP:
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
   quads->render.draw_indexed_prim = quads_split_indexed_prim;
   quads->render.release_vertices = quads_release_vertices;
   quads->render.flush = NULL;
   quads->gl_prim = GL_POINTS;
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
}
