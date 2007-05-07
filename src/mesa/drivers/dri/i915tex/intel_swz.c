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

   void *ptr = intel_vb_alloc_vertices( intel->vb, 
					nr_vertices,
					&swz->vbo_offset );

   if (!ptr) {
      render->flush( render, GL_FALSE );

      /* Not really sure how this could fail after the flush:
       */
      ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &swz->vbo_offset );
      assert(ptr);
   }

   swz->nr_vertices = nr_vertices;
   swz->vertex_stride = vertex_size;
   swz->vbo_vertices = ptr;

#define LOCAL_VERTS 1
#if LOCAL_VERTS
   swz->vertices = malloc(vertex_size * nr_vertices);
#else
   swz->vertices = swz->vbo_vertices;
#endif

   return swz->vertices;
}

static void swz_release_vertices( struct intel_render *render,
				      void *vertices )
{
   struct swz_render *swz = swz_render( render );

#if LOCAL_VERTS
   memcpy( swz->vbo_vertices, swz->vertices, 
	   swz->nr_vertices * swz->vertex_stride );
#endif

   swz->vertex_stride = 0;   
   swz->nr_vertices = 0;   
   swz->vbo_vertices = NULL;
   swz->vertices = NULL;
}


static void swz_start_render( struct intel_render *render,
			      GLboolean start_of_frame )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   GLuint i = 0;
      
   assert(!swz->started_binning);

   intel_cmdstream_reset( intel );

   /* Window size won't change during a frame (though cliprects may be
    * applied at the end).  
    */
   swz->zone_width  = align(intel->driDrawable->w, ZONE_WIDTH) / ZONE_WIDTH;
   swz->zone_height = align(intel->driDrawable->h, ZONE_HEIGHT) / ZONE_HEIGHT;
   swz->nr_zones = swz->zone_width * swz->zone_height;
   assert(swz->nr_zones < MAX_ZONES);
   
   if (0) _mesa_printf("swz %dx%d --> %dx%d\n", 
		       intel->driDrawable->w,
		       intel->driDrawable->h,
		       swz->zone_width,
		       swz->zone_height);


   /* Goes to the main batchbuffer: 
    */
//   intel_wait_flips(intel, INTEL_BATCH_NO_CLIPRECTS);

   for (i = 0; i < swz->nr_zones; i++) {
      /* alloc initial block */
      swz->initial_ptr[i] = intel_cmdstream_alloc_block( swz->intel );
      swz->zone[i].ptr = swz->initial_ptr[i];
      swz->zone[i].state.prim = ZONE_NONE;
      swz->zone[i].state.dirty = 0;
      swz->zone[i].state.swz_reset = 0;
   }

   memcpy( swz->initial_driver_state,
	   intel->state.current,
	   intel->state.driver_state_size );

   memcpy( swz->last_driver_state,
	   intel->state.current,
	   intel->state.driver_state_size );


   /* Emit the initial state to zone zero in full.  
    */
   {
      struct intel_hw_dirty flags;
      GLuint space;

      flags = intel->vtbl.diff_states( NULL, /* ??? */
				       swz->initial_driver_state );

      space = intel->vtbl.get_state_emit_size( intel, flags );
      
      intel->vtbl.emit_hardware_state( intel, 
				       (GLuint *)swz->zone[0].ptr,
				       swz->initial_driver_state,
				       flags, GL_TRUE );

      swz->zone[0].ptr += space;
   }

   /* Initial draw rectangle.  
    */
   zone_draw_rect( &swz->zone[0], 
		   intel->drawX, 
		   intel->drawY,
		   0, 0,
		   ZONE_WIDTH-1, 
		   ZONE_HEIGHT-1 );

   swz->started_binning = GL_TRUE; 
}


#if 0
static void swz_clear( struct intel_render *render,
		       GLuint unused_mask,
		       GLuint x1, GLuint y1, 
		       GLuint x2, GLuint y2 )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   struct intel_framebuffer *intel_fb = intel_get_fb( intel );
   if (intel_frame_is_double_buffered( intel->ft ) &&
       intel_frame_draws_since_swap( intel->ft ) == 0 &&
       swz_zone_aligned( swz ) &&
       x1 == 0 && 
       y1 == 0 &&
       x2 == intel_fb->Base.Width &&
       y2 == intel_fb->Base.Height )
   {
      swz->zone_init = GL_TRUE;
      swz_zone_init( render, unused_mask, x1, y1, x2, y2 );
   }
   else 
   {
      swz_clear_rect( render, unused_mask, x1, y1, x2, y2 );
   }
}
#endif



/* Called on frame end & premature frame end, which can occur for all
 * sorts of reasons.
 */
static void swz_flush( struct intel_render *render,
		       GLboolean finished )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   GLuint i = 0;
   GLuint x, y;

   struct intel_hw_dirty state_reset = intel->vtbl.diff_states( swz->last_driver_state,
								swz->initial_driver_state );

   if (swz->started_binning) 
   {
      GLuint state_space = intel->vtbl.get_state_emit_size( intel, state_reset );

//      assert(finished);

      LOCK_HARDWARE( intel );
      UPDATE_CLIPRECTS( intel );

      /* The window may have moved since we started, in which case the
       * bins constructed will no longer align with hardware zones.  That
       * sucks, we just ignore it.  Oh for private backbuffers.
       */
      for (x = y = i = 0; i < swz->nr_zones - 1; i++)
      {
	 struct swz_zone *zone = &swz->zone[i];
	 GLuint space = ZONE_DRAWRECT_SPACE + ZONE_END_SPACE;

	 zone_finish_prim( zone );

	 if (zone->state.swz_reset)
	    space += state_space;

	 if (intel_cmdstream_space( zone->ptr ) < space )
	 {
	    zone_get_space( swz, zone );
	 }

	 if (++x >= swz->zone_width) {
	    x = 0;
	    y++;
	 }

	 /*  Emit next zone's draw rect:
	  */
	 {
	    GLuint zx1 = x * ZONE_WIDTH;
	    GLuint zy1 = y * ZONE_HEIGHT;
	    GLuint zx2 = zx1 + ZONE_WIDTH - 1;
	    GLuint zy2 = zy1 + ZONE_HEIGHT - 1;
	 
	    zone_draw_rect( zone, 
			    intel->drawX, intel->drawY,
			    zx1, zy1,
			    zx2, zy2 );
	 }

	 /* If per-zone state has been emitted in this zone, issue
	  * another statechange to reset to the original state.
	  */
	 if (zone->state.swz_reset) 
	 {
	    intel->vtbl.emit_hardware_state( intel, 
					     (GLuint *)zone->ptr,
					     swz->initial_driver_state,
					     state_reset,
					     GL_FALSE );
	 }

	 zone_begin_batch( swz, zone, swz->initial_ptr[i + 1] );
      }


      /* Last zone: 
       */
      zone_finish_prim( &swz->zone[i] );   

      zone_end_batch( &swz->zone[i], intel->vtbl.flush_cmd() );

      /* Hmmm.  Issue a pointless begin batch to jump to the first zone:
       */
      BEGIN_BATCH(2, 0);

      OUT_BATCH( MI_BATCH_BUFFER_START |
		 MI_BATCH_GTT );	

      OUT_RELOC( intel->batch->buffer,
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,			 
		 swz->initial_ptr[0] - swz->intel->batch->map );
      
      ADVANCE_BATCH();

      intel_batchbuffer_flush( intel->batch, !finished );
      UNLOCK_HARDWARE( intel );
   }

   swz->started_binning = GL_FALSE;
}




static void swz_destroy_context( struct intel_render *render )
{
   struct swz_render *swz = swz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(swz->initial_driver_state);
   _mesa_free(swz->last_driver_state);
   _mesa_free(swz);
}

struct intel_render *intel_create_swz_render( struct intel_context *intel )
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

   swz->initial_driver_state = _mesa_malloc( intel->state.driver_state_size );
   swz->last_driver_state = _mesa_malloc( intel->state.driver_state_size );

   return &swz->render;
}


   


