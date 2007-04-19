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
    

#ifndef I915_STATE_H
#define I915_STATE_H

#include "i915_context.h"
#include "i915_reg.h"


void i915_init_state( struct i915_context *i915 );
void i915_destroy_state( struct i915_context *i915 );

void i915_emit_hardware_state( struct intel_context *intel );
GLuint i915_get_hardware_state_size( struct intel_context *intel );


const struct intel_tracked_state i915_check_fallback;
const struct intel_tracked_state i915_vertex_format;
const struct intel_tracked_state i915_choose_render;

/* Immediate state: 
 */
const struct intel_tracked_state i915_upload_S0S1;
const struct intel_tracked_state i915_upload_S2S4;
const struct intel_tracked_state i915_upload_S5;
const struct intel_tracked_state i915_upload_S6;
const struct intel_tracked_state i915_upload_S7;

/* Dynamic indirect:
 */
const struct intel_tracked_state i915_upload_BFO;
const struct intel_tracked_state i915_upload_BLENDCOLOR;
const struct intel_tracked_state i915_upload_DEPTHSCALE;
const struct intel_tracked_state i915_upload_FOGCOLOR;
const struct intel_tracked_state i915_upload_FOGMODE;
const struct intel_tracked_state i915_upload_IAB;
const struct intel_tracked_state i915_upload_MODES4;
const struct intel_tracked_state i915_upload_STIPPLE;
const struct intel_tracked_state i915_upload_dynamic_indirect;

/* Other indirect:
 */
const struct intel_tracked_state i915_upload_constants;
const struct intel_tracked_state i915_upload_program;
const struct intel_tracked_state i915_upload_maps;
const struct intel_tracked_state i915_upload_samplers;
const struct intel_tracked_state i915_upload_static;

/* Perform state differencing and update hardware: 
 */
const struct intel_tracked_state i915_state_differencer;


static INLINE GLuint
i915_translate_blend_equation(GLenum mode)
{
   switch (mode) {
   case GL_FUNC_ADD:
      return BLENDFUNC_ADD;
   case GL_MIN:
      return BLENDFUNC_MIN;
   case GL_MAX:
      return BLENDFUNC_MAX;
   case GL_FUNC_SUBTRACT:
      return BLENDFUNC_SUBTRACT;
   case GL_FUNC_REVERSE_SUBTRACT:
      return BLENDFUNC_REVERSE_SUBTRACT;
   default:
      return 0;
   }
}

static INLINE GLuint U_FIXED(GLfloat value, GLuint frac_bits)
{
   value *= (1<<frac_bits);
   return value < 0 ? 0 : value;
}

static INLINE GLint S_FIXED(GLfloat value, GLuint frac_bits)
{
   return value * (1<<frac_bits);
}

static INLINE GLuint ALIGN(GLuint x, GLuint align)
{
   return (x + align - 1) & ~(align - 1);
}



#endif
