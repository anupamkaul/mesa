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
#include "dd.h"
#include "tnl/tnl.h"
#include "tnl/t_context.h"

#include "texmem.h"

#include "intel_fbo.h"
#include "intel_screen.h"
#include "intel_batchbuffer.h"

#include "i915_context.h"
#include "i915_reg.h"

#define FILE_DEBUG_FLAG DEBUG_STATE

/***********************************************************************
 * Modes4: stencil masks and logicop 
 */
static void upload_MODES4( struct i915_context *i915 )
{
   i915->state.Ctx[I915_CTXREG_MODES4] = _3DSTATE_MODES_4_CMD;

   /* _NEW_STENCIL */
   if (ctx->Stencil.Enabled) {
      GLint testmask = ctx->Stencil.ValueMask[0] & 0xff;
      GLint writemask = ctx->Stencil.WriteMask[0] & 0xff;

      i915->state.Ctx[I915_CTXREG_MODES4] |= (ENABLE_STENCIL_TEST_MASK |
					      STENCIL_TEST_MASK(mask) |
					      ENABLE_STENCIL_WRITE_MASK |
					      STENCIL_WRITE_MASK(writemask));
   }

   /* _NEW_COLOR */
   if (ctx->Color._LogicOpEnabled)
   {
      int tmp = intel_translate_logic_op(ctx->Color.LogicOp);

      i915->state.Ctx[I915_CTXREG_MODES4] |= (ENABLE_LOGIC_OP_FUNC |
					      LOGIC_OP_FUNC(tmp));
   }
   else {
      /* This seems to be the only way to turn off logicop.  The
       * ENABLE_LOGIC_OP_FUNC is just a modify-enable bit to say this
       * field is present in the instruction.
       */
      i915->state.Ctx[I915_CTXREG_MODES4] |= (ENABLE_LOGIC_OP_FUNC |
					      LOGICOP_COPY);
   }
}

const struct i915_tracked_state i915_upload_MODES4 = {
   .dirty = {
      .mesa = _NEW_STENCIL | _NEW_COLOR,
      .intel = 0,
      .indirect = 0
   },
   .update = upload_MODES4
};


/***********************************************************************
 * BFO:  Backface stencil
 */

static void upload_BFO( struct i915_context *i915 )
{
   i915->state.Ctx[I915_CTXREG_BFO] = _3DSTATE_BACKFACE_STENCIL_OPS;

   /* _NEW_STENCIL 
    */
   if (ctx->Stencil.Enabled) {
      GLint test = intel_translate_compare_func(ctx->Stencil.Function[1]);
      GLint fop = intel_translate_stencil_op(fail);
      GLint dfop = intel_translate_stencil_op(zfail);
      GLint dpop = intel_translate_stencil_op(zpass);
      
      i915->state.Ctx[I915_CTXREG_BFO] |= 
	 (BFO_ENABLE_STENCIL_FUNCS |
	  BFO_ENABLE_STENCIL_TWO_SIDE |
	  BFO_ENABLE_STENCIL_REF |
	  BFO_STENCIL_TWO_SIDE |
	  (ref  << BFO_STENCIL_REF_SHIFT) |
	  (test << BFO_STENCIL_TEST_FUNC_SHIFT) |
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
      i915->state.Ctx[I915_CTXREG_BFO] |= (BFO_ENABLE_STENCIL_TWO_SIDE |
					   0);
   }      
}

const struct i915_tracked_state i915_upload_BFO = {
   .dirty = {
      .mesa = _NEW_STENCIL,
      .intel = 0,
      .indirect = 0
   },
   .update = upload_MODES4
};


/***********************************************************************
 * BLENDCOLOR
 */
static void upload_BLENDCOLOR( struct i915_context *i915 )
{
   struct i915_context *i915 = I915_CONTEXT(ctx);

   /* _NEW_COLOR 
    */
   if (ctx->Color.BlendEnabled) {
      const GLfloat *color = ctx->Color.BlendColor;
      GLubyte r, g, b, a;

      UNCLAMPED_FLOAT_TO_UBYTE(r, color[RCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(g, color[GCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(b, color[BCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(a, color[ACOMP]);

      BEGIN_BATCH(2, 0);
      OUT_BATCH(_3DSTATE_CONST_BLEND_COLOR_CMD);
      OUT_BATCH( (a << 24) | (r << 16) | (g << 8) | b );
      ADVANCE_BATCH();
   }
}

const struct i915_tracked_state i915_upload_BLENDCOLOR = {
   .dirty = {
      .mesa = _NEW_COLOR,
      .i915 = 0,
      .cache = 0
   },
   .update = upload_BLENDCOLOR
};


/***********************************************************************
 * IAB:  Independent Alpha Blend
 */
static void upload_IAB( struct i915_context *i915 )
{
   struct i915_context *i915 = I915_CONTEXT(ctx);


   i915->state.Ctx[I915_CTXREG_IAB] =


   if (ctx->Color.BlendEnabled) {

      GLuint eqRGB = ctx->Color.BlendEquationRGB;
      GLuint eqA = ctx->Color.BlendEquationA;
      GLuint srcRGB = ctx->Color.BlendSrcRGB;
      GLuint dstRGB = ctx->Color.BlendDstRGB;
      GLuint srcA = ctx->Color.BlendSrcA;
      GLuint dstA = ctx->Color.BlendDstA;
      
      if (srcA != srcRGB ||
	  dstA != dstRGB ||
	  eqA != eqRGB) {

	 if (eqA == GL_MIN || eqA == GL_MAX) {
	    srcA = dstA = GL_ONE;
	 }

	 BEGIN_BATCH(1, 0);

	 OUT_BATCH(  _3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD | 
		     IAB_MODIFY_ENABLE |
		     IAB_MODIFY_FUNC | 
		     IAB_MODIFY_SRC_FACTOR | 
		     IAB_MODIFY_DST_FACTOR |
		     SRC_ABLND_FACT(intel_translate_blend_factor(srcA)) |
		     DST_ABLND_FACT(intel_translate_blend_factor(dstA)) |
		     (translate_blend_equation(eqA) << IAB_FUNC_SHIFT) |
		     IAB_ENABLE );

	 ADVANCE_BATCH();
      }
   }
}

const struct i915_tracked_state i915_upload_IAB = {
   .dirty = {
      .mesa = _NEW_COLOR,
      .i915 = 0,
      .cache = 0
   },
   .update = upload_IAB
};


/***********************************************************************
 * Depthbuffer - currently constant, but rotation would change that.
 */

static void emit_buffers( struct i915_context *i915 )
{
      if (INTEL_DEBUG & DEBUG_STATE)
         fprintf(stderr, "I915_UPLOAD_BUFFERS:\n");
      BEGIN_BATCH(I915_DEST_SETUP_SIZE + 2, 0);
      OUT_BATCH(state->Buffer[I915_DESTREG_CBUFADDR0]);
      OUT_BATCH(state->Buffer[I915_DESTREG_CBUFADDR1]);
      OUT_RELOC(state->draw_region->buffer,
                DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                state->draw_region->draw_offset);

      if (state->depth_region) {
         OUT_BATCH(state->Buffer[I915_DESTREG_DBUFADDR0]);
         OUT_BATCH(state->Buffer[I915_DESTREG_DBUFADDR1]);
      }

      OUT_BATCH(state->Buffer[I915_DESTREG_DV1]);
      ADVANCE_BATCH();
}

static void upload_buffers(struct i915_context *i915)
{
   struct intel_region *color_region = state->draw_region;
   struct intel_region *depth_region = state->depth_region;
   GLuint value;

   if (color_region) {
      BEGIN_BATCH(4, 0);
      OUT_BATCH( _3DSTATE_BUF_INFO_CMD );
      OUT_BATCH( BUF_3D_ID_COLOR_BACK |
		 BUF_3D_PITCH(color_region->pitch * color_region->cpp) |
		 BUF_3D_USE_FENCE);
      OUT_RELOC(state->draw_region->buffer,
                DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                state->draw_region->draw_offset);
      ADVANCE_BATCH();
   }

   if (depth_region) {
      BEGIN_BATCH(4, 0);
      OUT_BATCH( _3DSTATE_BUF_INFO_CMD );
      OUT_BATCH( BUF_3D_ID_DEPTH |
		 BUF_3D_PITCH(depth_region->pitch * depth_region->cpp) |
		 BUF_3D_USE_FENCE );
      OUT_RELOC(state->depth_region->buffer,
		DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
		DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
		state->depth_region->draw_offset);
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

const struct i915_tracked_state i915_upload_buffers = {
   .dirty = {
      .mesa = 0,
      .i915 = I915_NEW_CBUF | I915_NEW_ZBUF,
      .cache = 0
   },
   .update = upload_buffers
};



/***********************************************************************
 * Polygon stipple
 *
 * The i915 supports a 4x4 stipple natively, GL wants 32x32.
 * Fortunately stipple is usually a repeating pattern.
 */
static void upload_stipple( struct i915_context *i915 )
{
   struct i915_context *i915 = I915_CONTEXT(ctx);
   const GLubyte *m = mask;
   GLubyte p[4];
   int i, j, k;

   i915->state.Stipple[I915_STPREG_ST1] = 0;
   
   if  (ctx->Polygon.StippleFlag &&
	i915->intel.reduced_primitive == GL_TRIANGLES) 
   {

   GLuint newMask;

   p[0] = mask[12] & 0xf;
   p[0] |= p[0] << 4;
   p[1] = mask[8] & 0xf;
   p[1] |= p[1] << 4;
   p[2] = mask[4] & 0xf;
   p[2] |= p[2] << 4;
   p[3] = mask[0] & 0xf;
   p[3] |= p[3] << 4;

   for (k = 0; k < 8; k++)
      for (j = 3; j >= 0; j--)
         for (i = 0; i < 4; i++, m++)
            if (*m != p[j]) {
               i915->intel.hw_stipple = 0;
               return;
            }

   newMask = (((p[0] & 0xf) << 0) |
              ((p[1] & 0xf) << 4) |
              ((p[2] & 0xf) << 8) | ((p[3] & 0xf) << 12));


   if (newMask == 0xffff || newMask == 0x0) {
      /* this is needed to make conform pass */
      i915->intel.hw_stipple = 0;
      return;
   }

      i915->state.Stipple[I915_STPREG_ST0] = _3DSTATE_STIPPLE;   if (active) {
      I915_STATECHANGE(i915, I915_UPLOAD_STIPPLE);
      i915->state.Stipple[I915_STPREG_ST1] &= ~ST1_ENABLE;
   }


   i915->state.Stipple[I915_STPREG_ST1] &= ~0xffff;
   i915->state.Stipple[I915_STPREG_ST1] |= newMask;
   i915->intel.hw_stipple = 1;

   if (active)
      i915->state.Stipple[I915_STPREG_ST1] |= ST1_ENABLE;
}


const struct i915_tracked_state i915_polygon_stipple = {
   .dirty = {
      .mesa = _NEW_POLYGONSTIPPLE,
      .i915 = 0,
      .cache = 0
   },
   .update = upload_polygon_stipple
};



/***********************************************************************
 * Misc invarient state packets
 */

static void upload_invarient_state( struct i915_context *i915 )
{
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
       (1)),
      (0),

      /* For private depth buffers but shared color buffers, eg
       * front-buffer rendering with a private depthbuffer.  We don't do
       * this.
       */
      (_3DSTATE_DEPTH_SUBRECT_DISABLE),

      /* Disable indirect state for now.
       */
      (_3DSTATE_LOAD_INDIRECT | 0),
      (0),
   };


   BATCH_LOCALS;

   BEGIN_BATCH(sizeof(invarient_state)/4, 0);
   
   for (i = 0; i < sizeof(invarient_state)/4; i++)
      OUT_BATCH( invarient_state[i] );

   ADVANCE_BATCH();
}

const struct i915_tracked_state i915_invarient_state = {
   .dirty = {
      .mesa = 0,
      .i915 = I915_NEW_CONTEXT,
      .cache = 0
   },
   .update = upload_invarient_state
};





static void check_fallback()
{
   if (ctx->Color._LogicOpEnabled && i915->intel.intelScreen->cpp == 2)    /* XXX FBO fix */
      fallback = 1;

   if (ctx->Stencil.Enabled) {
      if (!ctx->DrawBuffer)
	 return GL_TRUE;

      struct intel_renderbuffer *irbStencil
	 = intel_get_renderbuffer(ctx->DrawBuffer, BUFFER_STENCIL);

      if (!irbStencil || !irbStencil->region)
	 return GL_TRUE;
   }
	    
	    
}


static void update_scissor()
{
   struct i915_context *i915 = I915_CONTEXT(ctx);

   if (ctx->Scissor.Enabled && ctx->DrawBuffer) {
      int x1, y1, x2, y2;

      DBG("%s %d,%d %dx%d\n", __FUNCTION__, x, y, w, h);

      if (ctx->DrawBuffer->Name == 0) {
	 x1 = x;
	 y1 = ctx->DrawBuffer->Height - (y + h);
	 x2 = x + w - 1;
	 y2 = y1 + h - 1;
	 DBG("%s %d..%d,%d..%d (inverted)\n", __FUNCTION__, x1, x2, y1, y2);
      }
      else {
	 /* FBO - not inverted
	  */
	 x1 = x;
	 y1 = y;
	 x2 = x + w - 1;
	 y2 = y + h - 1;
	 DBG("%s %d..%d,%d..%d (not inverted)\n", __FUNCTION__, x1, x2, y1, y2);
      }
   
      x1 = CLAMP(x1, 0, ctx->DrawBuffer->Width - 1);
      y1 = CLAMP(y1, 0, ctx->DrawBuffer->Height - 1);
      x2 = CLAMP(x2, 0, ctx->DrawBuffer->Width - 1);
      y2 = CLAMP(y2, 0, ctx->DrawBuffer->Height - 1);
   
      DBG("%s %d..%d,%d..%d (clamped)\n", __FUNCTION__, x1, x2, y1, y2);

      I915_STATECHANGE(i915, I915_UPLOAD_BUFFERS);
      i915->state.Buffer[I915_DESTREG_SR0] = _3DSTATE_SCISSOR_RECT_0_CMD;
      i915->state.Buffer[I915_DESTREG_SR1] = (y1 << 16) | (x1 & 0xffff);
      i915->state.Buffer[I915_DESTREG_SR2] = (y2 << 16) | (x2 & 0xffff);


      i915->state.Buffer[I915_DESTREG_SENABLE] =
	 (_3DSTATE_SCISSOR_ENABLE_CMD | ENABLE_SCISSOR_RECT);
   }
   else
      i915->state.Buffer[I915_DESTREG_SENABLE] =
         (_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);

}




