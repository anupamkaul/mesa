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

#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "intel_batchbuffer.h"

#include "i915_reg.h"
#include "i915_context.h"
#include "i915_program.h"

#include "program_instruction.h"
#include "program.h"
#include "programopt.h"



void
i915_upload_program()
{
   if (dirty & I915_UPLOAD_PROGRAM) {
      if (INTEL_DEBUG & DEBUG_STATE)
         fprintf(stderr, "I915_UPLOAD_PROGRAM:\n");

      assert((state->Program[0] & 0x1ff) + 2 == state->ProgramSize);

      emit(intel, state->Program, state->ProgramSize * sizeof(GLuint));
      if (INTEL_DEBUG & DEBUG_STATE)
         i915_disassemble_program(state->Program, state->ProgramSize);
   }
}




void
i915ValidateFragmentProgram(struct i915_context *i915)
{
   GLcontext *ctx = &i915->intel.ctx;
   struct intel_context *intel = intel_context(ctx);

   struct i915_fragment_program *p =
      (struct i915_fragment_program *) ctx->FragmentProgram._Current;

   int i, offset = 0;

   if (i915->current_program != p) {


      i915->current_program = p;
   }


   /* Important:
    */
   VB->AttribPtr[VERT_ATTRIB_POS] = VB->NdcPtr;

   if (!p->translated)
      translate_program(p);


      i915_upload_program(i915, p);
}

