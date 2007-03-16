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

#define FILE_DEBUG_FLAG DEBUG_STATE

/* State that we have chosen to store in the DYNAMIC segment of the
 * i915 indirect state mechanism.
 *
 * It is very nice to note that in normal rendering, none of these
 * packets will end up being emitted.
 *
 * Can't cache these in the way we do the static state, as there is no
 * start/size in the command packet, instead an 'end' value that gets
 * incremented.
 *
 * Additionally, there seems to be a requirement to re-issue the full
 * (active) state every time a 4kb boundary is crossed.
 *
 * Simplest implementation is probably just to emit the full active
 * state every time.  Next would be to diff against previous, but note 
 */

static void set_dynamic_indirect( struct intel_context *intel,
				  GLuint offset,
				  const GLuint *src,
				  GLuint size )
{
#if 1
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint *dest = i915->dyn_indirect.buf + offset;
   GLuint i;

   for (i = 0; i < size; i++) {
      if (dest[i] != src[i]) {
	 dest[i] = src[i];
	 intel->state.dirty.intel |= I915_NEW_DYNAMIC_INDIRECT;
      }
   }
#else
   GLuint i;
   BEGIN_BATCH(size, 0);
   for (i = 0; i < size; i++)
      OUT_BATCH(src[i]);
   ADVANCE_BATCH();
#endif
}


/***********************************************************************
 * Modes4: stencil masks and logicop 
 */
static void upload_MODES4( struct intel_context *intel )
{
   GLuint modes4 = 0;

   /* _NEW_STENCIL */
   if (intel->state.Stencil->Enabled) {
      GLint testmask = intel->state.Stencil->ValueMask[0] & 0xff;
      GLint writemask = intel->state.Stencil->WriteMask[0] & 0xff;

      modes4 |= (_3DSTATE_MODES_4_CMD |
		 ENABLE_STENCIL_TEST_MASK |
		 STENCIL_TEST_MASK(testmask) |
		 ENABLE_STENCIL_WRITE_MASK |
		 STENCIL_WRITE_MASK(writemask));
   }

   /* _NEW_COLOR */
   if (intel->state.Color->_LogicOpEnabled) {
      modes4 |= (_3DSTATE_MODES_4_CMD |
		 ENABLE_LOGIC_OP_FUNC |
		 LOGIC_OP_FUNC(intel_translate_logic_op(intel->state.Color->LogicOp)));
   }
   
   /* Always, so that we know when state is in-active: 
    */
   set_dynamic_indirect( intel, 
			 I915_DYNAMIC_MODES4,
			 &modes4,
			 1 );
}

const struct intel_tracked_state i915_upload_MODES4 = {
   .dirty = {
      .mesa = _NEW_COLOR | _NEW_STENCIL,
      .intel = 0,
      .extra = 0
   },
   .update = upload_MODES4
};


/***********************************************************************
 */

static void upload_BFO( struct intel_context *intel )
{
   GLuint bf[2];

   bf[0] = 0;
   bf[1] = 0;
   

   /* _NEW_STENCIL 
    */
   if (intel->state.Stencil->Enabled) {
      if (intel->state.Stencil->TestTwoSide) {
	 GLint test  = intel_translate_compare_func(intel->state.Stencil->Function[1]);
	 GLint fop   = intel_translate_stencil_op(intel->state.Stencil->FailFunc[1]);
	 GLint dfop  = intel_translate_stencil_op(intel->state.Stencil->ZFailFunc[1]);
	 GLint dpop  = intel_translate_stencil_op(intel->state.Stencil->ZPassFunc[1]);
	 GLint ref   = intel->state.Stencil->Ref[1] & 0xff;
	 GLint wmask = intel->state.Stencil->WriteMask[1] & 0xff;
	 GLint tmask = intel->state.Stencil->ValueMask[1] & 0xff;
      
	 bf[0] = (_3DSTATE_BACKFACE_STENCIL_OPS |
		  BFO_ENABLE_STENCIL_FUNCS |
		  BFO_ENABLE_STENCIL_TWO_SIDE |
		  BFO_ENABLE_STENCIL_REF |
		  BFO_STENCIL_TWO_SIDE |
		  (ref  << BFO_STENCIL_REF_SHIFT) |
		  (test << BFO_STENCIL_TEST_SHIFT) |
		  (fop  << BFO_STENCIL_FAIL_SHIFT) |
		  (dfop << BFO_STENCIL_PASS_Z_FAIL_SHIFT) |
		  (dpop << BFO_STENCIL_PASS_Z_PASS_SHIFT));

	 bf[1] = (_3DSTATE_BACKFACE_STENCIL_MASKS |
		  BFM_ENABLE_STENCIL_TEST_MASK |
		  BFM_ENABLE_STENCIL_WRITE_MASK |
		  (tmask << BFM_STENCIL_TEST_MASK_SHIFT) |
		  (wmask << BFM_STENCIL_WRITE_MASK_SHIFT));
      }
      else {
	 /* This actually disables two-side stencil: The bit set is a
	  * modify-enable bit to indicate we are changing the two-side
	  * setting.  Then there is a symbolic zero to show that we are
	  * setting the flag to zero/off.
	  */
	 bf[0] = (_3DSTATE_BACKFACE_STENCIL_OPS |
		  BFO_ENABLE_STENCIL_TWO_SIDE |
		  0);
	 bf[1] = 0;
      }      
   }


   set_dynamic_indirect( intel, 
			 I915_DYNAMIC_BFO_0,
			 &bf[0],
			 2 );
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
 */


static void upload_BLENDCOLOR( struct intel_context *intel )
{
   GLuint bc[2];

   /* _NEW_COLOR 
    */
   if (intel->state.Color->BlendEnabled) {
      const GLfloat *color = intel->state.Color->BlendColor;
      GLubyte r, g, b, a;

      UNCLAMPED_FLOAT_TO_UBYTE(r, color[RCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(g, color[GCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(b, color[BCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(a, color[ACOMP]);

      bc[0] = (_3DSTATE_CONST_BLEND_COLOR_CMD);
      bc[1] = (a << 24) | (r << 16) | (g << 8) | b;

   }
   else {
      bc[0] = 0;
      bc[1] = 0;
   }

   set_dynamic_indirect( intel, 
			 I915_DYNAMIC_BC_0,
			 bc,
			 2 );
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
 */


static void upload_IAB( struct intel_context *intel )
{
   GLuint iab = 0;

   if (intel->state.Color->BlendEnabled) {
      GLuint eqRGB = intel->state.Color->BlendEquationRGB;
      GLuint eqA = intel->state.Color->BlendEquationA;
      GLuint srcRGB = intel->state.Color->BlendSrcRGB;
      GLuint dstRGB = intel->state.Color->BlendDstRGB;
      GLuint srcA = intel->state.Color->BlendSrcA;
      GLuint dstA = intel->state.Color->BlendDstA;

      if (eqA == GL_MIN || eqA == GL_MAX) {
	 srcA = dstA = GL_ONE;
      }

      if (eqRGB == GL_MIN || eqRGB == GL_MAX) {
	 srcRGB = dstRGB = GL_ONE;
      }
      
      if (srcA != srcRGB ||
	  dstA != dstRGB ||
	  eqA != eqRGB) {

	 iab = (_3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD |    
		IAB_MODIFY_ENABLE |
		IAB_ENABLE |
		IAB_MODIFY_FUNC | 
		IAB_MODIFY_SRC_FACTOR | 
		IAB_MODIFY_DST_FACTOR |
		SRC_ABLND_FACT(intel_translate_blend_factor(srcA)) |
		DST_ABLND_FACT(intel_translate_blend_factor(dstA)) |
		(i915_translate_blend_equation(eqA) << IAB_FUNC_SHIFT));
      }	 
      else {
	 iab = (_3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD |    
		IAB_MODIFY_ENABLE |
		0);
      }
   }


   set_dynamic_indirect( intel, 
			 I915_DYNAMIC_IAB,
			 &iab,
			 1 );
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
 */



static void upload_DEPTHSCALE( struct intel_context *intel )
{
   union { GLfloat f; GLuint u; } ds[2];

   ds[0].u = 0;
   ds[1].u = 0;
   
   if (intel->state.Polygon->OffsetFill) {
      
      ds[0].u = (_3DSTATE_DEPTH_OFFSET_SCALE);
      ds[1].f = 0;		/* XXX */
      
   }

   set_dynamic_indirect( intel, 
			 I915_DYNAMIC_DEPTHSCALE_0,
			 &ds[0].u,
			 2 );
}

const struct intel_tracked_state i915_upload_DEPTHSCALE = {
   .dirty = {
      .mesa = _NEW_POLYGON,
      .intel = 0,
      .extra = 0
   },
   .update = upload_DEPTHSCALE
};

/***********************************************************************
 * Do the group emit in a single packet.  
 */

#define CHECK( idx, nr ) do {				\
   if (i915->dyn_indirect.buf[idx] != 0) {		\
      GLint i;						\
      for (i = 0; i < nr; i++)				\
	 buf[count++] = i915->dyn_indirect.buf[idx+i];	\
   }							\
} while (0)



static void emit_indirect( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint buf[I915_DYNAMIC_SIZE], count = 0;
   
   CHECK( I915_DYNAMIC_MODES4, 1 );
   CHECK( I915_DYNAMIC_DEPTHSCALE_0, 2 );
   CHECK( I915_DYNAMIC_IAB, 1 );
   CHECK( I915_DYNAMIC_BC_0, 2 );
   CHECK( I915_DYNAMIC_BFO_0, 2 );

   /* XXX: need to check if we wrap 4kb and if so pad. 
    */
   /* Or just emit the whole lot, zeros and all (fix later...):
    */
   /* Also - want to check that something has changed & we're not just
    * re-emitting the same stuff.
    */
   if (count) {
      GLuint size = I915_DYNAMIC_SIZE * 4;
      GLuint flag = i915->dyn_indirect.done_reset ? 0 : DIS0_BUFFER_RESET;
      GLuint segment = SEGMENT_DYNAMIC_INDIRECT;
      GLuint offset = intel->batch->segment_finish_offset[segment];

      i915->dyn_indirect.done_reset = 1;

      BEGIN_BATCH(2,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | LI0_STATE_DYNAMIC_INDIRECT | (1<<14) | 0);
      OUT_RELOC( intel->batch->buffer, 
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		 ((offset + size - 4) | DIS0_BUFFER_VALID | flag) );
      ADVANCE_BATCH();

      /* XXX:
       */
      assert( offset + size < intel->batch->segment_max_offset[segment]);      
      intel->batch->segment_finish_offset[segment] += size;

      /* Just emit the original buffer, zeros and all as this will
       * avoid wrapping issues.  This is usually not emitted at all,
       * so not urgent to fix:
       */
      memcpy(intel->batch->map + offset, i915->dyn_indirect.buf, size );      
   }
}

const struct intel_tracked_state i915_upload_dynamic_indirect = {
   .dirty = {
      .mesa = 0,
      .intel = I915_NEW_DYNAMIC_INDIRECT,
      .extra = 0
   },
   .update = emit_indirect
};



