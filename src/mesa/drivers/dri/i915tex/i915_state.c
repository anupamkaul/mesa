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
#include "dd.h"
#include "tnl/tnl.h"
#include "tnl/t_context.h"

#include "texmem.h"

#include "intel_fbo.h"
#include "intel_screen.h"
#include "intel_batchbuffer.h"

#include "i915_context.h"
#include "i915_reg.h"

#define FILE_DEBUG_FLAG DEBUG_STATE


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
       


#include "brw_context.h"
#include "brw_state.h"
#include "bufmgr.h"
#include "intel_batchbuffer.h"

/* This is used to initialize brw->state.atoms[].  We could use this
 * list directly except for a single atom, brw_constant_buffer, which
 * has a .dirty value which changes according to the parameters of the
 * current fragment and vertex programs, and so cannot be a static
 * value.
 */
const struct brw_tracked_state *atoms[] =
{
   &i915_check_fallback,
   &i915_invarient_state,


   /* 
    */
   &i915_fp_choose_prog_get_inputs,

   /* Scan VB: 
    */
   &i915_fp_input_sizes,

   /* Get compiled version of the fragment program which is mildly
    * optimized according to input sizes.  This will be cached, but
    * for now recompile on all changes.
    *
    * Also calculate vertex layout, immediate (S2,S4) state, vertex
    * size.
    */
   &i915_fp_compile,

   /* Emit compiled version of the fragment program. 
    */
   &i915_upload_fp,

   /* Immediate state.  Don't make any effort to combine packets yet.
    */
   &i915_upload_S0S1,
   &i915_upload_S2,
   &i915_upload_S4,
   &i915_upload_S5,
   &i915_upload_S6,

   /* Other state.  This will eventually be able to emit itself either
    * to the batchbuffer directly, or as indirect state.  Indirect
    * state will be subject to caching so that we get something like
    * constant state objects from the i965 driver.
    */
   &i915_upload_maps,		/* must do before samplers */
   &i915_upload_samplers,

   &i915_upload_MODES4,
   &i915_upload_BFO,
   &i915_upload_BLENDCOLOR,
   &i915_upload_IAB,

   &i915_upload_colorbuffer,
   &i915_upload_depthbuffer,

   /* Note this packet has a dependency on the current primitive: 
    */
   &i915_upload_polygon_stipple, 

   &i915_upload_scissor,

   NULL,			/* i915_constant_buffer */
};


void i915_init_state( struct i915_context *i915 )
{
   GLuint i;

   i915_init_pools(i915);
   i915_init_caches(i915);

   i915->state.atoms = _mesa_malloc(sizeof(atoms));
   i915->state.nr_atoms = sizeof(atoms)/sizeof(*atoms);
   _mesa_memcpy(i915->state.atoms, atoms, sizeof(atoms));

   /* Patch in a pointer to the dynamic state atom:
    */
   for (i = 0; i < i915->state.nr_atoms; i++)
      if (i915->state.atoms[i] == NULL)
	 i915->state.atoms[i] = &i915->curbe.tracked_state;

   _mesa_memcpy(&i915->curbe.tracked_state, 
		&i915_constant_buffer,
		sizeof(i915_constant_buffer));
}


void i915_destroy_state( struct i915_context *i915 )
{
   if (i915->state.atoms) {
      _mesa_free(i915->state.atoms);
      i915->state.atoms = NULL;
   }

   i915_destroy_caches(i915);
   i915_destroy_batch_cache(i915);
   i915_destroy_pools(i915);   
}

/***********************************************************************
 */

static GLboolean check_state( const struct i915_state_flags *a,
			      const struct i915_state_flags *b )
{
   return ((a->mesa & b->mesa) ||
	   (a->i915 & b->i915) ||
	   (a->cache & b->cache));
}

static void accumulate_state( struct i915_state_flags *a,
			      const struct i915_state_flags *b )
{
   a->mesa |= b->mesa;
   a->i915 |= b->i915;
   a->cache |= b->cache;
}


static void xor_states( struct i915_state_flags *result,
			     const struct i915_state_flags *a,
			      const struct i915_state_flags *b )
{
   result->mesa = a->mesa ^ b->mesa;
   result->i915 = a->i915 ^ b->i915;
   result->cache = a->cache ^ b->cache;
}


/***********************************************************************
 * Emit all state:
 */
void i915_validate_state( struct i915_context *i915 )
{
   struct i915_state_flags *state = &i915->state.dirty;
   GLuint i;

   state->mesa |= i915->intel.NewGLState;
   i915->intel.NewGLState = 0;

   if (i915->wrap)
      state->i915 |= I915_NEW_CONTEXT;

   if (i915->emit_state_always) {
      state->mesa |= ~0;
      state->i915 |= ~0;
   }

   /* texenv program needs to notify us somehow when this happens: 
    * Some confusion about which state flag should represent this change.
    */
   if (i915->fragment_program != i915->attribs.FragmentProgram->_Current) {
      i915->fragment_program = i915->attribs.FragmentProgram->_Current;
      i915->state.dirty.mesa |= _NEW_PROGRAM;
      i915->state.dirty.i915 |= I915_NEW_FRAGMENT_PROGRAM;
   }


   if (state->mesa == 0 &&
       state->cache == 0 &&
       state->i915 == 0)
      return;

   if (i915->state.dirty.i915 & I915_NEW_CONTEXT)
      i915_clear_batch_cache_flush(i915);



   if (INTEL_DEBUG) {
      /* Debug version which enforces various sanity checks on the
       * state flags which are generated and checked to help ensure
       * state atoms are ordered correctly in the list.
       */
      struct i915_state_flags examined, prev;      
      _mesa_memset(&examined, 0, sizeof(examined));
      prev = *state;

      for (i = 0; i < i915->state.nr_atoms; i++) {	 
	 const struct i915_tracked_state *atom = i915->state.atoms[i];
	 struct i915_state_flags generated;

	 assert(atom->dirty.mesa ||
		atom->dirty.i915 ||
		atom->dirty.cache);
	 assert(atom->update);

	 if (check_state(state, &atom->dirty)) {
	    i915->state.atoms[i]->update( i915 );
	    
/* 	    emit_foo(i915); */
	 }

	 accumulate_state(&examined, &atom->dirty);

	 /* generated = (prev ^ state)
	  * if (examined & generated)
	  *     fail;
	  */
	 xor_states(&generated, &prev, state);
	 assert(!check_state(&examined, &generated));
	 prev = *state;
      }
   }
   else {
      for (i = 0; i < Elements(atoms); i++) {	 
	 if (check_state(state, &i915->state.atoms[i]->dirty))
	    i915->state.atoms[i]->update( i915 );
      }
   }

   memset(state, 0, sizeof(*state));
}

/* 
 */


void
i915InitState(struct i915_context *i915)
{
   GLcontext *ctx = &i915->intel.ctx;

   i915_init_packets(i915);

   intelInitState(ctx);

   memcpy(&i915->initial, &i915->state, sizeof(i915->state));
   i915->current = &i915->state;
}
