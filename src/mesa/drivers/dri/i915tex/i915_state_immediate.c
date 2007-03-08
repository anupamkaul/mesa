/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
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
  */
 


#include "intel_batchbuffer.h"
#include "intel_regions.h"

#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"

/***********************************************************************
 * S0,S1: Vertex buffer state.  
 */
static void upload_S0S1( struct i915_context *i915 )
{
   struct intel_context *intel = &i915->intel;

   /* INTEL_NEW_VBO */
   if (intel->vb->state.current_vbo) {

      BEGIN_BATCH(3, 0);

      OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		I1_LOAD_S(0) |
		I1_LOAD_S(1) |
		2);

      /* INTEL_NEW_VBO, INTEL_NEW_RELOC */
      OUT_RELOC(intel->vb->state.current_vbo->buffer,
		DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
		DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
		intel->vb->state.hw_vbo_offset);

      /* INTEL_NEW_VERTEX_SIZE */
      OUT_BATCH((intel->vertex_size << 24) |
		(intel->vertex_size << 16));

      ADVANCE_BATCH();   
   }
}

const struct i915_tracked_state i915_upload_S0S1 = {
   .dirty = {
      .mesa = 0,
      .intel = INTEL_NEW_VBO | INTEL_NEW_VERTEX_SIZE | INTEL_NEW_RELOC,
      .indirect = 0
   },
   .update = upload_S0S1
};


/***********************************************************************
 * S2: Vertex format 
 */
static void upload_S2( struct i915_context *i915 )
{
   /* I915_NEW_FRAGPROG */
   i915->state.Ctx[I915_CTXREG_LIS2] = i915->state.fragprog.LIS2;
}

const struct i915_tracked_state i915_upload_S2 = {
   .dirty = {
      .mesa = 0,
      .intel = I915_NEW_FRAGPROG,
      .indirect = 0
   },
   .update = upload_S2
};



/***********************************************************************
 * S4: Vertex format, rasterization state
 */
static void upload_S4(struct brw_context *brw)
{

   /* I915_NEW_FRAGPROG */
   i915->state.Ctx[I915_CTXREG_LIS4] |= i915->state.fragprog.LIS4;


   /* _NEW_POLYGON, _NEW_BUFFERS */
   {
      GLuint mode;

      if (!ctx->Polygon.CullFlag) {
	 mode = S4_CULLMODE_NONE;
      }
      else if (ctx->Polygon.CullFaceMode != GL_FRONT_AND_BACK) {
	 mode = S4_CULLMODE_CW;
	 
	 if (ctx->DrawBuffer && ctx->DrawBuffer->Name != 0)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
	 if (ctx->Polygon.CullFaceMode == GL_FRONT)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
	 if (ctx->Polygon.FrontFace != GL_CCW)
	    mode ^= (S4_CULLMODE_CW ^ S4_CULLMODE_CCW);
      }
      else {
	 mode = S4_CULLMODE_BOTH;
      }
      i915->state.Ctx[I915_CTXREG_LIS4] |= mode;
   }


   /* _NEW_LINE */
   {
      GLint width = (GLint) (ctx->Line.Width * 2);

      CLAMP_SELF(width, 1, 0xf);
      i915->state.Ctx[I915_CTXREG_LIS4] |= width << S4_LINE_WIDTH_SHIFT;

      if (ctx->Line.Smooth)
	 i915->state.Ctx[I915_CTXREG_LIS4] |= S4_LINE_ANTIALIAS_ENABLE;
   }

   /* _NEW_POINT */
   {
      GLint point_size = (int) ctx->Point._Size;

      CLAMP_SELF(point_size, 1, 255);
      i915->state.Ctx[I915_CTXREG_LIS4] |= point_size << S4_POINT_WIDTH_SHIFT;
   }

   /* _NEW_LIGHT */
   if (ctx->Light.ShadeModel == GL_FLAT) {
      i915->state.Ctx[I915_CTXREG_LIS4] |= (S4_FLATSHADE_ALPHA |
                                            S4_FLATSHADE_COLOR |
                                            S4_FLATSHADE_SPECULAR);
   }


   I915_SET_STATE_IMMEDIATE(i915, , &bcc);
}


const struct i915_tracked_state i915_upload_S4 = {
   .dirty = {
      .mesa = (_NEW_POLYGON | 
	       _NEW_BUFFERS |
	       _NEW_LINE |
	       _NEW_POINT |
	       _NEW_LIGHT),

      .intel = I915_NEW_FRAGPROG,
      .indirect = 0
   },
   .update = upload_S4
};



/***********************************************************************
 * 
 */
static void upload_S5( struct i915_context *i915 )
{
   /* _NEW_STENCIL */
   if (ctx->Stencil.Enabled) {
      GLint test = intel_translate_compare_func(ctx->Stencil.Function[0]);
      GLint fop = intel_translate_stencil_op(fail);
      GLint dfop = intel_translate_stencil_op(zfail);
      GLint dpop = intel_translate_stencil_op(zpass);
      
      i915->state.Ctx[I915_CTXREG_LIS5] |= (S5_STENCIL_TEST_ENABLE |
					    S5_STENCIL_WRITE_ENABLE);

      i915->state.Ctx[I915_CTXREG_LIS5] |= ((ref << S5_STENCIL_REF_SHIFT) |
					    (test <<
					     S5_STENCIL_TEST_FUNC_SHIFT));



      i915->state.Ctx[I915_CTXREG_LIS5] |= ((fop << S5_STENCIL_FAIL_SHIFT) |
					    (dfop <<
					     S5_STENCIL_PASS_Z_FAIL_SHIFT) |
					    (dpop <<
					     S5_STENCIL_PASS_Z_PASS_SHIFT));

   }

   /* _NEW_COLOR */
   if (RGBA_LOGICOP_ENABLED(ctx)) {
      i915->state.Ctx[I915_CTXREG_LIS5] |= S5_LOGICOP_ENABLE;
   }

   if (ctx->Color.DitherFlag) {
      i915->state.Ctx[I915_CTXREG_LIS5] |= S5_COLOR_DITHER_ENABLE;
   }

   {
      const GLubyte *mask = ctx->Color.ColorMask;

      if (!mask[0])
	 i915->state.Ctx[I915_CTXREG_LIS5] |= S5_WRITEDISABLE_RED;

      if (!mask[1])
	 i915->state.Ctx[I915_CTXREG_LIS5] |= S5_WRITEDISABLE_GREEN;

      if (!mask[2])
	 i915->state.Ctx[I915_CTXREG_LIS5] |= S5_WRITEDISABLE_BLUE;

      if (!mask[3])
	 i915->state.Ctx[I915_CTXREG_LIS5] |= S5_WRITEDISABLE_ALPHA;
   }
}

const struct i915_tracked_state i915_upload_S5 = {
   .dirty = {
      .mesa = (_NEW_STENCIL | _NEW_COLOR),
      .intel = 0,
      .indirect = 0
   },
   .update = upload_S5
};


/***********************************************************************
 */
static void upload_S6( struct i915_context *i915 )
{
   struct i915_context *i915 = I915_CONTEXT(ctx);


   i915->state.Ctx[I915_CTXREG_LIS6] = (S6_COLOR_WRITE_ENABLE |
					(2 << S6_TRISTRIP_PV_SHIFT));

   /* _NEW_COLOR
    */
   if (ctx->Color.AlphaTest) {
      int test = intel_translate_compare_func(ctx->Color.AlphaFunc);
      GLubyte refByte;

      CLAMPED_FLOAT_TO_UBYTE(refByte, ctx->Color.AlphaRef);
      
      i915->state.Ctx[I915_CTXREG_LIS6] |= S6_ALPHA_TEST_ENABLE;

      i915->state.Ctx[I915_CTXREG_LIS6] |= ((test << S6_ALPHA_TEST_FUNC_SHIFT) |
					    (((GLuint) refByte) << S6_ALPHA_REF_SHIFT));
   }

   /* _NEW_COLOR
    */
   if (ctx->Color.BlendEnabled && !RGBA_LOGICOP_ENABLED(ctx)) {

      GLuint eqRGB = ctx->Color.BlendEquationRGB;
      GLuint eqA = ctx->Color.BlendEquationA;
      GLuint srcRGB = ctx->Color.BlendSrcRGB;
      GLuint dstRGB = ctx->Color.BlendDstRGB;
      GLuint srcA = ctx->Color.BlendSrcA;
      GLuint dstA = ctx->Color.BlendDstA;
      
      if (eqRGB == GL_MIN || eqRGB == GL_MAX) {
	 srcRGB = dstRGB = GL_ONE;
      }

      i915->state.Ctx[I915_CTXREG_LIS6] |= S6_CBUF_BLEND_ENABLE;
      
      lis6 |= SRC_BLND_FACT(intel_translate_blend_factor(srcRGB));
      lis6 |= DST_BLND_FACT(intel_translate_blend_factor(dstRGB));
      lis6 |= translate_blend_equation(eqRGB) << S6_CBUF_BLEND_FUNC_SHIFT;
   }

   /* _NEW_DEPTH 
    */
   if (ctx->Depth.Test) {
      GLint func = intel_translate_compare_func(ctx->Depth.Func);

      i915->state.Ctx[I915_CTXREG_LIS6] |= S6_DEPTH_TEST_ENABLE;
      i915->state.Ctx[I915_CTXREG_LIS6] |= func << S6_DEPTH_TEST_FUNC_SHIFT;

      if (ctx->Depth.Mask)
	 i915->state.Ctx[I915_CTXREG_LIS6] |= S6_DEPTH_WRITE_ENABLE;
   }
}

const struct i915_tracked_state i915_upload_S6 = {
   .dirty = {
      .mesa = (_NEW_COLOR | _NEW_DEPTH),
      .intel = 0,
      .indirect = 0
   },
   .update = upload_S6
};

