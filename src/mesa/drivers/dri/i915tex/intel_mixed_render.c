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


struct mixed_render {
   struct intel_render render;
   struct intel_context *intel;

   GLuint sw_prims;

   struct intel_render *hw;
   struct intel_render *sw;
};

static INLINE struct mixed_render *mixed_render( struct intel_render *render )
{
   return (struct mixed_render *)render;
}


static void mixed_allocate_vertices( struct intel_render *render,
				     GLuint )
{
   /* Always build vertices in a local memory buffer.
    */
}
   

static void mixed_new_vertices( struct intel_render *render )
{
   struct mixed_render *mixed = mixed_render( render );
   struct intel_context *intel = mixed->intel;

   GLboolean ok = intel_vb_validate_vertices( intel->vb, VB_HW_VERTS );

   if (!ok) {
      mixed_flush( render, GL_FALSE );

      /* Not really sure how this could fail: 
       */
      ok = intel_vb_validate_vertices( intel->vb, VB_HW_VERTS );
      assert(ok);
   }

   mixed->offset = intel_vb_get_vbo_index_offset( intel->vb );
}


static void mixed_set_prim( struct intel_render *render,
			    const GLuint *indices,
				       GLuint nr )
{
   struct mixed_render *crc = mixed_render( render );

   if (mixmixed->prim
}


static void mixed_draw_prim( struct intel_render *render,
			     GLuint start,
			     GLuint nr )
{
   /* Emit vertices to active renderer at this point. 
    */
}

static void mixed_draw_indexed_prim( struct intel_render *render,
				     const GLuint *indices,
				     GLuint nr )
{
   /* Emit vertices to active renderer.  Use a vertex cache to
    * minimize duplication.
    */
}


static void mixed_set_prim( struct intel_render *render,
			      GLenum mode )
{
   struct mixed_render *mixed = mixed_render( render );
   struct intel_render *active;

   if (mixed->sw_prims & (1<<mode)) 
      active = mixed->sw;
   else
      active = mixed->hw;

   if (active != mixed->active) {
      mixed->active->flush( mixed->active, GL_FALSE );
      mixed->active = active;
      reset_vertex_cache( mixed );
   }
  
   mixed->active->set_prim( mixed->active, mode );
}


static void mixed_start_render( struct intel_render *render )
{
   struct mixed_render *mixed = mixed_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   /* Start a new batchbuffer, emit wait for pending flip.
    */
   intel_wait_flips(mixed->intel, 0);
}


static void mixed_flush( struct intel_render *render, 
			   GLboolean finished_frame )
{
}

static void mixed_abandon_frame( struct intel_render *render )
{
   /* struct mixed_render *mixed = mixed_render( render ); */
   _mesa_printf("%s\n", __FUNCTION__);
}

static void mixed_clear( struct intel_render *render )
{
   /* Not really sure how this will work yet. 
    */
   /* struct mixed_render *mixed = mixed_render( render ); */
   _mesa_printf("%s\n", __FUNCTION__);
   assert(0);
}



static void mixed_destroy_context( struct intel_render *render )
{
   struct mixed_render *mixed = mixed_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(mixed);
}

struct intel_render *intel_create_mixed_render( struct intel_context *intel )
{
   struct mixed_render *mixed = CALLOC_STRUCT(mixed_render);

   /* XXX: Add casts here to avoid the compiler messages:
    */
   mixed->render.destroy_context = mixed_destroy_context;
   mixed->render.start_render = mixed_start_render;
   mixed->render.abandon_frame = mixed_abandon_frame;
   mixed->render.flush = mixed_flush;
   mixed->render.clear = mixed_clear;
   mixed->render.set_prim = mixed_set_prim;
   mixed->render.new_vertices = mixed_new_vertices;
   mixed->render.draw_prim = mixed_draw_prim;
   mixed->render.draw_indexed_prim = mixed_draw_prim_indexed;

   mixed->intel = intel;
   mixed->hw_prim = 0;

   return &mixed->render;
}
