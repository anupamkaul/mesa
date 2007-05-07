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
#include "intel_utils.h"
#include "intel_fbo.h"
#include "intel_state.h"


union i915_hw_dirty {
   struct {
      GLuint prim:2;
      GLuint immediate:8;
      GLuint indirect:6;
      GLuint pad:1;
      GLuint reserved_swz:15;
   } i915;
   struct intel_hw_dirty intel;
};


static INLINE void EMIT_DWORD( GLuint **ptr, GLuint dw )
{
   **ptr = dw;
   (*ptr)++;
}

/* XXX: assumption that ptr points somewhere in the batchbuffer!!
 */
static INLINE void EMIT_RELOC( struct intel_context *intel,
			       GLuint **ptr, 
			       struct _DriBufferObject *buffer,
			       GLuint flags,
			       GLuint mask, GLuint delta)
{
   intel_batchbuffer_set_reloc( intel->batch,
				SEGMENT_IMMEDIATE,
				((GLubyte *)(*ptr)) - intel->batch->map,
				buffer,
				flags, mask, delta );
   (*ptr)++;
}




static void emit_immediates( struct intel_context *intel,
			     GLuint **ptr,
			     const struct i915_state *state,
			     GLuint dirty )
{
   if (dirty) {
      GLuint nr = count_bits(dirty);
      GLuint i;

      EMIT_DWORD( ptr,
		  (_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		   (dirty << 4) |
		   (nr - 1)));
	 
      if (dirty & (1<<0)) {
	 
	 /* Prelocate with the NO_MOVE flag:
	  */
	 GLuint no_move = 0; // DRM_BO_FLAG_NO_MOVE;

	 EMIT_RELOC( intel,
		     ptr,
		     state->vbo,
		     DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ | no_move,
		     DRM_BO_MASK_MEM | DRM_BO_FLAG_READ | no_move,
		     state->immediate[0] );
      }

      for (i = 1; i < I915_MAX_IMMEDIATE; i++) {
	 if (dirty & (1<<i)) {
	    EMIT_DWORD( ptr,
			state->immediate[i] );
	 }
      }
   }
}




/* Emit the load indirect packet.  The actual data has already been
 * emitted to the caches.
 */
static void emit_indirect( struct intel_context *intel,
			   GLuint **ptr,
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

      EMIT_DWORD( ptr,  
		  (_3DSTATE_LOAD_INDIRECT | 
		   (dirty<<8) |
		   intel->batch->state_memtype |
		   (size - 2)));
      
      for (i = 0; i < I915_MAX_CACHE; i++) {
	 if (dirty & (1<<i)) {
	    EMIT_RELOC( intel, 
			ptr,
			intel->batch->state_buffer, 
			intel->batch->state_memflags,
			DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
			( state->offsets[i] | flag | SIS0_BUFFER_VALID ) );

	    /* No state size dword for dynamic state:
	     */
	    if (i != I915_CACHE_DYNAMIC)
	       EMIT_DWORD( ptr, 
			   state->sizes[i]-1 );
	 }
      }
   }
}

static GLuint size_indirect( GLuint dirty )
{
   if (dirty) {
      GLuint nr = count_bits(dirty);
      GLuint size = nr * 2 + 1;
      
      if (dirty & (1<<I915_CACHE_DYNAMIC))
	 size -= 1;
   
      return size * sizeof(GLuint);
   }
   else 
      return 0;
}

static GLuint size_immediate( GLuint dirty )
{
   if (dirty) {
      GLuint nr = count_bits(dirty);
      return (nr + 1) * sizeof(GLuint);
   }
   else 
      return 0;
}


static GLuint i915_get_state_size( struct intel_context *intel,
				   struct intel_hw_dirty iflags )
{
   union i915_hw_dirty flags;

   flags.intel = iflags;

   return (size_indirect(flags.i915.indirect) +
	   size_immediate(flags.i915.immediate));
}


static GLuint diff_immediate( const struct i915_state *from,
			      const struct i915_state *to )
{
   GLuint dirty = 0;
   GLuint i;

   /* Lost context?
    */
   if (from == NULL || from->id != to->id) {
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

   if (from == NULL || from->id != to->id) {
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


static struct intel_hw_dirty i915_diff_states( const void *old_state,
					       const void *new_state )
{
   const struct i915_state *old_i915_state = (const struct i915_state *)old_state;
   const struct i915_state *new_i915_state = (const struct i915_state *)new_state;
   union i915_hw_dirty flags;

   flags.i915.prim = 0;
   flags.i915.immediate = diff_immediate( old_i915_state, new_i915_state );
   flags.i915.indirect = diff_indirect( old_i915_state, new_i915_state );
   flags.i915.pad = 0;
   flags.i915.reserved_swz = 0;

   return flags.intel;
}


/* Combine packets, diff against hardware state and emit a minimal set
 * of changes.
 */
static void i915_emit_hardware_state( struct intel_context *intel,
				      GLuint *ptr,
				      const void *driver_state,
				      struct intel_hw_dirty intel_flags,
				      GLboolean force_load )
{
   const struct i915_state *state = (const struct i915_state *)driver_state;
   union i915_hw_dirty flags;
   
   flags.intel = intel_flags;

   emit_immediates( intel, &ptr, state, flags.i915.immediate );
   emit_indirect( intel, &ptr, state, flags.i915.indirect, force_load );
}


void i915_init_differencer( struct i915_context *i915 )
{
   i915->intel.state.current = (void *)(&i915->current);
   i915->intel.state.driver_state_size = sizeof(struct i915_state);

   i915->intel.vtbl.get_state_emit_size = i915_get_state_size;
   i915->intel.vtbl.emit_hardware_state = i915_emit_hardware_state;
   i915->intel.vtbl.diff_states = i915_diff_states;
}
