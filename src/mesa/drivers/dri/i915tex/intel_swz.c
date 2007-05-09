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
#include "intel_fbo.h"
#include "intel_state_inlines.h"
#include "intel_swapbuffers.h"
#include "intel_frame_tracker.h"
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
      intel_frame_flush_and_restart( intel->ft );

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
   memcpy( swz->vbo_vertices, 
	   swz->vertices, 
	   swz->nr_vertices * swz->vertex_stride );

   free(swz->vertices);
#endif

   swz->vertex_stride = 0;   
   swz->nr_vertices = 0;   
   swz->vbo_vertices = NULL;
   swz->vertices = NULL;
}


static void init_zones( struct swz_render *swz )
{
   struct intel_context *intel = swz->intel;
   struct intel_hw_dirty flags = 
      intel->vtbl.diff_states( NULL, intel->state.current );
   GLuint space = intel->vtbl.get_state_emit_size( intel, flags );
   GLint x, y, i;
   GLint drawX = intel->drawX; 
   GLint drawY = intel->drawY; 

   _mesa_printf("draw origin: %d,%d\n", intel->drawX, intel->drawY);

   for (i = y = 0; y < swz->zone_height; y++)
   {
      for (x = 0; x < swz->zone_width ; x++, i++)
      {
	 struct swz_zone *zone = &swz->zone[i];

	 swz->initial_ptr[i] = intel_cmdstream_alloc_block( swz->intel );

	 zone->ptr = swz->initial_ptr[i];
	 zone->state.prim = ZONE_NONE;
	 zone->state.force_reload = 0;
	 zone->state.swz_reset = 0;
	 zone->state.dirty = 0;

	 /* Emit the initial state in zone zero::
	  */
	 if (i == 0)
	    intel->vtbl.emit_hardware_state( intel, 
					     (GLuint *)zone->ptr,
					     intel->state.current,
					     flags );
	 else
	    memset( zone->ptr, 0, space );
      
	 /* But always leave space for it:
	  */
	 zone->ptr += space;
      
	 if (space & 4) 
	    zone_emit_noop( zone );
      
	 /* Draw rectangle.  
	  */
	 zone_draw_rect( zone, 
			 drawX - swz->xoff + x * ZONE_WIDTH,
			 drawY - swz->yoff + y * ZONE_HEIGHT,
			 drawX - swz->xoff + x * ZONE_WIDTH + ZONE_WIDTH - 1,
			 drawY - swz->yoff + y * ZONE_HEIGHT + ZONE_HEIGHT - 1,
			 drawX, 
			 drawY );
      }
   }


   swz->initial_state_size = align(space, 8);
}		       


static void swz_start_render( struct intel_render *render,
			      GLboolean start_of_frame )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;

   assert(intel_frame_mode(intel->ft) == INTEL_FT_FLUSHED);
   assert(!swz->started_binning);

   /* Window size won't change during a frame (though cliprects may be
    * applied at the end).  
    */
   swz->xoff = intel->drawX % 64;
   swz->yoff = intel->drawY % 32;

   swz->zone_width  = align(intel->driDrawable->w + swz->xoff, ZONE_WIDTH) / ZONE_WIDTH;
   swz->zone_height = align(intel->driDrawable->h + swz->yoff, ZONE_HEIGHT) / ZONE_HEIGHT;
   swz->nr_zones = swz->zone_width * swz->zone_height;
   assert(swz->nr_zones < MAX_ZONES);
   
   if (1) _mesa_printf("%s %dx%d --> %dx%d\n", 
		       __FUNCTION__,
		       intel->driDrawable->w,
		       intel->driDrawable->h,
		       swz->zone_width,
		       swz->zone_height);


   /* Goes to the main batchbuffer: 
    */
//   intel_wait_flips(intel, INTEL_BATCH_NO_CLIPRECTS);

   memcpy( swz->last_driver_state,
	   intel->state.current,
	   intel->state.driver_state_size );


   intel_cmdstream_reset( intel );
   swz->pre_post.ptr = intel_cmdstream_alloc_block( swz->intel );
   assert(swz->pre_post.ptr == intel->batch->map);
  
   init_zones( swz );

   swz->reset_state.prim = 0;
   swz->reset_state.force_reload = 0;
   swz->reset_state.swz_reset = 0;
   swz->reset_state.dirty = 0;

   swz->draws = 0;
   swz->clears = 0;
   swz->started_binning = GL_TRUE; 
}


static void emit_initial_state( struct swz_render *swz,
				GLuint i )
{
   struct intel_context *intel = swz->intel;

   assert(i > 0);
   assert(swz->reset_state.force_reload == 0);

   intel->vtbl.emit_hardware_state( intel, 
				    (GLuint *)swz->initial_ptr[i],
				    intel->state.current,
				    swz->reset_state );
}
				


static INLINE void chain_zones( struct swz_render *swz,
				GLuint i,
				GLuint x, 
				GLuint y )

{
   struct swz_zone *zone = &swz->zone[i];

   /* finish prim: 2 dwords
    * mi_flush: 1 dword
    * begin_batch: 2 dwords
    */
   assert(intel_cmdstream_space(zone->ptr) >= 16);

   zone_finish_prim( zone );

   /* Emit a flush with the end_scene bit set
    */
   zone_mi_flush( zone, (1<<4) );

   if (i+1 < swz->nr_zones) {
      if (zone->state.swz_reset) {
	 /* If per-zone state was emitted in this zone, issue another
	  * statechange to reset to the original state in zone i+1.
	  */
	 emit_initial_state( swz, i+1 );
	 zone_begin_batch( swz, zone, swz->initial_ptr[i+1] );
      }
      else {
	 zone_begin_batch( swz, zone, swz->initial_ptr[i+1] + swz->initial_state_size );
      }
   }
   else {
      zone_begin_batch( swz, zone, swz->pre_post.ptr );
   }

}




/* Called on transition to INTEL_FT_FLUSHED, which happens at frame
 * end, renderer switch and flush, which can occur for all sorts of
 * reasons.
 */
static void swz_flush( struct intel_render *render,
		       GLboolean finished )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   GLuint i = 0;
   GLuint x, y;

   _mesa_printf("%s finished: %d\n", __FUNCTION__, finished);

   if (swz->started_binning) 
   {
      if (swz->vertices) 
	 memcpy( swz->vbo_vertices, 
		 swz->vertices, 
		 swz->nr_vertices * swz->vertex_stride );




      /* Emit preamble - tweak cachemode0:
       */
      if ( 0 &&
	   (intel->state.depth_region == 0 ||
	    finished)
	 ) 
	 zone_loadreg_imm( &swz->pre_post, 
			   CACHE_MODE_0, 
			   (CM0_RC_OP_FLUSH_MODIFY |
			    CM0_DEPTH_WRITE_MODIFY |
			    CM0_ZONE_OPT_MODIFY |
			    CM0_ZONE_OPT_ENABLE |
			    CM0_RC_OP_FLUSH_DISABLE |
			    CM0_DEPTH_WRITE_DISABLE) );
      else
	 zone_loadreg_imm( &swz->pre_post, 
			   CACHE_MODE_0, 
			   (CM0_RC_OP_FLUSH_MODIFY |
			    CM0_DEPTH_WRITE_MODIFY |
			    CM0_ZONE_OPT_MODIFY |
			    CM0_ZONE_OPT_ENABLE |
			    CM0_RC_OP_FLUSH_DISABLE |
			    CM0_DEPTH_WRITE_ENABLE) );



      /* Call into the first zone:
       */
      zone_begin_batch( swz, &swz->pre_post, swz->initial_ptr[0] );
      
      if (((unsigned long)swz->pre_post.ptr) & 4)
	 zone_emit_noop( &swz->pre_post );

      /* We keep the same window position we started with...  That may
       * leave droppings in X, but at least ZONE_INIT prims will be
       * happy.  Oh for private backbuffers.
       */
      for (i = y = 0; y < swz->zone_height; y++)
      {
	 for (x = 0; x < swz->zone_width ; x++, i++)
	 {
	    chain_zones(swz, i, x, y);
	 }
      }

      /* Emit postamble:
       */     

      /* Restore CACHE_MODE_0 bits:
       */
      zone_loadreg_imm( &swz->pre_post, 
			CACHE_MODE_0, 
			(CM0_RC_OP_FLUSH_MODIFY |
			 CM0_DEPTH_WRITE_MODIFY |
			 CM0_ZONE_OPT_MODIFY |
			 CM0_ZONE_OPT_DISABLE |
			 CM0_RC_OP_FLUSH_ENABLE |
			 CM0_DEPTH_WRITE_ENABLE) );


      /* Tell the batchbuffer code about what we've emitted:
       */
      intel->batch->segment_finish_offset[0] = swz->pre_post.ptr - intel->batch->map;
      intel_batchbuffer_flush( intel->batch, !finished );
   }

   memset(swz->zone, 0, sizeof(swz->zone));
   memset(swz->initial_ptr, 0, sizeof(swz->initial_ptr));
   swz->started_binning = GL_FALSE;
}



static void swz_clear( struct intel_render *render,
		       GLuint mask,
		       GLuint x1, GLuint y1, 
		       GLuint x2, GLuint y2 )
{
   struct swz_render *swz = swz_render( render );
   struct intel_context *intel = swz->intel;
   struct intel_framebuffer *intel_fb = intel_get_fb( intel );
   GLboolean do_depth = !!(mask & BUFFER_BIT_DEPTH);
   GLboolean do_stencil = !!(mask & BUFFER_BIT_STENCIL);


   if (mask == 0)
      return;

   intel_frame_set_mode( intel->ft, INTEL_FT_SWZ );
   assert(swz->started_binning);

   /* Turn on stencil clear if possible.  Otherwise apps that don't
    * request a stencil buffer are prevented from using the zone_init
    * path.
    */
   if (do_depth && intel_frame_can_clear_stencil( intel->ft ))
      do_stencil = GL_TRUE;

   if (intel_fb->may_use_zone_init &&
       !intel_frame_is_in_frame(intel->ft) &&
       x1 == 0 &&
       y1 == 0 &&
       x2 == intel_fb->Base.Width &&
       y2 == intel_fb->Base.Height &&
       do_depth == do_stencil &&
       do_depth  			/* ??? */
      )
   {
      swz_zone_init( render, mask, x1, y1, x2, y2 );
   }
   else 
   {
      _mesa_printf("%d, %d\n", x2, y2);
      swz_clear_rect( render, mask, x1, y1, x2, y2 );
   }
}


static void swz_destroy_context( struct intel_render *render )
{
   struct swz_render *swz = swz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(swz->last_driver_state);
   _mesa_free(swz);
}



struct intel_render *intel_create_swz_render( struct intel_context *intel )
{
   struct swz_render *swz = CALLOC_STRUCT(swz_render);

   swz->render.limits.max_indices = ~0;
   swz->render.destroy = swz_destroy_context;
   swz->render.start_render = swz_start_render;
   swz->render.allocate_vertices = swz_allocate_vertices;
   swz->render.set_prim = swz_set_prim;
   swz->render.draw_prim = NULL;
   swz->render.draw_indexed_prim = NULL;
   swz->render.release_vertices = swz_release_vertices;
   swz->render.flush = swz_flush;
   swz->render.clear_rect = swz_clear;

   swz->intel = intel;

   swz->last_driver_state = _mesa_malloc( intel->state.driver_state_size );

   return &swz->render;
}


   



