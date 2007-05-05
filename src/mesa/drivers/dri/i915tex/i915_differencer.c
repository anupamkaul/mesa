/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */
      
#include "i915_context.h"
#include "i915_state.h"
#include "i915_reg.h"
#include "intel_batchbuffer.h"
#include "intel_fbo.h"


static GLuint count_bits( GLuint mask )
{
   GLuint i, nr = 0;

   for (i = 1; mask >= i; i <<= 1) 
      if (mask & i)
	 nr++;

   return nr;
}

static void emit_immediates( struct intel_context *intel,
			     const struct i915_state *to,
			     GLuint dirty )
{
   if (dirty) {
      GLuint nr = count_bits(dirty);
      GLuint i;

      BEGIN_BATCH( nr + 1, 0 );
      OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		(dirty << 4) |
		(nr - 1));
	 
      if (dirty & (1<<0)) {
	 
	 /* Prelocate with the NO_MOVE flag:
	  */
	 GLuint no_move = 0; // DRM_BO_FLAG_NO_MOVE;

	 OUT_RELOC(to->vbo,
		   DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ | no_move,
		   DRM_BO_MASK_MEM | DRM_BO_FLAG_READ | no_move,
		   to->immediate[0]);
      }

      for (i = 1; i < I915_MAX_IMMEDIATE; i++) {
	 if (dirty & (1<<i)) {
	    OUT_BATCH( to->immediate[i] );
	 }
      }
	 
      ADVANCE_BATCH();
   }
}




/* Emit the load indirect packet.  The actual data has already been
 * emitted to the caches.
 */
static void emit_indirect( struct intel_context *intel,
			   const struct i915_state *state,
			   GLuint dirty,
			   GLboolean force_load )
{
   if (dirty) {
      GLuint nr = count_bits(dirty);
      GLuint size = nr * 2 + 1;
      GLuint flag = 0;
      GLuint i;

      if (force_load)
	 flag = SIS0_FORCE_LOAD;

      /* No state size dword for dynamic state:
       */
      if (dirty & (1<<I915_CACHE_DYNAMIC))
	 size -= 1;

      BEGIN_BATCH(size,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | (dirty<<8) |
		 intel->batch->state_memtype | (size - 2));

      for (i = 0; i < I915_MAX_CACHE; i++) {
	 if (dirty & (1<<i)) {
	    OUT_RELOC( intel->batch->state_buffer, 
		       intel->batch->state_memflags,
		       DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		       ( state->offsets[i] | flag | SIS0_BUFFER_VALID ) );

	    /* No state size dword for dynamic state:
	     */
	    if (i != I915_CACHE_DYNAMIC)
	       OUT_BATCH( state->sizes[i]-1 );
	 }
      }

      ADVANCE_BATCH();
   }
}



GLuint i915_get_hardware_state_size( struct intel_context *intel )
{
   return (I915_MAX_IMMEDIATE + 1 + I915_MAX_CACHE * 2 + 1);
}


static GLuint diff_immediate( const struct i915_state *from,
			      const struct i915_state *to )
{
   GLuint dirty = 0;
   GLuint i;

   /* Lost context?
    */
   if (from->id != to->id) {
      dirty = (1<<I915_MAX_IMMEDIATE) - 1;
   }
   else {
      for (i = 0; i < I915_MAX_IMMEDIATE; i++) {
	 if (from->immediate[i] != to->immediate[i]) {
	    dirty |= 1<<i;
	 }
      }

      if (from->vbo != to->vbo)
	 dirty |= 1<<I915_IMMEDIATE_S0;
   }
	    
   if (to->vbo == NULL)
      dirty &= ~(1<<I915_IMMEDIATE_S0);

   return dirty;
}


static GLuint diff_indirect( const struct i915_state *from,
			     const struct i915_state *to )
{
   GLuint dirty = 0;
   GLuint i;

   if (from->id != to->id) {
      dirty = (1<<I915_MAX_CACHE) - 1;
   }
   else {
      /* Checking the offsets is sufficient - no need to examine sizes as
       * they don't change independently of offsets.
       */
      for (i = 0; i < I915_MAX_CACHE; i++) {
	 if (from->offsets[i] != to->offsets[i]) {
	    dirty |= 1<<i;
	 }
      }
   }

   /* Don't emit empty packets:
    */
   for (i = 0; i < I915_MAX_CACHE; i++) {
      if (to->sizes[i] == 0) 
	 dirty &= ~(1<<i);
   }

   return dirty;
}

static GLuint i915_diff_states( const struct i915_state *from,
				const struct i915_state *to )
{
   GLuint dirty;
   
   dirty = (diff_indirect( from, to ) << I915_MAX_IMMEDIATE);
   dirty |= diff_immediate( from, to );

   return dirty;
} 


/* Combine packets, diff against hardware state and emit a minimal set
 * of changes.
 */
static void emit_hardware_state( struct intel_context *intel,
				 const struct i915_state *new,
				 GLuint dirty,
				 GLboolean force_load )
{
   emit_immediates( intel, new, dirty & ((1<<I915_MAX_IMMEDIATE)-1) );
   emit_indirect( intel, new, dirty >> I915_MAX_IMMEDIATE, force_load );
}


void i915_emit_hardware_state( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   const struct i915_state *new = &i915->current;
   struct i915_state *old = &i915->hardware;
   GLboolean force_load = (old->id != new->id);

   GLuint dirty = i915_diff_states( old, new );
   emit_hardware_state( intel, new, dirty, force_load );

   memcpy(old, new, sizeof(*new));
   i915->hardware_dirty = 0;
}

