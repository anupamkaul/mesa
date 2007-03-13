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
#include "intel_batchbuffer.h"
#include "intel_regions.h"
#include "intel_state_inlines.h"

#include "i915_context.h"
#include "i915_reg.h"
#include "i915_state.h"
#include "i915_state_inlines.h"

#define FILE_DEBUG_FLAG DEBUG_STATE

/***********************************************************************
 * Modes4: stencil masks and logicop 
 */
static void upload_MODES4( struct intel_context *intel )
{
   GLuint modes4 = _3DSTATE_MODES_4_CMD;

   /* _NEW_STENCIL */
   if (intel->state.Stencil->Enabled) {
      GLint testmask = intel->state.Stencil->ValueMask[0] & 0xff;
      GLint writemask = intel->state.Stencil->WriteMask[0] & 0xff;

      modes4 |= (ENABLE_STENCIL_TEST_MASK |
		 STENCIL_TEST_MASK(testmask) |
		 ENABLE_STENCIL_WRITE_MASK |
		 STENCIL_WRITE_MASK(writemask));
   }

   /* _NEW_COLOR */
   if (intel->state.Color->_LogicOpEnabled)
   {
      modes4 |= (ENABLE_LOGIC_OP_FUNC |
		 LOGIC_OP_FUNC(intel_translate_logic_op(intel->state.Color->LogicOp)));
   }
   else {
      /* This seems to be the only way to turn off logicop.  The
       * ENABLE_LOGIC_OP_FUNC is just a modify-enable bit to say this
       * field is present in the instruction.
       */
      modes4 |= (ENABLE_LOGIC_OP_FUNC |
		 LOGICOP_COPY);
   }
   
   /* This needs to be sent in all states (subject eventually to
    * caching to avoid duplicate emits, and later to indirect state
    * management)
    */
   BEGIN_BATCH(1,0);
   OUT_BATCH(modes4);
   ADVANCE_BATCH();
}

const struct intel_tracked_state i915_upload_MODES4 = {
   .dirty = {
      .mesa = _NEW_STENCIL | _NEW_COLOR,
      .intel = 0,
      .extra = 0
   },
   .update = upload_MODES4
};


/***********************************************************************
 * BFO:  Backface stencil
 */

static void upload_BFO( struct intel_context *intel )
{
   GLuint bfo;

   /* _NEW_STENCIL 
    */
   if (intel->state.Stencil->Enabled) {
      GLint test = intel_translate_compare_func(intel->state.Stencil->Function[1]);
      GLint fop = intel_translate_stencil_op(intel->state.Stencil->FailFunc[1]);
      GLint dfop = intel_translate_stencil_op(intel->state.Stencil->ZFailFunc[1]);
      GLint dpop = intel_translate_stencil_op(intel->state.Stencil->ZPassFunc[1]);
      GLint ref = intel->state.Stencil->Ref[1] & 0xff;
      
      bfo = (_3DSTATE_BACKFACE_STENCIL_OPS |
	     BFO_ENABLE_STENCIL_FUNCS |
	     BFO_ENABLE_STENCIL_TWO_SIDE |
	     BFO_ENABLE_STENCIL_REF |
	     BFO_STENCIL_TWO_SIDE |
	     (ref  << BFO_STENCIL_REF_SHIFT) |
	     (test << BFO_STENCIL_TEST_SHIFT) |
	     (fop  << BFO_STENCIL_FAIL_SHIFT) |
	     (dfop << BFO_STENCIL_PASS_Z_FAIL_SHIFT) |
	     (dpop << BFO_STENCIL_PASS_Z_PASS_SHIFT));
   }
   else {
      /* This actually disables two-side stencil: The bit set is a
       * modify-enable bit to indicate we are changing the two-side
       * setting.  Then there is a symbolic zero to show that we are
       * setting the flag to zero/off.
       */
      bfo = (_3DSTATE_BACKFACE_STENCIL_OPS |
	     BFO_ENABLE_STENCIL_TWO_SIDE |
	     0);
   }      

   BEGIN_BATCH(1,0);
   OUT_BATCH(bfo);
   ADVANCE_BATCH();
}

const struct intel_tracked_state i915_upload_BFO = {
   .dirty = {
      .mesa = _NEW_STENCIL,
      .intel = 0,
      .extra = 0
   },
   .update = upload_BFO
};


/***********************************************************************
 * BLENDCOLOR
 */
static void upload_BLENDCOLOR( struct intel_context *intel )
{
   /* _NEW_COLOR 
    */
   if (intel->state.Color->BlendEnabled) {
      const GLfloat *color = intel->state.Color->BlendColor;
      GLubyte r, g, b, a;

      UNCLAMPED_FLOAT_TO_UBYTE(r, color[RCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(g, color[GCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(b, color[BCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(a, color[ACOMP]);

      /* Only needs to be sent when blend is enabled
       */
      BEGIN_BATCH(2, 0);
      OUT_BATCH(_3DSTATE_CONST_BLEND_COLOR_CMD);
      OUT_BATCH( (a << 24) | (r << 16) | (g << 8) | b );
      ADVANCE_BATCH();
   }
}

const struct intel_tracked_state i915_upload_BLENDCOLOR = {
   .dirty = {
      .mesa = _NEW_COLOR,
      .intel = 0,
      .extra = 0
   },
   .update = upload_BLENDCOLOR
};


/***********************************************************************
 * IAB:  Independent Alpha Blend
 */
static void upload_IAB( struct intel_context *intel )
{
   if (intel->state.Color->BlendEnabled) {      
      GLuint iab = (_3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD | 
		    IAB_MODIFY_ENABLE |
		    0);

      GLuint eqRGB = intel->state.Color->BlendEquationRGB;
      GLuint eqA = intel->state.Color->BlendEquationA;
      GLuint srcRGB = intel->state.Color->BlendSrcRGB;
      GLuint dstRGB = intel->state.Color->BlendDstRGB;
      GLuint srcA = intel->state.Color->BlendSrcA;
      GLuint dstA = intel->state.Color->BlendDstA;
      
      if (srcA != srcRGB ||
	  dstA != dstRGB ||
	  eqA != eqRGB) {

	 if (eqA == GL_MIN || eqA == GL_MAX) {
	    srcA = dstA = GL_ONE;
	 }

	 iab |= (IAB_MODIFY_FUNC | 
		 IAB_MODIFY_SRC_FACTOR | 
		 IAB_MODIFY_DST_FACTOR |
		 SRC_ABLND_FACT(intel_translate_blend_factor(srcA)) |
		 DST_ABLND_FACT(intel_translate_blend_factor(dstA)) |
		 (i915_translate_blend_equation(eqA) << IAB_FUNC_SHIFT) |
		 IAB_ENABLE );
      }
	 
      BEGIN_BATCH(1, 0);
      OUT_BATCH( iab );
      ADVANCE_BATCH();
   }
}

const struct intel_tracked_state i915_upload_IAB = {
   .dirty = {
      .mesa = _NEW_COLOR,
      .intel = 0,
      .extra = 0
   },
   .update = upload_IAB
};


/***********************************************************************
 * Depthbuffer - currently constant, but rotation would change that.
 */


static void upload_buffers(struct intel_context *intel)
{
   struct intel_region *color_region = intel->state.draw_region;
   struct intel_region *depth_region = intel->state.depth_region;

   if (color_region) {
      BEGIN_BATCH(4, 0);
      OUT_BATCH( _3DSTATE_BUF_INFO_CMD );
      OUT_BATCH( BUF_3D_ID_COLOR_BACK |
		 BUF_3D_PITCH(color_region->pitch * color_region->cpp) |
		 BUF_3D_USE_FENCE);
      OUT_RELOC(intel->state.draw_region->buffer,
                DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                intel->state.draw_region->draw_offset);
      ADVANCE_BATCH();
   }

   if (depth_region) {
      BEGIN_BATCH(4, 0);
      OUT_BATCH( _3DSTATE_BUF_INFO_CMD );
      OUT_BATCH( BUF_3D_ID_DEPTH |
		 BUF_3D_PITCH(depth_region->pitch * depth_region->cpp) |
		 BUF_3D_USE_FENCE );
      OUT_RELOC(intel->state.depth_region->buffer,
		DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
		DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
		intel->state.depth_region->draw_offset);
      ADVANCE_BATCH();
   }

   /*
    * Compute/set I915_DESTREG_DV1 value
    */
   BEGIN_BATCH(2, 0);
   OUT_BATCH(_3DSTATE_DST_BUF_VARS_CMD);

   OUT_BATCH( DSTORG_HORT_BIAS(0x8) |     /* .5 */
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

   ADVANCE_BATCH();
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

   BEGIN_BATCH(2, 0);
   OUT_BATCH(st0);
   OUT_BATCH(st1);
   ADVANCE_BATCH();
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

      BEGIN_BATCH(4, 0);
      OUT_BATCH(_3DSTATE_SCISSOR_ENABLE_CMD | ENABLE_SCISSOR_RECT);
      OUT_BATCH(_3DSTATE_SCISSOR_RECT_0_CMD);
      OUT_BATCH((y1 << 16) | (x1 & 0xffff));
      OUT_BATCH((y2 << 16) | (x2 & 0xffff));
      ADVANCE_BATCH();
   }
   else {
      BEGIN_BATCH(1, 0);
      OUT_BATCH(_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);
      ADVANCE_BATCH();
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

