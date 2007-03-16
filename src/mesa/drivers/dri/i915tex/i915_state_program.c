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

#include "i915_context.h"
#include "i915_fpc.h"
#include "i915_cache.h"





/*********************************************************************************
 * Program instructions (and decls)
 */


static void upload_program( struct intel_context *intel )
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
    * text, just store the compiled version in the fragment program
    * struct.
    */
   if (!fp->translated) {
      i915_compile_fragment_program(i915, fp);
   }

   /* This is an unnnecessary copy - fix the interface...
    */
   {
      struct i915_cache_packet packet;

      packet_init( &packet, I915_CACHE_PROGRAM, fp->program_size, 0 );

      for (i = 0; i < fp->program_size; i++)
	 packet_dword( &packet, fp->program[i] );

      i915_cache_emit( i915->cctx, &packet );
   }
}


/* See i915_wm.c:
 */
const struct intel_tracked_state i915_upload_program = {
   .dirty = {
      .mesa  = (0),
      .intel   = (INTEL_NEW_FRAGMENT_PROGRAM), /* ?? Is this all ?? */
      .extra = 0
   },
   .update = upload_program
};


