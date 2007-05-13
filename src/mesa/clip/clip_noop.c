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
#include "clip/clip_noop.h"
#include "clip/clip_context.h"

/* This is an optimized version of the clipping pipe when no
 * operations are active.  It does however actually do a couple of
 * things:
 *    - translate away QUADS, QUADSTRIP and LINELOOP prims
 *    - split up excessively long indexed prims.
 */
struct noop_render {
   struct clip_render render;
   struct clip_render *hw;
   GLenum gl_prim;
};

static INLINE struct noop_render *noop_render( struct clip_render *render )
{
   return (struct noop_render *)render;
}



static void *noop_allocate_vertices( struct clip_render *render,
				      GLuint vertex_size,
				      GLuint nr_vertices )
{
   struct noop_render *noop = noop_render( render );
   return noop->hw->allocate_vertices( noop->hw, vertex_size, nr_vertices );
}



static void noop_set_prim( struct clip_render *render,
			      GLenum mode )
{
   struct noop_render *noop = noop_render( render );

//   _mesa_printf("%s: %d\n", __FUNCTION__, mode);

   noop->gl_prim = mode;

   switch (mode) {
   case GL_LINE_LOOP:
      noop->hw->set_prim( noop->hw, GL_LINE_STRIP );
      break;
   case GL_QUADS:
   case GL_QUAD_STRIP:
      noop->hw->set_prim( noop->hw, GL_TRIANGLES );
      break;
   default:
      noop->hw->set_prim( noop->hw, mode );
      break;
   }
}




static void noop_draw_indexed_prim( struct clip_render *render,
				     const GLuint *indices,
				     GLuint length )
{
   struct noop_render *noop = noop_render( render );

   switch (noop->gl_prim) {
   case GL_LINE_LOOP: {
      GLuint tmp_indices[2] = { indices[length],
				indices[0] };

      noop->hw->draw_indexed_prim( noop->hw, 
				    indices, 
				    length );

      noop->hw->draw_indexed_prim( noop->hw,
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

	 noop->hw->draw_indexed_prim( noop->hw, tmp, j );
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

      noop->hw->draw_indexed_prim( noop->hw, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      noop->hw->draw_indexed_prim( noop->hw, 
				   indices, 
				   length );
      break;
   }
}


static GLuint trim( GLuint count, GLuint first, GLuint incr )
{
   return count - (count - first) % incr; 
}


static void noop_split_indexed_prim( struct clip_render *render,
				      const GLuint *indices,
				      GLuint count )
{
   struct noop_render *noop = noop_render( render );

   if (count < noop->hw->limits.max_indices) 
   {
      noop_draw_indexed_prim( render, indices, count );
   }
   else 
   {
      GLuint first, incr;
      GLuint fan_verts = clip_prim_info(noop->gl_prim, &first, &incr);
      GLuint replay = first - incr;
      GLuint max_step = noop->hw->limits.max_indices - (fan_verts + replay);
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

	    noop_draw_indexed_prim( render, tmp, fan_verts + step );

	    free(tmp);
	 }
	 else {
	    noop_draw_indexed_prim( render, indices + start, step );
	 }

	 start += step;

	 /* Do we need to replay some verts?
	  */
	 if (start < count) 
	    start -= replay;
      }
   }
}



static void noop_draw_prim( struct clip_render *render,
			     GLuint start,
			     GLuint length )
{
   struct noop_render *noop = noop_render( render );

//   _mesa_printf("%s (%s) %d/%d\n", __FUNCTION__, 
//		_mesa_lookup_enum_by_nr(noop->gl_prim),
//		start, length );

   switch (noop->gl_prim) {

      /* Lineloop just doesn't work as a concept.  Should get
       * translated away by the vbo module and never disgrace the rest
       * of the driver with its presence.  Am assuming here that we
       * get a full primitive with begin and end vertices, otherwise
       * there will be glitches.  Fix them in the vbo module!!!
       */
   case GL_LINE_LOOP: {
      GLuint indices[2] = { start + length - 1, start };

      noop->hw->draw_prim( noop->hw, start, length );
      noop->hw->draw_indexed_prim( noop->hw, indices, 2 );
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

	 noop->hw->draw_indexed_prim( noop->hw, tmp, j );
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

      noop->hw->draw_indexed_prim( noop->hw, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      noop->hw->draw_prim( noop->hw, start, length );
      break;
   }
}


static void noop_release_vertices( struct clip_render *render, 
				    void *hw_verts)
{
   struct noop_render *noop = noop_render( render );
   noop->hw->release_vertices( noop->hw, hw_verts );
}


static void noop_destroy_context( struct clip_render *render )
{
   struct noop_render *noop = noop_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(noop);
}

struct clip_render *clip_create_noop_render( struct clip_context *draw )
{
   struct noop_render *noop = CALLOC_STRUCT(noop_render);

   noop->render.destroy = noop_destroy_context;
   noop->render.start_render = NULL;
   noop->render.allocate_vertices = noop_allocate_vertices;
   noop->render.set_prim = noop_set_prim;
   noop->render.draw_prim = noop_draw_prim;
   noop->render.draw_indexed_prim = noop_split_indexed_prim;
   noop->render.release_vertices = noop_release_vertices;
   noop->render.flush = NULL;
   noop->gl_prim = GL_POINTS;
   return &noop->render;
}

/* Or, could just peer into the draw struct and update these values on
 * allocate vertices.
 */
void clip_noop_set_hw_render( struct clip_render *render,
				struct clip_render *hw )
{
   struct noop_render *noop = noop_render( render );
   noop->hw = hw;
}
