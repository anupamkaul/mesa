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

#include "intel_batchbuffer.h"
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


static GLuint invarient_state[] = {

   (_3DSTATE_AA_CMD |
    AA_LINE_ECAAR_WIDTH_ENABLE |
    AA_LINE_ECAAR_WIDTH_1_0 |
    AA_LINE_REGION_WIDTH_ENABLE | 
    AA_LINE_REGION_WIDTH_1_0),

#if 1
   /* Could use these to reduce the size of vertices when the
    * incoming array is constant.  For now these are don't care
    * items - maybe don't bother about setting them??
    */
   (_3DSTATE_DFLT_DIFFUSE_CMD),
   (0),

   (_3DSTATE_DFLT_SPEC_CMD),
   (0),

   (_3DSTATE_DFLT_Z_CMD),
   (0),
#endif

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


   /* For private depth buffers but shared color buffers, eg
    * front-buffer rendering with a private depthbuffer.  We don't do
    * this.
    */
   (_3DSTATE_DEPTH_SUBRECT_DISABLE),
};




/* 
 */
static void upload_static(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct intel_region *color_region = intel->state.draw_region;
   struct intel_region *depth_region = intel->state.depth_region;
   struct i915_cache_packet packet;
   GLboolean scissor = (intel->state.Scissor->Enabled && 
			intel->state.DrawBuffer);
   GLuint clearparams = intel->state.clearparams;
   GLuint i;

   GLuint dwords = ((color_region ? 3 : 0) + 
		    (depth_region ? 3 : 0) + 
		    2 +		/* DV */
		    (scissor      ? 4 : 1) +
		    (clearparams ? 14 : 0) +
		    Elements(invarient_state));

   GLuint relocs = ((color_region ? 1 : 0) + 
		    (depth_region ? 1 : 0));
   

   packet_init( &packet, I915_CACHE_STATIC, dwords, relocs );
   
   /***********************************************************************
    * Misc invarient state packets
    */
   for (i = 0; i < Elements(invarient_state); i++)
      packet_dword( &packet, invarient_state[i] );



   /***********************************************************************
    * Buffers
    */
   if (color_region) {
      packet_dword( &packet, _3DSTATE_BUF_INFO_CMD );
      packet_dword( &packet, BUF_3D_ID_COLOR_BACK |
		    BUF_3D_PITCH(color_region->pitch * color_region->cpp) |
/* 		    BUF_3D_TILED_SURFACE | */
/* 		    BUF_3D_TILE_WALK_X | */
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
/* 		    BUF_3D_TILED_SURFACE | */
/* 		    BUF_3D_TILE_WALK_X | */
		    BUF_3D_USE_FENCE );
      packet_reloc( &packet, depth_region->buffer,
		    DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
		    DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
		    depth_region->draw_offset);
   }


   /* This might become dynamic state if it turns out adjusting the
    * bias values can cure our point-rendering woes.
    */
   packet_dword( &packet,_3DSTATE_DST_BUF_VARS_CMD);
   packet_dword( &packet, (DSTORG_HORT_BIAS(0x8) |     /* .5 */
			   DSTORG_VERT_BIAS(0x8) |     /* .5 */
			   LOD_PRECLAMP_OGL |
			   TEX_DEFAULT_COLOR_OGL | 
			   DITHER_FULL_ALWAYS |
			   (color_region && color_region->cpp == 4 
			    ? DV_PF_8888
			    : DV_PF_565) |
			   (depth_region && depth_region->cpp == 4 
			    ? DEPTH_FRMT_24_FIXED_8_OTHER
			    : DEPTH_FRMT_16_FIXED)) );


   /***********************************************************************
    * Scissor.  
    *
    * Is it static or dynamic???  It is not understood by the hardware
    * binner, so if we ever implement HWZ, it would be static under that
    * scheme, or somehow not handled, or perhaps we would have to
    * manually clip primitives to the scissor region.  For now, we call
    * it static.
    */

   /* _NEW_SCISSOR, _NEW_BUFFERS 
    */
   if (scissor) {
      
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

      packet_dword( &packet,_3DSTATE_SCISSOR_ENABLE_CMD | ENABLE_SCISSOR_RECT);
      packet_dword( &packet,_3DSTATE_SCISSOR_RECT_0_CMD);
      packet_dword( &packet,(y1 << 16) | (x1 & 0xffff));
      packet_dword( &packet,(y2 << 16) | (x2 & 0xffff));
   }
   else {
      packet_dword( &packet,_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);
   }


   /* INTEL_NEW_CLEAR_PARAMS, _NEW_DEPTH
    */
   if (clearparams) {
      GLuint clearColor = 0;
      GLuint clearDepth = 0;
      GLuint clearStencil = 0;
      GLuint statemask = 0;

      if (color_region && (clearparams & (BUFFER_BIT_BACK_LEFT |
					  BUFFER_BIT_FRONT_LEFT |
					  BUFFER_BIT_COLOR0))) {
	 statemask |= CLEARPARAM_WRITE_COLOR;
	 if (color_region->cpp == 4) 
	    clearColor = intel->ClearColor8888;
	 else
	    clearColor = (intel->ClearColor565 << 16) | intel->ClearColor565;
      }

      if (depth_region && (clearparams & (BUFFER_BIT_STENCIL | 
					  BUFFER_BIT_DEPTH))) {
	 if (clearparams & BUFFER_BIT_DEPTH) {
	    statemask |= CLEARPARAM_WRITE_DEPTH;
	    clearDepth = intel->ctx.Depth.Clear * 0xffffffff;
	 }

	 if (depth_region->cpp == 4) {
	    clearDepth &= 0xffffff00;
	    if (clearparams & BUFFER_BIT_STENCIL) {
	       statemask |= CLEARPARAM_WRITE_STENCIL;
	       clearStencil = STENCIL_WRITE_MASK(intel->ctx.Stencil.Clear);
	       clearDepth |= clearStencil;
	    }
	 } 
	 else {
	    clearDepth = (clearDepth & 0xffff0000) | clearDepth >> 16;
	 }
      }

      packet_dword( &packet, _3DSTATE_CLEAR_PARAMETERS );
      packet_dword( &packet, statemask | CLEARPARAM_CLEAR_RECT );
      packet_dword( &packet, clearColor );
      packet_dword( &packet, clearDepth );
      packet_dword( &packet, intel->ClearColor8888 );
      packet_float( &packet, intel->ctx.Depth.Clear );
      packet_dword( &packet, clearStencil );

      packet_dword( &packet, _3DSTATE_CLEAR_PARAMETERS );
      packet_dword( &packet, statemask | CLEARPARAM_ZONE_INIT );
      packet_dword( &packet, clearColor );
      packet_dword( &packet, clearDepth );
      packet_dword( &packet, intel->ClearColor8888 );
      packet_float( &packet, intel->ctx.Depth.Clear );
      packet_dword( &packet, clearStencil );
   }

   i915_cache_emit( i915->cctx, &packet );
}


const struct intel_tracked_state i915_upload_static = {
   .dirty = {
      .mesa = _NEW_SCISSOR | _NEW_BUFFERS | _NEW_COLOR | _NEW_DEPTH | _NEW_STENCIL,
      .intel = INTEL_NEW_CBUF | INTEL_NEW_ZBUF | INTEL_NEW_FENCE | INTEL_NEW_CLEAR_PARAMS,
      .extra = 0
   },
   .update = upload_static
};

