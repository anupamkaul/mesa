/*
 Copyright (C) Intel Corp.  2007.  All Rights Reserved.
 Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 develop this 3D driver.
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  *   Michel Dänzer <michel@tungstengraphics.com>
  */
      
#include "intel_context.h"
#include "intel_fbo.h"
#include "intel_vb.h"
#include "intel_batchbuffer.h"
#include "intel_lock.h"
#include "intel_frame_tracker.h"
#include "intel_reg.h"
#include "intel_swapbuffers.h"
#include "intel_state.h"
#include "intel_state_inlines.h"
#include "intel_frame_tracker.h"
#include "intel_utils.h"
#include "clip/clip_context.h"

#include "i915_context.h"
#include "i915_state.h"

#define FILE_DEBUG_FLAG DEBUG_RENDER

struct hwz_render {
   struct clip_render render;
   struct intel_context *intel;
   GLuint hw_prim;
   GLuint offset;

   GLboolean started_binning;
   GLboolean start_of_frame;
};

static INLINE struct hwz_render *hwz_render( struct clip_render *render )
{
   return (struct hwz_render *)render;
}


static void *hwz_allocate_vertices( struct clip_render *render,
					GLuint vertex_size,
					GLuint nr_vertices )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   void *ptr;

   ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &hwz->offset );

   if (!ptr) {
      render->flush( render, GL_FALSE );

      /* Not really sure how this could fail after the flush:
       */
      ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &hwz->offset );
      assert(ptr);
   }

   intel_update_software_state( intel );

   return ptr;
}

static void hwz_release_vertices( struct clip_render *render,
				      void *vertices )
{
   /* Nothing to do.   
    */
}


/* By the time this gets called, software state should be clean and
 * there should be sufficient batch to hold things...  How does that
 * work with big indexed primitives???
 *
 * Do we need to be able to split indexed prims?? - yes - would solve
 * isosurf crash also.
 */
static GLuint *hwz_emit_hardware_state( struct hwz_render *hwz,
					GLuint dwords,
					GLuint batchflags )
{
   struct intel_context *intel = hwz->intel;
   union i915_hw_dirty flags;
   GLuint state_size;

   flags.intel = intel_track_states( intel,
				     intel->state.hardware,
				     intel->state.current );
   state_size = intel->vtbl.get_state_emit_size( intel, flags.intel );

   if (flags.i915.indirect & (1<<I915_CACHE_STATIC)) {
      assert(!hwz->started_binning);

      flags.i915.indirect &= ~(1<<I915_CACHE_STATIC);
      state_size -= 8;
   }

   /* Just emit to the batch stream:
    */
   intel_batchbuffer_require_space( intel->batch,
				    0,
				    state_size + dwords * 4, 
				    batchflags );

   /* What do we do on flushes????
    */
   assert(intel->state.dirty.intel == 0);

   {
      GLuint *ptr = (GLuint *) (intel->batch->map +
				intel->batch->segment_finish_offset[0]);
      
      intel->vtbl.emit_hardware_state( intel, ptr,
				       intel->state.current,
				       flags.intel );


      intel->batch->segment_finish_offset[0] += state_size + dwords * 4;
      return ptr + state_size/4;
   }
}


static void hwz_draw_indexed_prim( struct clip_render *render,
				       const GLuint *indices,
				       GLuint nr )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   const GLuint offset = hwz->offset;
   GLuint j;

   DBG("%s (%d) %d\n", __FUNCTION__, hwz->hw_prim,  nr );

   if (nr == 0 || !intel_validate_vertices(hwz->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );
   hwz->start_of_frame = 0;

   /* The 'dwords' usage below ensures that both the state and the
    * primitive command below end up in the same batchbuffer,
    * otherwise there is a risk that another context might
    * interpose a batchbuffer containing different statesetting
    * commands.
    */
   GLuint dwords = 1 + (nr+1)/2;
   GLuint *ptr = hwz_emit_hardware_state(hwz, dwords, INTEL_BATCH_HWZ);

   *ptr++ = ( _3DPRIMITIVE | 
	      hwz->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_ELTS | 
	      nr );
      
   /* Pack indices into 16bits 
    */
   for (j = 0; j+1 < nr; j += 2) {
      *ptr++ = ( (offset + indices[j]) | ((offset + indices[j+1])<<16) );
   }

   if (j < nr) {
      *ptr++ = ( (offset + indices[j]) );
   }
}


static void hwz_draw_prim( struct clip_render *render,
			       GLuint start,
			       GLuint nr )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   GLuint dwords = 2;
   GLuint *ptr;

   DBG("%s (%d) %d/%d\n", __FUNCTION__, hwz->hw_prim, start, nr );

   if (nr == 0 || !intel_validate_vertices(hwz->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );
   hwz->start_of_frame = 0;

   ptr = hwz_emit_hardware_state(hwz, dwords, INTEL_BATCH_HWZ);

   ptr[0] = ( _3DPRIMITIVE | 
	      hwz->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_SEQUENTIAL | 
	      nr );      
   ptr[1] = ( hwz->offset + start );
}


static GLuint hw_prim[GL_POLYGON+1] = {
   PRIM3D_POINTLIST,
   PRIM3D_LINELIST,
   PRIM3D_LINESTRIP,
   PRIM3D_LINESTRIP,
   PRIM3D_TRILIST,
   PRIM3D_TRISTRIP,
   PRIM3D_TRIFAN,
   PRIM3D_POINTLIST,
   PRIM3D_POINTLIST,
   PRIM3D_POLY
};


static const GLenum reduced_prim[GL_POLYGON+1] = {
   GL_POINTS,
   GL_LINES,
   GL_LINES,
   GL_LINES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES
};

/* XXX: this is where we need to set the reduced primitive from: 
 */
static void hwz_set_prim( struct clip_render *render,
			      GLenum mode )
{
   struct hwz_render *hwz = hwz_render( render );

   switch (mode) {
   case GL_POINTS:
   case GL_LINES:
   case GL_LINE_STRIP:
   case GL_TRIANGLES:
   case GL_TRIANGLE_STRIP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      hwz->hw_prim = hw_prim[mode];
      break;

   case GL_LINE_LOOP:
   case GL_QUADS:
   case GL_QUAD_STRIP:
   default:
      /* Not supported by the hardware rasterizers. 
       */
      assert(0);
      break;
   }

//   _mesa_printf("%s %d -> %x\n", __FUNCTION__, mode, hwz->hw_prim );

   if (hwz->intel->hw_reduced_prim != reduced_prim[mode]) {
      hwz->intel->hw_reduced_prim = reduced_prim[mode];
      hwz->intel->state.dirty.intel |= INTEL_NEW_REDUCED_PRIMITIVE;

      intel_update_software_state( hwz->intel );
   }


   assert(!hwz->intel->Fallback);
}


static void hwz_start_render( struct clip_render *render,
				  GLboolean start_of_frame )
{
   struct hwz_render *hwz = hwz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   assert(!hwz->started_binning);

   (void) hwz_emit_hardware_state(hwz, 0, INTEL_BATCH_HWZ);

   hwz->started_binning = GL_TRUE;
   hwz->start_of_frame = start_of_frame;
}


static void hwz_flush( struct clip_render *render, 
			   GLboolean finished_frame )
{
   struct hwz_render *hwz = hwz_render( render );

   struct intel_context *intel = hwz->intel;



   if (intel->batch->segment_finish_offset[0] != 0)
      intel_batchbuffer_flush(intel->batch, !finished_frame);

   hwz->started_binning = 0;
   hwz->start_of_frame = 0;
}


static void hwz_clear_rect( struct clip_render *render,
				GLuint mask,
				GLuint x1, GLuint y1, 
				GLuint x2, GLuint y2 )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   struct intel_framebuffer *intel_fb = intel_get_fb( intel );
   GLboolean do_depth = !!(mask & BUFFER_BIT_DEPTH);
   GLboolean do_stencil = !!(mask & BUFFER_BIT_STENCIL);
   union fi *ptr;

   DBG("%s %d..%d %d..%d\n", __FUNCTION__, x1, x2, y1, y2);

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );

   ptr = (union fi *)hwz_emit_hardware_state(hwz, 7, INTEL_BATCH_HWZ);

   if (intel_fb->may_use_zone_init &&
       hwz->start_of_frame &&
       x1 == 0 &&
       y1 == 0 &&
       x2 == intel_fb->Base.Width &&
       y2 == intel_fb->Base.Height &&
       do_depth == do_stencil &&
       do_depth  			/* ??? */
       ) {
      ptr[0].i = (_3DPRIMITIVE | PRIM3D_ZONE_INIT | 5);

      intel->batch->zone_init_offset = (char*)&ptr[0].u -
	 (char*)intel->batch->map;
   }
   else {
      ptr[0].i = (_3DPRIMITIVE | PRIM3D_CLEAR_RECT | 5);
   }

   ptr[1].f = x2;
   ptr[2].f = y2;
   ptr[3].f = x1;
   ptr[4].f = y2;
   ptr[5].f = x1;
   ptr[6].f = y1;

   hwz->start_of_frame = 0;
}




static void hwz_destroy_context( struct clip_render *render )
{
   struct hwz_render *hwz = hwz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(hwz);
}

struct clip_render *intel_create_hwz_render( struct intel_context *intel )
{
   struct hwz_render *hwz = CALLOC_STRUCT(hwz_render);

   hwz->render.name = "hwz";
   hwz->render.limits.max_indices = (SEGMENT_SZ - 1024) / sizeof(GLushort);

   hwz->render.destroy = hwz_destroy_context;
   hwz->render.start_render = hwz_start_render;
   hwz->render.allocate_vertices = hwz_allocate_vertices;
   hwz->render.set_prim = hwz_set_prim;
   hwz->render.draw_prim = hwz_draw_prim;
   hwz->render.draw_indexed_prim = hwz_draw_indexed_prim;
   hwz->render.release_vertices = hwz_release_vertices;
   hwz->render.flush = hwz_flush;
   hwz->render.clear_rect = hwz_clear_rect;

   hwz->intel = intel;
   hwz->hw_prim = 0;

   return &hwz->render;
}
