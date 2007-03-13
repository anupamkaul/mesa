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
#include "macros.h"
#include "enums.h"
#include "program.h"

#include "intel_batchbuffer.h"
#include "i915_context.h"
#include "i915_reg.h"
#include "tnl/t_context.h"
#include "tnl/t_vertex.h"


#if 0
/* Scan the TNL VB struct and look at the size of each attribute
 * coming out.  
 *
 * The fragment program has been determined by this point, so it is ok
 * to restrict the list to the inputs referenced by the fragprog.
 *
 * This is not a 
 */
void check_input_sizes( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLcontext *ctx = &intel->ctx;
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   GLubyte old_sizes[8];
   GLuint i;

   memcpy(old_sizes, i915->fragprog.input_sizes, sizeof(old_sizes));

   for (i = 0; i < FRAG_ATTRIB_MAX; i++) {
      GLvector4f *attrib = VB->AttribPtr[i];
      i915->fragprog.input_sizes[i] = attrib->size;
   }

   /* Raise statechanges if input sizes and varying have changed: 
    */
   if (memcmp(i915->fragprog.input_sizes, old_sizes, sizeof(old_sizes)) != 0)
      intel->state.dirty.intel |= I915_NEW_INPUT_SIZES;
}

#endif


/***********************************************************************
 * 
 */

#define SZ_TO_HW(sz)  ((sz-2)&0x3)
#define EMIT_SZ(sz)   (EMIT_1F + (sz) - 1)
#define EMIT_ATTR( ATTR, STYLE, S4, SZ )				\
do {									\
   intel->vertex_attrs[intel->vertex_attr_count].attrib = (ATTR);	\
   intel->vertex_attrs[intel->vertex_attr_count].format = (STYLE);	\
   s4 |= S4;								\
   intel->vertex_attr_count++;						\
   offset += (SZ);							\
} while (0)

#define EMIT_PAD( N )							\
do {									\
   intel->vertex_attrs[intel->vertex_attr_count].attrib = 0;		\
   intel->vertex_attrs[intel->vertex_attr_count].format = EMIT_PAD;	\
   intel->vertex_attrs[intel->vertex_attr_count].offset = (N);		\
   intel->vertex_attr_count++;						\
   offset += (N);							\
} while (0)

/***********************************************************************
 * 
 */


static void i915_calculate_vertex_format( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct i915_fragment_program *fp = i915->fragment_program;
   const GLuint inputsRead = fp->Base.Base.InputsRead;
   GLuint s2 = S2_TEXCOORD_NONE;
   GLuint s4 = 0;
   GLuint offset = 0;
   GLuint i;

   intel->vertex_attr_count = 0;
   intel->wpos_offset = 0;
   intel->wpos_size = 0;
   intel->coloroffset = 0;
   intel->specoffset = 0;

   if (inputsRead & FRAG_BITS_TEX_ANY) {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_4F_VIEWPORT, S4_VFMT_XYZW, 16);
   }
   else {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_3F_VIEWPORT, S4_VFMT_XYZ, 12);
   }

   if (inputsRead & FRAG_BIT_COL0) {
      intel->coloroffset = offset / 4;
      EMIT_ATTR(_TNL_ATTRIB_COLOR0, EMIT_4UB_4F_BGRA, S4_VFMT_COLOR, 4);
   }

   if (inputsRead & (FRAG_BIT_COL1 | FRAG_BIT_FOGC)) {
      if (inputsRead & FRAG_BIT_COL1) {
         intel->specoffset = offset / 4;
         EMIT_ATTR(_TNL_ATTRIB_COLOR1, EMIT_3UB_3F_BGR, S4_VFMT_SPEC_FOG, 3);
      }
      else
         EMIT_PAD(3);

      if (inputsRead & FRAG_BIT_FOGC)
         EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1UB_1F, S4_VFMT_SPEC_FOG, 1);
      else
         EMIT_PAD(1);
   }

   if (inputsRead & FRAG_BIT_FOGC) {
      EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1F, S4_VFMT_FOG_PARAM, 4);
   }

   for (i = 0; i < I915_TEX_UNITS; i++) {
      if (inputsRead & (FRAG_BIT_TEX0 << i)) {

	 /* _NEW_VB_OUTPUT_SIZES 
	  */
/*          int sz = VB->TexCoordPtr[i]->size; */
	 int sz = 2;

         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(sz));

         EMIT_ATTR(_TNL_ATTRIB_TEX0 + i, EMIT_SZ(sz), 0, sz * 4);
      }
      else if (i == fp->wpos_tex) {

         /* If WPOS is required, duplicate the XYZ position data in an
          * unused texture coordinate:
          */
         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(3));

         intel->wpos_offset = offset;
         intel->wpos_size = 3 * sizeof(GLuint);

         EMIT_PAD(intel->wpos_size);
      }
   }

   if (s2 != i915->fragprog.LIS2 || 
       s4 != i915->fragprog.LIS4) {

      GLuint vs = _tnl_install_attrs(&intel->ctx,
				     intel->vertex_attrs,
				     intel->vertex_attr_count,
				     intel->ViewportMatrix.m, 0);

      intel->vertex_size = vs >> 2;
      i915->fragprog.LIS2 = s2;
      i915->fragprog.LIS4 = s4;
      intel->state.dirty.intel |= I915_NEW_VERTEX_FORMAT;
   }
}


/* Could use the information calculated here to optimize the fragment
 * program.
 */
const struct intel_tracked_state i915_vertex_format = {
   .dirty = {
      .mesa  = 0,
      .intel   = (I915_NEW_FRAGMENT_PROGRAM 
/* 		 | INTEL_NEW_VB_OUTPUT_SIZES */
	 ), 
      .extra = 0
   },
   .update = i915_calculate_vertex_format
};

