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
#include "intel_frame_tracker.h"
#include "intel_state.h"
#include "intel_state_inlines.h"
#include "intel_utils.h"
#include "clip/clip_context.h"

struct classic_render {
   struct clip_render render;
   struct intel_context *intel;
   GLuint hw_prim;
   GLuint offset;
};

static INLINE struct classic_render *classic_render( struct clip_render *render )
{
   return (struct classic_render *)render;
}


static void *classic_allocate_vertices( struct clip_render *render,
					GLuint vertex_size,
					GLuint nr_vertices )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;
   void *ptr;

   ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &crc->offset );

   if (!ptr) {
      render->flush( render, GL_FALSE );

      /* Not really sure how this could fail after the flush:
       */
      ptr = intel_vb_alloc_vertices( intel->vb, nr_vertices, &crc->offset );
      assert(ptr);
   }

   intel_update_software_state( intel );

   return ptr;
}

static void classic_release_vertices( struct clip_render *render,
				      void *vertices )
{
   /* Nothing to do.   
    */
}


static void classic_draw_indexed_prim( struct clip_render *render,
				       const GLuint *indices,
				       GLuint nr )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;
   const GLuint offset = crc->offset;
   GLuint j;

   if (nr == 0 || !intel_validate_vertices(crc->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_CLASSIC );


   /* The 'dwords' usage below ensures that both the state and the
    * primitive command below end up in the same batchbuffer,
    * otherwise there is a risk that another context might
    * interpose a batchbuffer containing different statesetting
    * commands.
    */
   GLuint dwords = 1 + (nr+1)/2;
   GLuint *ptr = intel_emit_hardware_state(intel, dwords, INTEL_BATCH_CLIPRECTS);

   *ptr++ = ( _3DPRIMITIVE | 
	      crc->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_ELTS | 
	      nr );
      
   for (j = 0; j+1 < nr; j += 2) {
      *ptr++ = ( (offset + indices[j]) | ((offset + indices[j+1])<<16) );
   }

   if (j < nr) {
      *ptr++ = ( (offset + indices[j]) );
   }
}


static void classic_draw_prim( struct clip_render *render,
			       GLuint start,
			       GLuint nr )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;
   GLuint dwords = 2;
   GLuint *ptr;

//   _mesa_printf("%s (%d) %d/%d\n", __FUNCTION__, crc->hw_prim, start, nr );

   if (nr == 0 || !intel_validate_vertices(crc->hw_prim, nr))
      return; 

   intel_frame_set_mode( intel->ft, INTEL_FT_CLASSIC );

   ptr = intel_emit_hardware_state(intel, dwords, INTEL_BATCH_CLIPRECTS);

   ptr[0] = ( _3DPRIMITIVE | 
	      crc->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_SEQUENTIAL | 
	      nr );      
   ptr[1] = ( crc->offset + start );
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
static void classic_set_prim( struct clip_render *render,
			      GLenum mode )
{
   struct classic_render *crc = classic_render( render );

   switch (mode) {
   case GL_POINTS:
   case GL_LINES:
   case GL_LINE_STRIP:
   case GL_TRIANGLES:
   case GL_TRIANGLE_STRIP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      crc->hw_prim = hw_prim[mode];
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

//   _mesa_printf("%s %d -> %x\n", __FUNCTION__, mode, crc->hw_prim );

   if (crc->intel->hw_reduced_prim != reduced_prim[mode]) {
      crc->intel->hw_reduced_prim = reduced_prim[mode];
      crc->intel->state.dirty.intel |= INTEL_NEW_REDUCED_PRIMITIVE;

      intel_update_software_state( crc->intel );
   }


   assert(!crc->intel->Fallback);
}


static void classic_start_render( struct clip_render *render,
				  GLboolean start_of_frame )
{
   struct classic_render *crc = classic_render( render );
//   _mesa_printf("%s\n", __FUNCTION__);

   /* Should already be flushed!
    */
   assert( crc->intel->batch->segment_finish_offset[0] == 0);

   /* Start a new batchbuffer, emit wait for pending flip.
    */
   if (start_of_frame)
      intel_wait_flips(crc->intel, 0);

#if 0
   /* Flush any non-cliprect batch now:
    */
   intel_batchbuffer_require_space( crc->intel->batch, 0, 
				    SEGMENT_IMMEDIATE,
				    INTEL_BATCH_CLIPRECTS );
#endif

   intel_update_software_state( crc->intel );
}


static void classic_flush( struct clip_render *render, 
			   GLboolean finished_frame )
{
   struct classic_render *crc = classic_render( render );

   struct intel_context *intel = crc->intel;

   if (intel->batch->segment_finish_offset[0] != 0)
      intel_batchbuffer_flush(intel->batch, !finished_frame);
}


static void classic_clear_rect( struct clip_render *render,
				GLuint unused_mask,
				GLuint x1, GLuint y1, 
				GLuint x2, GLuint y2 )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;

   intel_frame_set_mode( intel->ft, INTEL_FT_CLASSIC );

   /* XXX: i915 only
    */
#define PRIM3D_CLEAR_RECT	(0xa<<18)

   union fi *ptr = (union fi *)intel_emit_hardware_state(intel, 7, 
							 INTEL_BATCH_CLIPRECTS);

   ptr[0].i = (_3DPRIMITIVE | PRIM3D_CLEAR_RECT | 5);
   ptr[1].f = x2;
   ptr[2].f = y2;
   ptr[3].f = x1;
   ptr[4].f = y2;
   ptr[5].f = x1;
   ptr[6].f = y1;
}



static void classic_destroy_context( struct clip_render *render )
{
   struct classic_render *crc = classic_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(crc);
}

struct clip_render *intel_create_classic_render( struct intel_context *intel )
{
   struct classic_render *crc = CALLOC_STRUCT(classic_render);

   crc->render.limits.max_indices = (SEGMENT_SZ - 1024) / sizeof(GLushort);

   crc->render.destroy = classic_destroy_context;
   crc->render.start_render = classic_start_render;
   crc->render.allocate_vertices = classic_allocate_vertices;
   crc->render.set_prim = classic_set_prim;
   crc->render.draw_prim = classic_draw_prim;
   crc->render.draw_indexed_prim = classic_draw_indexed_prim;
   crc->render.release_vertices = classic_release_vertices;
   crc->render.flush = classic_flush;
   crc->render.clear_rect = classic_clear_rect;

   crc->intel = intel;
   crc->hw_prim = 0;

   return &crc->render;
}
