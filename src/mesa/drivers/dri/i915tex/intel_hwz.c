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
#include "intel_swapbuffers.h"
#include "intel_state.h"
#include "intel_state_inlines.h"
#include "intel_frame_tracker.h"
#include "intel_utils.h"
#include "draw/intel_draw.h"

#include "i915_context.h"

struct hwz_render {
   struct intel_render render;
   struct intel_context *intel;
   GLuint hw_prim;
   GLuint offset;
};

static INLINE struct hwz_render *hwz_render( struct intel_render *render )
{
   return (struct hwz_render *)render;
}


static void *hwz_allocate_vertices( struct intel_render *render,
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

static void hwz_release_vertices( struct intel_render *render,
				      void *vertices )
{
   /* Nothing to do.   
    */
}



static void hwz_draw_indexed_prim( struct intel_render *render,
				       const GLuint *indices,
				       GLuint nr )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   const GLuint offset = hwz->offset;
   GLuint j;

   if (nr == 0 || !intel_validate_vertices(hwz->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );

   /* The 'dwords' usage below ensures that both the state and the
    * primitive command below end up in the same batchbuffer,
    * otherwise there is a risk that another context might
    * interpose a batchbuffer containing different statesetting
    * commands.
    */
   GLuint dwords = 1 + (nr+1)/2;
   GLuint *ptr = intel_emit_hardware_state(intel, dwords, INTEL_BATCH_HWZ);

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


static void hwz_draw_prim( struct intel_render *render,
			       GLuint start,
			       GLuint nr )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;
   GLuint dwords = 2;
   GLuint *ptr;

//   _mesa_printf("%s (%d) %d/%d\n", __FUNCTION__, hwz->hw_prim, start, nr );

   if (nr == 0 || !intel_validate_vertices(hwz->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );

   ptr = intel_emit_hardware_state(intel, dwords, INTEL_BATCH_HWZ);

   ptr[0] = ( _3DPRIMITIVE | 
	      hwz->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_SEQUENTIAL | 
	      nr );      
   ptr[1] = ( hwz->offset + start );
}

#if 0
static void hwz_set_state( struct intel_render *render,
			   void *state )
{
   /* Bogus - need to stop hwz at this point */
   {
      struct intel_framebuffer *intel_fb =
	 (struct intel_framebuffer*)intel->ctx.DrawBuffer;

      if ((dirty & (1<<0)) && intel_fb->hwz) {
	 dirty &= ~(1<<0);
	 size -= 2;
      }
   }

}
#endif


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
static void hwz_set_prim( struct intel_render *render,
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


static void hwz_start_render( struct intel_render *render,
				  GLboolean start_of_frame )
{
   struct hwz_render *hwz = hwz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   /* Start a new batchbuffer, emit wait for pending flip.
    */
   if (start_of_frame)
      intel_wait_flips(hwz->intel, 0);
}


static void hwz_flush( struct intel_render *render, 
			   GLboolean finished_frame )
{
   struct hwz_render *hwz = hwz_render( render );

   struct intel_context *intel = hwz->intel;



   if (intel->batch->segment_finish_offset[0] != 0)
      intel_batchbuffer_flush(intel->batch, !finished_frame);


}


static void hwz_clear_rect( struct intel_render *render,
				GLuint unused_mask,
				GLuint x1, GLuint y1, 
				GLuint x2, GLuint y2 )
{
   struct hwz_render *hwz = hwz_render( render );
   struct intel_context *intel = hwz->intel;

   intel_frame_set_mode( intel->ft, INTEL_FT_HWZ );

   /* XXX: i915 only
    */
#define PRIM3D_CLEAR_RECT	(0xa<<18)
#define PRIM3D_ZONE_INIT	(0xd<<18)

   union fi *ptr = (union fi *)intel_emit_hardware_state(intel, 7, INTEL_BATCH_HWZ);

   if (1) {
      ptr[0].i = (_3DPRIMITIVE | PRIM3D_CLEAR_RECT | 5);
      ptr[1].f = x2;
      ptr[2].f = y2;
      ptr[3].f = x1;
      ptr[4].f = y2;
      ptr[5].f = x1;
      ptr[6].f = y1;
   }
   else {
      ptr[0].i = (_3DPRIMITIVE | PRIM3D_ZONE_INIT | 5);
      ptr[1].f = x2;
      ptr[2].f = y2;
      ptr[3].f = x1;
      ptr[4].f = y2;
      ptr[5].f = x1;
      ptr[6].f = y1;
   }
}




static void hwz_destroy_context( struct intel_render *render )
{
   struct hwz_render *hwz = hwz_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(hwz);
}

struct intel_render *intel_create_hwz_render( struct intel_context *intel )
{
   struct hwz_render *hwz = CALLOC_STRUCT(hwz_render);

   hwz->render.limits.max_indices = (SEGMENT_SZ - 1024) / sizeof(GLushort);

   hwz->render.destroy = hwz_destroy_context;
   hwz->render.start_render = hwz_start_render;
   hwz->render.allocate_vertices = hwz_allocate_vertices;
//   hwz->render.set_state = hwz_set_state;
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
