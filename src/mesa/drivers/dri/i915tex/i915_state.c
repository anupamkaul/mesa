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
#include "intel_state.h"

#include "i915_context.h"
#include "i915_state.h"

#define FILE_DEBUG_FLAG DEBUG_STATE

       

/* This is used to initialize intel->state.atoms[].  We could use this
 * list directly except for a single atom, i915_constants, which
 * has a .dirty value which changes according to the parameters of the
 * current fragment and vertex programs, and so cannot be a static
 * value.
 */
const struct intel_tracked_state *atoms[] =
{
   &i915_check_fallback,
   &i915_invarient_state,	


   &intel_update_viewport,
   &intel_update_render_index,

   /* 
    */
/*    &i915_fp_choose_prog, */

   /* Scan VB:
    */
/*    &i915_vb_output_sizes, */

   /* Get compiled version of the fragment program which is mildly
    * optimized according to input sizes.  This will be cached, but
    * for now recompile on all changes.
    *
    * Also calculate vertex layout, immediate (S2,S4) state, vertex
    * size.
    */
   &i915_fp_compile_and_upload,
   
   /* Calculate vertex format, program t_vertex.c, etc: 
    */
   &i915_vertex_format,


   /* Immediate state.  Don't make any effort to combine packets yet.
    */
   &i915_upload_MODES4,
   &i915_upload_S2S4,
   &i915_upload_S5,
   &i915_upload_S6,
   &i915_upload_IAB,
   &i915_upload_BLENDCOLOR,

   /* Other state.  This will eventually be able to emit itself either
    * to the batchbuffer directly, or as indirect state.  Indirect
    * state will be subject to caching so that we get something like
    * constant state objects from the i965 driver.
    */
   &i915_upload_maps,		/* must do before samplers */
   &i915_upload_samplers,

   &i915_upload_BFO,

   &i915_upload_buffers,

   /* Note this packet has a dependency on the current primitive: 
    */
   &i915_upload_stipple, 

   &i915_upload_scissor,

   &i915_upload_S0S1,

   NULL,			/* i915_constants */
};


void i915_init_state( struct i915_context *i915 )
{
   struct intel_context *intel = &i915->intel;
   GLuint i;

   intel->driver_state.atoms = _mesa_malloc(sizeof(atoms));
   intel->driver_state.nr_atoms = sizeof(atoms)/sizeof(*atoms);
   _mesa_memcpy(intel->driver_state.atoms, atoms, sizeof(atoms));

   /* Patch in a pointer to the dynamic state atom:
    */
   for (i = 0; i < intel->driver_state.nr_atoms; i++)
      if (intel->driver_state.atoms[i] == NULL)
	 intel->driver_state.atoms[i] = &i915->constants.tracked_state;

   _mesa_memcpy(&i915->constants.tracked_state, 
		&i915_fp_upload_constants,
		sizeof(i915_fp_upload_constants));
}


void i915_destroy_state( struct i915_context *i915 )
{
   struct intel_context *intel = &i915->intel;

   if (intel->driver_state.atoms) {
      _mesa_free(intel->driver_state.atoms);
      intel->driver_state.atoms = NULL;
   }
}

