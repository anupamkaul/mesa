/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "enums.h"

#include "intel_fbo.h"
#include "intel_regions.h"
#include "intel_state_inlines.h"

#include "i915_context.h"
#include "i915_reg.h"
#include "i915_state.h"
#include "i915_cache.h"

#define FILE_DEBUG_FLAG DEBUG_STATE

/* State that we have chosen to store in the STATIC segment of the
 * i915 indirect state mechanism.  
 *
 * There is a lot of flexibility for breaking state up between dynamic
 * & static buckets of the indirect mechanism.  
 * 
 * At a hardware level they are treated slightly differently, dynamic
 * state supports an incremental update model, whereas static is
 * better suited to usage where the same set of packets are present
 * every static state emit.  This isn't enforced though, and in normal
 * operations there is nothing preventing arbitary usage of either
 * bucket.
 */


/***********************************************************************
 * Depthbuffer - currently constant, but rotation would change that.
 */


static void upload_buffers(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct intel_region *color_region = intel->state.draw_region;
   struct intel_region *depth_region = intel->state.depth_region;

   GLuint dwords = ((color_region ? 3 : 0) + 
		    (depth_region ? 3 : 0) + 
		    2);

   GLuint relocs = ((color_region ? 1 : 0) + 
		    (depth_region ? 1 : 0));
   
   struct i915_cache_packet packet;

   packet_init( &packet, I915_CACHE_BUFFERS, dwords, relocs );

   if (color_region) {
      packet_dword( &packet, _3DSTATE_BUF_INFO_CMD );
      packet_dword( &packet, BUF_3D_ID_COLOR_BACK |
		    BUF_3D_PITCH(color_region->pitch * color_region->cpp) |
		    BUF_3D_USE_FENCE);

      packet_reloc( &packet, color_region->buffer,
		    DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
		    DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
		    color_region->draw_offset);
   }

   if (depth_region) {
      packet_dword( &packet, _3DSTATE_BUF_INFO_CMD );
      packet_dword( &packet, BUF_3D_ID_DEPTH |
		    BUF_3D_PITCH(depth_region->pitch * depth_region->cpp) |
		    BUF_3D_USE_FENCE );
      packet_reloc( &packet, depth_region->buffer,
		    DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
		    DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
		    depth_region->draw_offset);
   }


   packet_dword( &packet,_3DSTATE_DST_BUF_VARS_CMD);
   packet_dword( &packet, DSTORG_HORT_BIAS(0x8) |     /* .5 */
		 DSTORG_VERT_BIAS(0x8) |     /* .5 */
		 LOD_PRECLAMP_OGL |
		 TEX_DEFAULT_COLOR_OGL | 
		 DITHER_FULL_ALWAYS |
		 (color_region && color_region->cpp == 4 
		  ? DV_PF_8888
		  : DV_PF_565) |
		 (depth_region && depth_region->cpp == 4 
		  ? DEPTH_FRMT_24_FIXED_8_OTHER
		  : DEPTH_FRMT_16_FIXED) );

   i915_cache_emit( i915->cctx, &packet );
}

const struct intel_tracked_state i915_upload_buffers = {
   .dirty = {
      .mesa = 0,
      .intel = INTEL_NEW_CBUF | INTEL_NEW_ZBUF | INTEL_NEW_FENCE,
      .extra = 0
   },
   .update = upload_buffers
};



/***********************************************************************
 * Polygon stipple
 *
 * The i915 supports a 4x4 stipple natively, GL wants 32x32.
 * Fortunately stipple is usually a repeating pattern.
 *
 * XXX: does stipple pattern need to be adjusted according to
 * the window position?
 *
 * XXX: possibly need workaround for conform paths test. 
 */

static void upload_stipple( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint st0 = _3DSTATE_STIPPLE;
   GLuint st1 = 0;
   
   GLboolean hw_stipple_fallback = 0;

   /* _NEW_POLYGON, INTEL_NEW_REDUCED_PRIMITIVE 
    */
   if (intel->state.Polygon->StippleFlag &&
       intel->reduced_primitive == GL_TRIANGLES) {

      /* _NEW_POLYGONSTIPPLE
       */
      const GLubyte *mask = (const GLubyte *)intel->state.PolygonStipple;
      GLubyte p[4];
      GLint i, j, k;

      p[0] = mask[12] & 0xf;
      p[0] |= p[0] << 4;
      p[1] = mask[8] & 0xf;
      p[1] |= p[1] << 4;
      p[2] = mask[4] & 0xf;
      p[2] |= p[2] << 4;
      p[3] = mask[0] & 0xf;
      p[3] |= p[3] << 4;
      
      st1 |= ST1_ENABLE;

      for (k = 0; k < 8; k++) {
	 for (j = 3; j >= 0; j--) {
	    for (i = 0; i < 4; i++, mask++) {
	       if (*mask != p[j]) {
		  hw_stipple_fallback = 1;
		  st1 &= ~ST1_ENABLE;
	       }
	    }
	 }
      }      

      st1 |= (((p[0] & 0xf) << 0) |
	      ((p[1] & 0xf) << 4) |
	      ((p[2] & 0xf) << 8) | 
	      ((p[3] & 0xf) << 12));
   }

   assert(!hw_stipple_fallback); /* TODO */

   {
      struct i915_cache_packet packet;

      packet_init( &packet, I915_CACHE_STIPPLE, 2, 0 );
      packet_dword( &packet,st0);
      packet_dword( &packet,st1);
      i915_cache_emit( i915->cctx, &packet );
   }
}


const struct intel_tracked_state i915_upload_stipple = {
   .dirty = {
      .mesa = _NEW_POLYGONSTIPPLE, _NEW_POLYGON,
      .intel = INTEL_NEW_REDUCED_PRIMITIVE,
      .extra = 0
   },
   .update = upload_stipple
};



/***********************************************************************
 * Scissor.  
 */

static void upload_scissor( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct i915_cache_packet packet;

   /* _NEW_SCISSOR, _NEW_BUFFERS 
    */
   if (intel->state.Scissor->Enabled && 
       intel->state.DrawBuffer) {
      
      GLint x = intel->state.Scissor->X;
      GLint y = intel->state.Scissor->Y;
      GLint w = intel->state.Scissor->Width;
      GLint h = intel->state.Scissor->Height;
      
      GLint x1, y1, x2, y2;

      if (intel->state.DrawBuffer->Name == 0) {
	 x1 = x;
	 y1 = intel->state.DrawBuffer->Height - (y + h);
	 x2 = x + w - 1;
	 y2 = y1 + h - 1;
      }
      else {
	 /* FBO - not inverted
	  */
	 x1 = x;
	 y1 = y;
	 x2 = x + w - 1;
	 y2 = y + h - 1;
      }
   
      x1 = CLAMP(x1, 0, intel->state.DrawBuffer->Width - 1);
      y1 = CLAMP(y1, 0, intel->state.DrawBuffer->Height - 1);
      x2 = CLAMP(x2, 0, intel->state.DrawBuffer->Width - 1);
      y2 = CLAMP(y2, 0, intel->state.DrawBuffer->Height - 1);

      packet_init( &packet, I915_CACHE_SCISSOR, 4, 0 );
      packet_dword( &packet,_3DSTATE_SCISSOR_ENABLE_CMD | ENABLE_SCISSOR_RECT);
      packet_dword( &packet,_3DSTATE_SCISSOR_RECT_0_CMD);
      packet_dword( &packet,(y1 << 16) | (x1 & 0xffff));
      packet_dword( &packet,(y2 << 16) | (x2 & 0xffff));
      i915_cache_emit( i915->cctx, &packet );
   }
   else {
      packet_init( &packet, I915_CACHE_SCISSOR, 1, 0 );
      packet_dword( &packet,_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);
      i915_cache_emit( i915->cctx, &packet );
   }
}

const struct intel_tracked_state i915_upload_scissor = {
   .dirty = {
      .mesa = _NEW_SCISSOR | _NEW_BUFFERS,
      .intel = 0,
      .extra = 0
   },
   .update = upload_scissor
};






/***********************************************************************
 * Misc invarient state packets
 */

static void upload_invarient( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint i;

   static GLuint invarient_state[] = {

      (_3DSTATE_AA_CMD |
       AA_LINE_ECAAR_WIDTH_ENABLE |
       AA_LINE_ECAAR_WIDTH_1_0 |
       AA_LINE_REGION_WIDTH_ENABLE | 
       AA_LINE_REGION_WIDTH_1_0),

      /* Could use these to reduce the size of vertices when the incoming
       * array is constant.
       */
      (_3DSTATE_DFLT_DIFFUSE_CMD),
      (0),

      (_3DSTATE_DFLT_SPEC_CMD),
      (0),

      (_3DSTATE_DFLT_Z_CMD),
      (0),

      /* We support texture crossbar via the fragment shader, rather than
       * with this mechanism.
       */
      (_3DSTATE_COORD_SET_BINDINGS |
       CSB_TCB(0, 0) |
       CSB_TCB(1, 1) |
       CSB_TCB(2, 2) |
       CSB_TCB(3, 3) |
       CSB_TCB(4, 4) |
       CSB_TCB(5, 5) | 
       CSB_TCB(6, 6) | 
       CSB_TCB(7, 7)),

      /* Setup OpenGL rasterization state:
       */
      (_3DSTATE_RASTER_RULES_CMD |
       ENABLE_POINT_RASTER_RULE |
       OGL_POINT_RASTER_RULE |
       ENABLE_LINE_STRIP_PROVOKE_VRTX |
       ENABLE_TRI_FAN_PROVOKE_VRTX |
       LINE_STRIP_PROVOKE_VRTX(1) |
       TRI_FAN_PROVOKE_VRTX(2) | 
       ENABLE_TEXKILL_3D_4D | 
       TEXKILL_4D),

      /* Need to initialize this to zero.
       */
      (_3DSTATE_LOAD_STATE_IMMEDIATE_1 | 
       I1_LOAD_S(3) | 
       (0)),
      (0),

      (_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT),
      (_3DSTATE_SCISSOR_RECT_0_CMD),
      (0),
      (0),


      /* For private depth buffers but shared color buffers, eg
       * front-buffer rendering with a private depthbuffer.  We don't do
       * this.
       */
      (_3DSTATE_DEPTH_SUBRECT_DISABLE),

      (_3DSTATE_BACKFACE_STENCIL_OPS | BFO_ENABLE_STENCIL_TWO_SIDE | 0)
   };

   /* Disable indirect state for now.
    */
#if 0
   BEGIN_BATCH(2, 0);
   OUT_BATCH(_3DSTATE_LOAD_INDIRECT | 0);
   OUT_BATCH(0);
   ADVANCE_BATCH();
#endif

   /* Will be nice if this can be preserved over several frames.  I
    * guess logical contexts would do much the same thing.
    */
   {
      struct i915_cache_packet packet;

      packet_init( &packet, I915_CACHE_INVARIENT, sizeof(invarient_state)/4, 0);   
      for (i = 0; i < sizeof(invarient_state)/4; i++)
	 packet_dword( &packet, invarient_state[i] );

      i915_cache_emit( i915->cctx, &packet );
   }
}

const struct intel_tracked_state i915_upload_invarient = {
   .dirty = {
      .mesa = 0,
      .intel = INTEL_NEW_CONTEXT, /* or less frequently? */
      .extra = 0
   },
   .update = upload_invarient
};



