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
			     const struct i915_state *from,			     
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

   if (dirty) {
      GLuint nr = count_bits(dirty);

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





/* Macro to identify whole packets that differ in any of their dwords.
 */
#define CHECK( idx, nr ) do {			\
   for (i = idx; i < idx+nr; i++) {		\
      if (to->dynamic[i] != from->dynamic[i]) {	\
	 dirty |= ((1<<nr)-1) << idx;		\
	 size += nr;				\
	 break;					\
      }						\
   }						\
} while (0)


static void emit_dynamic_indirect( struct intel_context *intel,
				   const struct i915_state *from,
				   const struct i915_state *to )
{
   GLuint size, offset, pagetop, flags;
   GLuint dirty = 0, flag = 0;
   GLuint i;

   /* Lost context? 
    */
   if (from->id != to->id) {
      dirty = (1 << I915_MAX_DYNAMIC) - 1;
      flag = DIS0_BUFFER_RESET;
      size = I915_MAX_DYNAMIC;
   }
   else {
      /* Otherwise, compare the two states 
       */
      size = 0;
      CHECK( I915_DYNAMIC_MODES4, 1 ); 
      CHECK( I915_DYNAMIC_DEPTHSCALE_0, 2 ); 
      CHECK( I915_DYNAMIC_IAB, 1 ); 
      CHECK( I915_DYNAMIC_BC_0, 2 ); 
      CHECK( I915_DYNAMIC_BFO_0, 2 ); 
      CHECK( I915_DYNAMIC_STP_0, 2 ); 
   }


   offset = intel->batch->segment_finish_offset[SEGMENT_DYNAMIC_INDIRECT];
   pagetop = ALIGN(offset, 4096);
   flags = DIS0_BUFFER_VALID;

   /* check if we cross a 4k boundary and if so pad to 4k and emit
    * full state.
    */
   if (0 && pagetop != ALIGN(offset + size * 4, 4096)) 
   {
      dirty = (1<<I915_MAX_DYNAMIC)-1;
      size = I915_MAX_DYNAMIC;
      memset(intel->batch->map + offset, 0, pagetop - offset);
      offset = pagetop;
   } 

   /* Emit:
    */
   if (dirty) {
      GLuint segment = SEGMENT_DYNAMIC_INDIRECT;
      GLuint *ptr;

      /* Emit the "load state" command,  
       */
      BEGIN_BATCH(2,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | LI0_STATE_DYNAMIC_INDIRECT | (1<<14) | 0);
      OUT_RELOC( intel->batch->buffer, 
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		 ((offset + size*4 - 4) | DIS0_BUFFER_VALID | flag) );
      ADVANCE_BATCH();

      /* This should not be possible:
       */
      assert( offset + size*4 < intel->batch->segment_max_offset[segment]);
      intel->batch->segment_finish_offset[segment] += size*4;

      ptr = (GLuint *)(intel->batch->map + offset);
      
      /* Finally emit the state: 
       */
      for (i = 0; i < I915_MAX_DYNAMIC; i++) {
	 if (dirty & (1<<i)) {
	    *ptr++ = to->dynamic[i];
	 }
      }
   }
}



static void emit_cached_indirect( struct intel_context *intel,
				  const struct i915_state *from,
				  const struct i915_state *to )
{
   GLuint flag = 0;
   GLuint dirty = 0;
   GLuint i;
   struct intel_framebuffer *intel_fb =
      (struct intel_framebuffer*)intel->ctx.DrawBuffer;

   if (from->id != to->id) {
      dirty = (1<<I915_MAX_CACHE) - 1;
      dirty &= ~(1<<I915_CACHE_ZERO); /* clear out placeholder */
      flag = SIS0_FORCE_LOAD;
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

   assert(!(dirty & (1<<I915_CACHE_ZERO)));
	    
   /* Emit the load indirect packet.  The actual data has already been
    * emitted to the caches.
    */
   if (dirty) {
      GLuint nr = count_bits(dirty);
      GLuint size = nr * 2 + 1;

      if ((dirty & (1<<0)) && intel_fb->hwz) {
	 dirty &= ~(1<<0);
	 size -= 2;
      }

      BEGIN_BATCH(size,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | (dirty<<8) | (1<<14) | (size - 2));

      for (i = 0; i < I915_MAX_CACHE; i++) {
	 if (dirty & (1<<i)) {
	    OUT_RELOC( intel->batch->buffer, 
		       DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		       DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		       ( to->offsets[i] | flag | SIS0_BUFFER_VALID ) );
	    OUT_BATCH( to->sizes[i]-1 );
	 }
      }

      ADVANCE_BATCH();
   }
}

GLuint i915_get_hardware_state_size( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   GLuint max_dwords = 0;

   /* Just return an upper bound.  The important information is
    * whether the value is zero or non-zero.
    */
   if (i915->hardware_dirty & I915_HW_IMMEDIATE)
      max_dwords += I915_MAX_IMMEDIATE + 1;

   if (i915->hardware_dirty & I915_HW_DYNAMIC_INDIRECT)
      max_dwords += 2;

   if (i915->hardware_dirty & I915_HW_CACHED_INDIRECT)
      max_dwords += (I915_MAX_CACHE-1) * 2 + 1;

   return max_dwords;
}


/* Combine packets, diff against hardware state and emit a minimal set
 * of changes.
 */
void i915_emit_hardware_state( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   const struct i915_state *new = &i915->current;
   struct i915_state *old = &i915->hardware;
   GLuint flags = i915->hardware_dirty;

   if (flags & I915_HW_IMMEDIATE)
      emit_immediates( intel, old, new );
   
   if (flags & I915_HW_DYNAMIC_INDIRECT) 
      emit_dynamic_indirect( intel, old, new );

   if (flags & I915_HW_CACHED_INDIRECT)
      emit_cached_indirect( intel, old, new );

   memcpy(old, new, sizeof(*new));
   i915->hardware_dirty = 0;
}


#if 0
static void update_hardware_dirty( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );
   i915->hardware_dirty |= intel->state.dirty.intel;
}


const struct intel_tracked_state i915_set_hardware_dirty = {
   .dirty = {
      .mesa = 0,
      .intel = (I915_NEW_DYNAMIC_INDIRECT |
		I915_NEW_CACHED_INDIRECT |
		I915_NEW_IMMEDIATE),
      .extra = 0
   },
   .update = update_hardware_dirty
};

#endif
