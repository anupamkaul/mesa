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
 * Can't cache these in the way we do the static state, as there is no
 * start/size in the command packet, instead an 'end' value that gets
 * incremented.
 *
 * Additionally, there seems to be a requirement to re-issue the full
 * (active) state every time a 4kb boundary is crossed.
 */



/* Macro to identify whole packets that differ in any of their dwords.
 */
#define CHECK( idx, nr ) do {			\
   for (i = idx; i < idx+nr; i++) {		\
      if (i915->dynamic.current[i] != i915->dynamic.hardware[i]) {	\
	 dirty |= ((1<<nr)-1) << idx;		\
	 size += nr;				\
	 break;					\
      }						\
   }						\
} while (0)


static void i915_dynamic_next_page( struct i915_context *i915 )
{
   struct intel_context *intel = &i915->intel;
   GLuint offset = intel->batch->segment_finish_offset[SEGMENT_DYNAMIC_INDIRECT];
   GLuint start = ALIGN(offset, 4096);

   /* XXX: FIX ME
    */
   assert(start + 4096 <= intel->batch->segment_max_offset[SEGMENT_DYNAMIC_INDIRECT]);
   intel->batch->segment_finish_offset[SEGMENT_DYNAMIC_INDIRECT] = start + 4096;

   i915->dynamic.ptr = (GLuint *)(intel->batch->state_map + start);
   i915->dynamic.offset = start;   
}

static INLINE GLuint page_space( void *ptr )
{
   return 4096 - (((unsigned long)ptr) & (4096-1));
}

void i915_dynamic_lost_hardware( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   i915->dynamic.ptr = (GLuint *)4096-1;
   i915->dynamic.offset = 0;
}

static void emit_dynamic_indirect( struct intel_context *intel)
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   GLuint dirty = 0;
   GLuint size = 0;
   GLuint i;

   CHECK( I915_DYNAMIC_MODES4, 1 ); 
   CHECK( I915_DYNAMIC_DEPTHSCALE_0, 2 ); 
   CHECK( I915_DYNAMIC_IAB, 1 ); 
   CHECK( I915_DYNAMIC_BC_0, 2 ); 
   CHECK( I915_DYNAMIC_BFO_0, 2 ); 
   CHECK( I915_DYNAMIC_STP_0, 2 ); 

   if (!dirty) 
      return;

   memcpy(i915->dynamic.current, 
	  i915->dynamic.hardware,
	  sizeof(i915->dynamic.hardware));

   /* Check if we cross a 4k boundary and if so allocate a new page
    * and emit full state. 
    */
   if (page_space(i915->dynamic.ptr) <  size * sizeof(GLuint) )
   {
      dirty = (1<<I915_MAX_DYNAMIC)-1;
      size = I915_MAX_DYNAMIC;
      i915_dynamic_next_page( i915 );
   } 


   i915->current.offsets[I915_CACHE_DYNAMIC] = i915->dynamic.offset;
   i915->current.sizes[I915_CACHE_DYNAMIC] = 1;
   i915->hardware_dirty |= I915_HW_INDIRECT;

   /* Finally emit the state: 
    */
   {
      GLuint i, j = 0;

      for (i = 0; i < I915_MAX_DYNAMIC; i++) {
	 if (dirty & (1<<i)) 
	    i915->dynamic.ptr[j++] = i915->dynamic.current[i];
      }

      i915->dynamic.ptr += j;
      i915->dynamic.offset += j * sizeof(GLuint);
   }
}


const struct intel_tracked_state i915_upload_dynamic_indirect = {
   .dirty = {
      .mesa = 0,
      .intel = (I915_NEW_DYNAMIC_INDIRECT),
      .extra = 0
   },
   .update = emit_dynamic_indirect
};

static inline void set_dynamic_indirect( struct intel_context *intel,
					 GLuint offset,
					 const GLuint *src,
					 GLuint size )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint *dst = i915->dynamic.current + offset;

   if (memcmp(dst, src, size * 4) != 0) {
      intel->state.dirty.intel |= I915_NEW_DYNAMIC_INDIRECT;
      memcpy(dst, src, size * 4);
   }
}


/***********************************************************************
 * Modes4: stencil masks and logicop 
 */
static void upload_MODES4( struct intel_context *intel )
{
   GLuint modes4 = 0;

   /* _NEW_STENCIL */
   {
      GLint testmask = intel->state.Stencil->ValueMask[0] & 0xff;
      GLint writemask = intel->state.Stencil->WriteMask[0] & 0xff;

      modes4 |= (_3DSTATE_MODES_4_CMD |
		 ENABLE_STENCIL_TEST_MASK |
		 STENCIL_TEST_MASK(testmask) |
		 ENABLE_STENCIL_WRITE_MASK |
		 STENCIL_WRITE_MASK(writemask));
   }

   /* _NEW_COLOR */
   {
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

   memset( bf, 0, sizeof(bf) );

   /* _NEW_STENCIL 
    */
   {
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

   memset( bc, 0, sizeof(bc) );

   /* _NEW_COLOR 
    */
   {
      const GLfloat *color = intel->state.Color->BlendColor;
      GLubyte r, g, b, a;

      UNCLAMPED_FLOAT_TO_UBYTE(r, color[RCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(g, color[GCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(b, color[BCOMP]);
      UNCLAMPED_FLOAT_TO_UBYTE(a, color[ACOMP]);

      bc[0] = (_3DSTATE_CONST_BLEND_COLOR_CMD);
      bc[1] = (a << 24) | (r << 16) | (g << 8) | b;
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

   {
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

   memset( ds, 0, sizeof(ds) );
   
   {
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

static void upload_STIPPLE( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   GLboolean fallback_on_poly_stipple = 0;
   GLuint st[2];

   st[0] = _3DSTATE_STIPPLE;
   st[1] = 0;
   
   /* _NEW_POLYGON 
    */
   if (intel->state.Polygon->StippleFlag) {

      /* _NEW_POLYGONSTIPPLE
       */
      const GLubyte *mask = (const GLubyte *)intel->state.PolygonStipple;
      GLubyte p[4];
      GLint i, j, k;

      p[0] = mask[12] & 0xf;
      p[1] = mask[8] & 0xf;
      p[2] = mask[4] & 0xf;
      p[3] = mask[0] & 0xf;

      st[1] |= ST1_ENABLE;
      st[1] |= ((p[0] << 0) |
		(p[1] << 4) |
		(p[2] << 8) | 
		(p[3] << 12));

      p[0] |= p[0] << 4;
      p[1] |= p[1] << 4;
      p[2] |= p[2] << 4;
      p[3] |= p[3] << 4;
      
      for (k = 0; k < 8; k++) {
	 for (j = 3; j >= 0; j--) {
	    for (i = 0; i < 4; i++, mask++) {
	       if (*mask != p[j]) {
		  fallback_on_poly_stipple = 1;
		  st[1] = 0;
	       }
	    }
	 }
      }      
   }

   if (fallback_on_poly_stipple != i915->fallback_on_poly_stipple) {
      intel->state.dirty.intel |= I915_NEW_POLY_STIPPLE_FALLBACK;
      i915->fallback_on_poly_stipple = fallback_on_poly_stipple;
   }

   if (!fallback_on_poly_stipple) {
      set_dynamic_indirect( intel, 
			    I915_DYNAMIC_STP_0,
			    &st[0],
			    2 );
   }
}


const struct intel_tracked_state i915_upload_STIPPLE = {
   .dirty = {
      .mesa = _NEW_POLYGONSTIPPLE, _NEW_POLYGON,
      .intel = 0,
      .extra = 0
   },
   .update = upload_STIPPLE
};
