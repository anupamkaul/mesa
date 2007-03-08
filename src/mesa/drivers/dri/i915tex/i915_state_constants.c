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



static void
upload_params(struct i915_fragment_program *p)
{
   GLint i;


   /* Always seemed to get a failure if I used memcmp() to
    * shortcircuit this state upload.  Needs further investigation?
    */
   if (p->nr_constants) {
      GLuint nr = p->nr_constants;

      I915_ACTIVESTATE(i915, I915_UPLOAD_CONSTANTS, 1);
      I915_STATECHANGE(i915, I915_UPLOAD_CONSTANTS);

      i915->state.Constant[0] = _3DSTATE_PIXEL_SHADER_CONSTANTS | ((nr) * 4);
      i915->state.Constant[1] = (1 << (nr - 1)) | ((1 << (nr - 1)) - 1);

      memcpy(&i915->state.Constant[2], p->constant, 4 * sizeof(int) * (nr));
      i915->state.ConstantSize = 2 + (nr) * 4;

      if (0) {
         GLuint i;
         for (i = 0; i < nr; i++) {
            fprintf(stderr, "const[%d]: %f %f %f %f\n", i,
                    p->constant[i][0],
                    p->constant[i][1], p->constant[i][2], p->constant[i][3]);
         }
      }
   }
   else {
      I915_ACTIVESTATE(i915, I915_UPLOAD_CONSTANTS, 0);
   }


///////////////////////
   if (p->nr_params)
      _mesa_load_state_parameters(p->ctx, p->FragProg.Base.Parameters);

   for (i = 0; i < p->nr_params; i++) {
      GLint reg = p->param[i].reg;
      COPY_4V(p->constant[reg], p->param[i].values);
   }

   p->params_uptodate = 1;


/////////////////

   /* Allocate state in the indirect buffer and upload constants:
    */
   
   
}





static void
track_params(struct i915_fragment_program *p)
{
   GLint i;

   if (p->nr_params)
      _mesa_load_state_parameters(p->ctx, p->FragProg.Base.Parameters);

   for (i = 0; i < p->nr_params; i++) {
      GLint reg = p->param[i].reg;
      COPY_4V(p->constant[reg], p->param[i].values);
   }

   p->params_uptodate = 1;
}


/* This tracked state is unique in that the state it monitors varies
 * dynamically depending on the parameters tracked by the fragment and
 * vertex programs.  This is the template used as a starting point,
 * each context will maintain a copy of this internally and update as
 * required.
 */
const struct i915_tracked_state i915_constant_buffer = {
   .dirty = {
      .mesa = 0,      /* plus fp flags */
      .intel  = (I915_NEW_FRAGMENT_PROGRAM),
      .cache = (CACHE_NEW_PROGRAM) /* ?? */
   },
   .update = upload_params
};

