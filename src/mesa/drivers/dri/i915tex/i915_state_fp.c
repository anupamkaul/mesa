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
#include "i915_fpc.h"





/*********************************************************************************
 * Program instructions (and decls)
 */


static void i915_upload_fp( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct i915_fragment_program *fp = i915->fragment_program;
   GLuint i;

   if (&fp->Base != intel->state.FragmentProgram->_Current) {
      i915->fragment_program = (struct i915_fragment_program *)
	 intel->state.FragmentProgram->_Current;
     
      fp = i915->fragment_program;
      /* This is stupid 
       */
      intel->state.dirty.intel |= INTEL_NEW_FRAGMENT_PROGRAM;
   }

   /* As the compiled program depends only on the original program
    * text (??? for now at least ???), there is no need for a compiled
    * program cache, just store the compiled version with the original
    * text.
    */
   if (!fp->translated) {
      i915_compile_fragment_program(i915, fp);
   }

   BEGIN_BATCH( fp->program_size, 0 );

   for (i = 0; i < fp->program_size; i++)
      OUT_BATCH( fp->program[i] );

   ADVANCE_BATCH();
}


/* See i915_wm.c:
 */
const struct intel_tracked_state i915_fp_compile_and_upload = {
   .dirty = {
      .mesa  = (0),
      .intel   = (INTEL_NEW_FRAGMENT_PROGRAM), /* ?? Is this all ?? */
      .extra = 0
   },
   .update = i915_upload_fp
};


/*********************************************************************************
 * Program constants and state parameters
 */
static void
upload_constants(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   struct i915_fragment_program *p = i915->fragment_program;
   GLint i;

   /* XXX: Pull from state, not ctx!!! 
    */
   if (p->nr_params)
      _mesa_load_state_parameters(&intel->ctx, p->Base.Base.Parameters);

   for (i = 0; i < p->nr_params; i++) {
      GLint reg = p->param[i].reg;
      COPY_4V(p->constant[reg], p->param[i].values);
   }

   /* Always seemed to get a failure if I used memcmp() to
    * shortcircuit this state upload.  Needs further investigation?
    */
   if (p->nr_constants) {
      GLuint nr = p->nr_constants;

      BEGIN_BATCH( nr * 4 + 2, 0 );
      OUT_BATCH( _3DSTATE_PIXEL_SHADER_CONSTANTS | (nr * 4) );
      OUT_BATCH( (1 << (nr - 1)) | ((1 << (nr - 1)) - 1) );

      for (i = 0; i < nr; i++) {
	 OUT_BATCH_F(p->constant[i][0]);
	 OUT_BATCH_F(p->constant[i][1]);
	 OUT_BATCH_F(p->constant[i][2]);
	 OUT_BATCH_F(p->constant[i][3]);
      }
      
      ADVANCE_BATCH();
   }
}


/* This tracked state is unique in that the state it monitors varies
 * dynamically depending on the parameters tracked by the fragment and
 * vertex programs.  This is the template used as a starting point,
 * each context will maintain a copy of this internally and update as
 * required.
 */
const struct intel_tracked_state i915_fp_constants = {
   .dirty = {
      .mesa = 0,      /* plus fp state flags */
      .intel  = INTEL_NEW_FRAGMENT_PROGRAM,
      .extra = 0
   },
   .update = upload_constants
};
