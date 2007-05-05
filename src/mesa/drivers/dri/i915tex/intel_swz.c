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
      
#include "i915_context.h"
#include "i915_state.h"
#include "i915_reg.h"

#include "intel_batchbuffer.h"
#include "intel_vb.h"
#include "intel_state_inlines.h"
#include "intel_swapbuffers.h"
#include "intel_lock.h"

#include "draw/intel_draw.h"

#define INTEL_SWZ_PRIVATE
#include "intel_swz.h"



static void *swz_allocate_vertices( struct intel_render *render,
					GLuint vertex_size,
					GLuint nr_vertices )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   void *ptr;

   ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &swz->vbo_offset );

   if (!ptr) {
      render->flush( render, GL_FALSE );

      /* Not really sure how this could fail after the flush:
       */
      ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &swz->vbo_offset );
      assert(ptr);
   }

   swz->vertex_stride = vertex_size;
   swz->vertices = ptr;

   return ptr;
}

static void swz_release_vertices( struct intel_render *render,
				      void *vertices )
{
   struct swz_render *swz = swz_render( render );
   swz->vertex_stride = 0;
   swz->vertices = NULL;
}


static void swz_start_render( struct intel_render *render,
			      GLboolean start_of_frame )
{
   struct swz_render *swz = swz_render( render );
   GLuint x, y;
   GLuint i = 0;
      
   /* Goes to the main batchbuffer: 
    */
//   intel_wait_flips(intel, INTEL_BATCH_NO_CLIPRECTS);

   /* Window size won't change during a frame (though cliprects may be
    * applied at the end).  Hence we can set up all the zones & zone
    * preamble here.
    */
   for (y = 0; y < swz->zone_height; y++) {
      for (x = 0; x < swz->zone_stride; x++, i++) {
	 /* alloc initial block */
	 swz->initial_ptr[i] = intel_cmdstream_alloc_block( swz->intel );
	 swz->zone[i].ptr = swz->initial_ptr[i];
	 swz->zone[i].state.prim = ZONE_NONE;
	 swz->zone[i].state.dirty = 0;
      }
   }

   swz->nr_zones = i;
   swz->started_binning = GL_TRUE; /* actually want to delay until first prim is seen */
}




/* Called on frame end & premature frame end, which can occur for all
 * sorts of reasons.
 */
static void swz_flush( struct intel_render *render,
		       GLboolean finished )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   GLuint i;
   GLuint x, y;

   LOCK_HARDWARE( intel );
   UPDATE_CLIPRECTS( intel );

   /* The window may have moved since we started, in which case the
    * bins constructed will no longer align with hardware zones.  That
    * sucks, we just ignore it.  Oh for private backbuffers.
    */
   for (y = 0; y < swz->zone_height; y++) 
   {
      for (x = 0; x < swz->zone_stride; x++, i++) 
      {
	 zone_finish_prim( &swz->zone[i] );

	 GLuint zx1 = x * ZONE_WIDTH;
	 GLuint zy1 = y * ZONE_HEIGHT;
	 GLuint zx2 = zx1 + ZONE_WIDTH - 1;
	 GLuint zy2 = zy1 + ZONE_HEIGHT - 1;
	 
	 zone_draw_rect( &swz->zone[i], 
			 intel->drawX, intel->drawY,
			 zx1, zy1,
			 zx2, zy2 );

	 zone_begin_batch( swz,
			   &swz->zone[i], 
			   swz->initial_ptr[i + 1] );
      }
   }

   /* Last zone: 
    */
   zone_finish_prim( &swz->zone[i] );   
   zone_end_batch( &swz->zone[i] );

   intel_batchbuffer_flush( intel->batch, !finished );
   UNLOCK_HARDWARE( intel );
}

static void swz_set_state( struct intel_render *render )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   struct intel_hw_dirty flags = intel->vtbl.get_hw_dirty( intel );
   
   if (flags.dirty == 0)
      return;

   if (1 /* swz->started_binning */) {

      GLuint i;

      /* Just mark the differences, state will be emitted per-zone later
       * on.
       */
      for (i = 0; i < swz->nr_zones; i++)
	 swz->zone[i].state.dirty |= flags.dirty;
      
      swz->state_reset_bits |= flags.dirty;
   }
   else {
      /* Emit initial states to zone[0] only. 
       */
   }
}



static void swz_destroy_context( struct intel_render *render )
{
   struct swz_render *swz = swz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(swz);
}

struct intel_render *intel_swz_create_context( struct intel_context *intel )
{
   struct swz_render *swz = CALLOC_STRUCT(swz_render);

   swz->render.destroy = swz_destroy_context;
   swz->render.start_render = swz_start_render;
   swz->render.allocate_vertices = swz_allocate_vertices;
   swz->render.set_prim = swz_set_prim;
   swz->render.draw_prim = NULL;
   swz->render.draw_indexed_prim = NULL;
   swz->render.release_vertices = swz_release_vertices;
   swz->render.flush = swz_flush;
   swz->render.clear_rect = swz_clear_rect;

   swz->intel = intel;

   return &swz->render;
}


   


