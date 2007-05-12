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
#include "clip/clip_context.h"

#define ELT_TABLE_SIZE 16

struct mixed_render {
   struct clip_render render;
   struct intel_context *intel;

   GLuint vertex_size;


   struct { 
      GLuint in;
      GLuint out;
   } vert_cache[ELT_TABLE_SIZE];

   struct clip_render *hw;
   struct clip_render *sw;
};

static INLINE struct mixed_render *mixed_render( struct clip_render *render )
{
   return (struct mixed_render *)render;
}

/* Clear the vertex cache:
 */
static void reset_vertex_cache( struct mixed_render *mixed )
{
   GLuint i;
   for (i = 0; i < ELT_TABLE_SIZE; i++)
      copy->vert_cache[i].in = ~0;
}

static void mixed_start_render( struct clip_render *render )
{
   struct mixed_render *mixed = mixed_render( render );
   mixed->active = NULL;
}




/* Really want to allocate vertices from the intel vertex buffer, but
 * mapped read/write rather than write-combined.  That way there is no
 * need to copy the vertices on the classic path, or do anything crazy
 * like a vertex cache on the indexed path to reduce copying.  
 * 
 * The swrast path would have to wait when it remaps the vertices
 * after switching from classic, but it will have to wait to access
 * the screen maps anyway, so that's a non-issue.
 */
static void *mixed_allocate_vertices( struct clip_render *render,
				      GLuint vertex_size,
				      GLuint nr_verts )
{
   /* Always build vertices in a local memory buffer.
    */

   mixed->vertex_size = vertex_size;
}
   

static void mixed_draw_prim( struct clip_render *render,
			     GLuint start,
			     GLuint nr )
{
   /* Emit vertices to active renderer at this point. 
    */

   mixed->active->draw_prim( mixed->active, start, nr );
}

static void mixed_draw_indexed_prim( struct clip_render *render,
				     const GLuint *indices,
				     GLuint nr )
{
   /* Emit vertices to active renderer.  Use a vertex cache to
    * minimize duplication.
    */

   mixed->active->draw_indexed_prim( mixed->active, out, out_nr );
}


static void mixed_set_prim( struct clip_render *render,
			      GLenum mode )
{
   struct mixed_render *mixed = mixed_render( render );
   struct clip_render *active;

   if (mixed->intel->fallback_prims & (1<<mode)) 
      active = mixed->sw;
   else
      active = mixed->hw;

   if (active != mixed->active) {
      if (mixed->active) {
	 mixed->active->flush( mixed->active, GL_FALSE );
      }
      else {
	 active->start_render( active );
      }

      mixed->active = active;      
      reset_vertex_cache( mixed );
   }
  
   mixed->active->set_prim( mixed->active, mode );
}



static void mixed_flush( struct clip_render *render, 
			   GLboolean finished_frame )
{
   if (mixed->active)
      mixed->active->flush( render, finished_frame );

   if (finished_frame) 
      mixed->active = NULL;	/* redundant, see start render. */
}




static void mixed_destroy( struct clip_render *render )
{
   struct mixed_render *mixed = mixed_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(mixed);
}


struct clip_render *intel_create_mixed_render( struct intel_context *intel )
{
   struct mixed_render *mixed = CALLOC_STRUCT(mixed_render);

   mixed->render.limits.max_indices = (SEGMENT_SZ - 1024) / sizeof(GLushort);

   mixed->render.start_render = mixed_start_render;
   mixed->render.allocate_vertices = mixed_allocate_vertices;
   mixed->render.set_prim = mixed_set_prim;
   mixed->render.draw_prim = mixed_draw_prim;
   mixed->render.draw_indexed_prim = mixed_draw_prim_indexed;
   mixed->render.release_vertices = mixed_release_vertices;
   mixed->render.flush = mixed_flush;
   mixed->render.destroy = mixed_destroy;

   mixed->intel = intel;
   mixed->hw = intel->classic;
   mixed->sw = intel->swrast;
   mixed->active = NULL;
   mixed->hw_prim = 0;

   return &mixed->render;
}
