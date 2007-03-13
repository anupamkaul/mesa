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
 

#include "intel_batchbuffer.h"
#include "intel_regions.h"
#include "intel_state_inlines.h"

#include "macros.h"

#include "i915_context.h"
#include "i915_state.h"
#include "i915_state_inlines.h"
#include "i915_reg.h"


#define STATE_LOGICOP_ENABLED(state) \
  ((state)->Color->ColorLogicOpEnabled || \
   ((state)->Color->BlendEnabled && (state)->Color->BlendEquationRGB == GL_LOGIC_OP))


/***********************************************************************
 * S0,S1: Vertex buffer state.  
 */
static void upload_S0S1( struct intel_context *intel )
{

   /* INTEL_NEW_VBO */
   if (intel->state.vbo) {

      BEGIN_BATCH(3, 0);

      OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		I1_LOAD_S(0) |
		I1_LOAD_S(1) |
		2);

      /* INTEL_NEW_VBO, INTEL_NEW_RELOC */
      OUT_RELOC(intel->state.vbo,
		DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
		DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
		intel->state.vbo_offset);

      /* INTEL_NEW_VERTEX_SIZE */
      OUT_BATCH((intel->vertex_size << 24) |
		(intel->vertex_size << 16));

      ADVANCE_BATCH();   
   }
}

const struct intel_tracked_state i915_upload_S0S1 = {
   .dirty = {
      .mesa = 0,
      .intel = INTEL_NEW_VBO | INTEL_NEW_VERTEX_SIZE | INTEL_NEW_FENCE,
      .extra = 0
   },
   .update = upload_S0S1
};





/***********************************************************************
 * S4: Vertex format, rasterization state
 */
static void upload_S2S4(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint LIS2, LIS4;
   
   /* I915_NEW_VERTEX_FORMAT */
   LIS2 = i915->fragprog.LIS2;
   LIS4 = i915->fragprog.LIS4;


   /* _NEW_POLYGON, _NEW_BUFFERS */
   {
      GLuint mode;

      if (!intel->state.Polygon->CullFlag) {
	 mode = S4_CULLMODE_NONE;
      }
      else if (intel->state.Polygon->CullFaceMode != GL_FRONT_AND_BACK) {
	 mode = S4_CULLMODE_CW;
	 
	 if (intel->state.DrawBuffer && intel->state.DrawBuffer->Name != 0)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
	 if (intel->state.Polygon->CullFaceMode == GL_FRONT)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
	 if (intel->state.Polygon->FrontFace != GL_CCW)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
      }
      else {
	 mode = S4_CULLMODE_BOTH;
      }

      LIS4 |= mode;
   }


   /* _NEW_LINE */
   {
      GLint width = (GLint) (intel->state.Line->Width * 2);

      CLAMP_SELF(width, 1, 0xf);
      LIS4 |= width << S4_LINE_WIDTH_SHIFT;

      if (intel->state.Line->SmoothFlag)
	 LIS4 |= S4_LINE_ANTIALIAS_ENABLE;
   }

   /* _NEW_POINT */
   {
      GLint point_size = (int) intel->state.Point->_Size;

      CLAMP_SELF(point_size, 1, 255);
      LIS4 |= point_size << S4_POINT_WIDTH_SHIFT;
   }

   /* _NEW_LIGHT */
   if (intel->state.Light->ShadeModel == GL_FLAT) {
      LIS4 |= (S4_FLATSHADE_ALPHA |
	       S4_FLATSHADE_COLOR |
	       S4_FLATSHADE_SPECULAR);
   }

   
   BEGIN_BATCH(3, 0);
   
   OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	     I1_LOAD_S(2) |
	     I1_LOAD_S(4) |
	     2);
   OUT_BATCH(LIS2);
   OUT_BATCH(LIS4);
   ADVANCE_BATCH();
}


const struct intel_tracked_state i915_upload_S2S4 = {
   .dirty = {
      .mesa = (_NEW_POLYGON | 
	       _NEW_BUFFERS |
	       _NEW_LINE |
	       _NEW_POINT |
	       _NEW_LIGHT),

      .intel = I915_NEW_VERTEX_FORMAT,
      .extra = 0
   },
   .update = upload_S2S4
};



/***********************************************************************
 * 
 */
static void upload_S5( struct intel_context *intel )
{
   GLuint LIS5 = 0;

   /* _NEW_STENCIL */
   if (intel->state.Stencil->Enabled) {
      GLint test = intel_translate_compare_func(intel->state.Stencil->Function[0]);
      GLint fop = intel_translate_stencil_op(intel->state.Stencil->FailFunc[0]);
      GLint dfop = intel_translate_stencil_op(intel->state.Stencil->ZFailFunc[0]);
      GLint dpop = intel_translate_stencil_op(intel->state.Stencil->ZPassFunc[0]);
      GLint ref = intel->state.Stencil->Ref[0] & 0xff;
      
      LIS5 |= (S5_STENCIL_TEST_ENABLE |
	       S5_STENCIL_WRITE_ENABLE |
	       (ref  << S5_STENCIL_REF_SHIFT) |
	       (test << S5_STENCIL_TEST_FUNC_SHIFT) |
	       (fop  << S5_STENCIL_FAIL_SHIFT) |
	       (dfop << S5_STENCIL_PASS_Z_FAIL_SHIFT) |
	       (dpop << S5_STENCIL_PASS_Z_PASS_SHIFT));
   }

   /* _NEW_COLOR */
   if (STATE_LOGICOP_ENABLED(&intel->state)) {
      LIS5 |= S5_LOGICOP_ENABLE;
   }

   if (intel->state.Color->DitherFlag) {
      LIS5 |= S5_COLOR_DITHER_ENABLE;
   }

   {
      const GLubyte *mask = intel->state.Color->ColorMask;

      if (!mask[0])
	 LIS5 |= S5_WRITEDISABLE_RED;

      if (!mask[1])
	 LIS5 |= S5_WRITEDISABLE_GREEN;

      if (!mask[2])
	 LIS5 |= S5_WRITEDISABLE_BLUE;

      if (!mask[3])
	 LIS5 |= S5_WRITEDISABLE_ALPHA;
   }

   BEGIN_BATCH(2, 0);   
   OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	     I1_LOAD_S(5) |
	     1);
   OUT_BATCH(LIS5);
   ADVANCE_BATCH();

}

const struct intel_tracked_state i915_upload_S5 = {
   .dirty = {
      .mesa = (_NEW_STENCIL | _NEW_COLOR),
      .intel = 0,
      .extra = 0
   },
   .update = upload_S5
};


/***********************************************************************
 */
static void upload_S6( struct intel_context *intel )
{
   GLuint LIS6 = (S6_COLOR_WRITE_ENABLE |
		  (2 << S6_TRISTRIP_PV_SHIFT));

   /* _NEW_COLOR
    */
   if (intel->state.Color->AlphaEnabled) {
      int test = intel_translate_compare_func(intel->state.Color->AlphaFunc);
      GLubyte refByte;

      CLAMPED_FLOAT_TO_UBYTE(refByte, intel->state.Color->AlphaRef);
      
      LIS6 |= S6_ALPHA_TEST_ENABLE;

      LIS6 |= ((test << S6_ALPHA_TEST_FUNC_SHIFT) |
	       (((GLuint) refByte) << S6_ALPHA_REF_SHIFT));
   }

   /* _NEW_COLOR
    */
   if (intel->state.Color->BlendEnabled && 
       !STATE_LOGICOP_ENABLED(&intel->state)) {

      GLuint eqRGB = intel->state.Color->BlendEquationRGB;
      GLuint srcRGB = intel->state.Color->BlendSrcRGB;
      GLuint dstRGB = intel->state.Color->BlendDstRGB;
      
      if (eqRGB == GL_MIN || eqRGB == GL_MAX) {
	 srcRGB = dstRGB = GL_ONE;
      }

      LIS6 |= (S6_CBUF_BLEND_ENABLE |
	       SRC_BLND_FACT(intel_translate_blend_factor(srcRGB)) |
	       DST_BLND_FACT(intel_translate_blend_factor(dstRGB)) |
	       (i915_translate_blend_equation(eqRGB) << S6_CBUF_BLEND_FUNC_SHIFT));
   }

   /* _NEW_DEPTH 
    */
   if (intel->state.Depth->Test) {
      GLint func = intel_translate_compare_func(intel->state.Depth->Func);

      LIS6 |= S6_DEPTH_TEST_ENABLE;
      LIS6 |= func << S6_DEPTH_TEST_FUNC_SHIFT;

      if (intel->state.Depth->Mask)
	 LIS6 |= S6_DEPTH_WRITE_ENABLE;
   }

   BEGIN_BATCH(2, 0);   
   OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	     I1_LOAD_S(6) |
	     1);
   OUT_BATCH(LIS6);
   ADVANCE_BATCH();

}

const struct intel_tracked_state i915_upload_S6 = {
   .dirty = {
      .mesa = (_NEW_COLOR | _NEW_DEPTH),
      .intel = 0,
      .extra = 0
   },
   .update = upload_S6
};

