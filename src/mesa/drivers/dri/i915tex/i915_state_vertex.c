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

#include "intel_draw.h"
#include "i915_context.h"
#include "i915_reg.h"

#include "intel_vb.h"
#include "vf/vf.h"




/***********************************************************************
 * 
 */

#define SZ_TO_HW(sz)  ((sz-2)&0x3)
#define EMIT_SZ(sz)   (EMIT_1F + (sz) - 1)
#define EMIT_ATTR( ATTR, STYLE, S4, SZ )			\
do {								\
   i915->vertex_attrs[i915->vertex_attr_count].attrib = (ATTR);	\
   i915->vertex_attrs[i915->vertex_attr_count].format = (STYLE);	\
   i915->vertex_attr_count++;					\
   s4 |= S4;							\
   offset += (SZ);						\
} while (0)

#define EMIT_PAD( N )						\
do {								\
   i915->vertex_attrs[i915->vertex_attr_count].attrib = 0;		\
   i915->vertex_attrs[i915->vertex_attr_count].format = EMIT_PAD;	\
   i915->vertex_attrs[i915->vertex_attr_count].offset = (N);		\
   i915->vertex_attr_count++;					\
   offset += (N);						\
} while (0)

/***********************************************************************
 * 
 */
static inline GLuint attr_size(GLuint sizes, GLuint attr)
{
   return ((sizes >> (attr*2)) & 0x3) + 1;
}

static void i915_calculate_vertex_format( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );

   /* INTEL_NEW_FRAGMENT_PROGRAM
    */
   struct i915_fragment_program *fp = 
      i915_fragment_program(intel->state.FragmentProgram->_Current);
   const GLuint inputsRead = fp->Base.Base.InputsRead;

   /* INTEL_NEW_FRAG_ATTRIB_SIZES
    */
   const GLuint sizes = intel->frag_attrib_sizes;

   GLuint s2 = S2_TEXCOORD_NONE;
   GLuint s4 = 0;
   GLuint offset = 0;
   GLuint i;
   GLboolean need_w = (inputsRead & FRAG_BITS_TEX_ANY);
   GLboolean have_w = (attr_size(sizes, FRAG_ATTRIB_WPOS) == 4);
   GLboolean have_z = (attr_size(sizes, FRAG_ATTRIB_WPOS) >= 3);

   i915->vertex_attr_count = 0;

   if (have_w && need_w) {
      EMIT_ATTR(VF_ATTRIB_POS, EMIT_4F_VIEWPORT, S4_VFMT_XYZW, 16);
   }
   else if (1 || have_z) {
      EMIT_ATTR(VF_ATTRIB_POS, EMIT_3F_VIEWPORT, S4_VFMT_XYZ, 12);
   }
   else {
      /* Need to update default z values to whatever zero maps to in
       * the current viewport.  Or figure out that we don't need z in
       * the current state.
       */
      EMIT_ATTR(VF_ATTRIB_POS, EMIT_2F_VIEWPORT, S4_VFMT_XY, 8);
   }
   

   if (inputsRead & FRAG_BIT_COL0) {
      EMIT_ATTR(VF_ATTRIB_COLOR0, EMIT_4UB_4F_BGRA, S4_VFMT_COLOR, 4);
   }

   if (inputsRead & FRAG_BIT_COL1) {
      EMIT_ATTR(VF_ATTRIB_COLOR1, EMIT_3UB_3F_BGR, S4_VFMT_SPEC_FOG, 3);
      EMIT_PAD(1);
   }

   if (inputsRead & FRAG_BIT_FOGC) {
      /* Note that the hardware gives precedence to this over the
       * byte-sized fog parameter in specular alpha.
       */
      EMIT_ATTR(VF_ATTRIB_FOG, EMIT_1F, S4_VFMT_FOG_PARAM, 4);
   }

   for (i = 0; i < I915_TEX_UNITS; i++) {
      if (inputsRead & (FRAG_BIT_TEX0 << i)) {

	 /* INTEL_NEW_FRAG_ATTRIB_SIZES 
	  *
	  * Basically need to know whether or not to include the W
	  * value.  This could be used to transform TXP->TEX
	  * instructions in the fragment program, but so far haven't
	  * seen much performance benefit from doing that.
	  *
	  * There could also be benefit in things like turning off
	  * perspective interpolation for the texture coordinates when
	  * it is detected that the application is really doing
	  * 2d-type texture operations.
	  */
	 int sz = attr_size(sizes, FRAG_ATTRIB_TEX0+i); 
	 
         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(sz));

         EMIT_ATTR(VF_ATTRIB_TEX0 + i, EMIT_SZ(sz), 0, sz * 4);
      }
      else if (i == fp->wpos_tex) {

         /* If WPOS is required, duplicate the XYZ position data in an
          * unused texture coordinate:
          */
	 /* Do another emit viewport */
      }
   }


   if (s2 != i915->vertex_format.LIS2 || 
       s4 != i915->vertex_format.LIS4) {

      intel_draw_set_hw_vertex_format( intel->draw, 
				       i915->vertex_attrs, 
				       i915->vertex_attr_count,
				       offset );

      /* Needed?  This does raise the INTEL_NEW_VERTEX_SIZE flag:
       */
      intel_vb_set_vertex_size( intel->vb, offset );
      
      i915->vertex_format.LIS2 = s2;
      i915->vertex_format.LIS4 = s4;
      intel->state.dirty.intel |= I915_NEW_VERTEX_FORMAT;
   }
}


/* Could use the information calculated here to optimize the fragment
 * program.
 */
const struct intel_tracked_state i915_vertex_format = {
   .dirty = {
      .mesa  = 0,
      .intel   = (INTEL_NEW_FRAGMENT_PROGRAM | 
		  INTEL_NEW_FRAG_ATTRIB_SIZES), 
      .extra = 0
   },
   .update = i915_calculate_vertex_format
};




#undef EMIT_ATTR
#undef EMIT_PAD
#define EMIT_ATTR( ATTR, STYLE, S4, SZ )		\
do {							\
   prim_attrs[prim_attr_count].attrib = (ATTR);		\
   prim_attrs[prim_attr_count].format = (STYLE);	\
   prim_attr_count++;					\
   offset += (SZ);					\
} while (0)

#define EMIT_PAD( N )					\
do {							\
   prim_attrs[prim_attr_count].attrib = 0;		\
   prim_attrs[prim_attr_count].format = EMIT_PAD;	\
   prim_attrs[prim_attr_count].offset = (N);		\
   prim_attr_count++;					\
   offset += (N);					\
} while (0)



static void calculate_setup_vertex_format( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );

   /* This is actually covered by I915_NEW_VERTEX_FORMAT
    */
   struct i915_fragment_program *fp = i915_fragment_program(intel->state.FragmentProgram->_Current);
   const GLuint inputsRead = fp->Base.Base.InputsRead;
   struct vf_attr_map prim_attrs[VF_ATTRIB_MAX];
   GLuint prim_attr_count = 0;
   GLuint offset = 0;

   /* This should really be set up by the draw engine itself.
    * 
    * As it stands, it requires:
    *   - vertex_header
    *   - clip coordinates (if and only if clipped_prims is set)
    *   - [the hardware vertex]
    *   - backface colors in the same layouts as frontface colors.
    */

   EMIT_ATTR(VF_ATTRIB_VERTEX_HEADER, EMIT_1F, 0, 4);
   
   /* INTEL_NEW_VB_STATE
    */
   if (intel->vb_state.clipped_prims) {
      EMIT_ATTR(VF_ATTRIB_CLIP_POS, EMIT_4F, 0, 16);
   }

   /* I915_NEW_VERTEX_FORMAT 
    */
   memcpy( prim_attrs + prim_attr_count, 
	   i915->vertex_attrs, 
	   i915->vertex_attr_count * sizeof(struct vf_attr_map));

   offset += i915->intel.vb->vertex_size_bytes;
   prim_attr_count += i915->vertex_attr_count;

   /* _NEW_LIGHT
    */
   if (intel->state.Light->Model.TwoSide) {
      if (inputsRead & FRAG_ATTRIB_COL0) {
	 EMIT_ATTR(VF_ATTRIB_BFC0, EMIT_4UB_4F_BGRA, 0, 4);
      }
	    
      if (inputsRead & FRAG_ATTRIB_COL1) {
	 EMIT_ATTR(VF_ATTRIB_BFC1, EMIT_3UB_3F_BGR, 0, 3);
	 EMIT_PAD(1);
      }
   }

   
   intel_draw_set_prim_vertex_format( intel->draw, 
				      prim_attrs, 
				      prim_attr_count,
				      offset );
}

const struct intel_tracked_state i915_setup_vertex_format = {
   .dirty = {
      .mesa  = _NEW_LIGHT,
      .intel   = I915_NEW_VERTEX_FORMAT | INTEL_NEW_VB_STATE, 
      .extra = 0
   },
   .update = calculate_setup_vertex_format
};

