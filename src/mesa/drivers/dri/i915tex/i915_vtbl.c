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
#include "mtypes.h"
#include "imports.h"
#include "macros.h"
#include "colormac.h"

#include "tnl/t_context.h"
#include "tnl/t_vertex.h"

#include "intel_batchbuffer.h"
#include "intel_tex.h"
#include "intel_regions.h"

#include "i915_reg.h"
#include "i915_context.h"




#if 0



#define OUT(x) do {				\
  if (0) _mesa_printf("OUT(0x%08x)\n", x);		\
 *p++ = (x);					\
} while(0)

/* Push the state into the sarea and/or texture memory.
 */
static void
i915_emit_state(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   struct i915_hw_state *state = i915->current;
   GLuint dirty;
   BATCH_LOCALS;

   /* We don't hold the lock at this point, so want to make sure that
    * there won't be a buffer wrap.  
    *
    * It might be better to talk about explicit places where
    * scheduling is allowed, rather than assume that it is whenever a
    * batchbuffer fills up.
    */
   intel_batchbuffer_require_space(intel->batch, 0,
				   get_state_size(state), 0);

   /* Do this here as we may have flushed the batchbuffer above,
    * causing more state to be dirty!
    */
   dirty = get_dirty(state);

   if (INTEL_DEBUG & DEBUG_STATE)
      fprintf(stderr, "%s dirty: %x\n", __FUNCTION__, dirty);

   /* This should not change during a scene for HWZ, correct?
    *
    * If it does change, we probably have to flush everything and
    * restart.
    */
   if (dirty & (I915_UPLOAD_INVARIENT | I915_UPLOAD_BUFFERS)) {
      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "I915_UPLOAD_INVARIENT:\n");

      i915_emit_invarient_state(intel);

      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "I915_UPLOAD_BUFFERS:\n");

      /* Does this go in dynamic indirect state, or static indirect
       * state???
       */
      BEGIN_BATCH(3, 0);
      OUT_BATCH(state->Buffer[I915_DESTREG_CBUFADDR0]);
      OUT_BATCH(state->Buffer[I915_DESTREG_CBUFADDR1]);
      OUT_RELOC(state->draw_region->buffer,
                DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                state->draw_region->draw_offset);
      ADVANCE_BATCH();

      if (state->depth_region) {
	 BEGIN_BATCH(3, 0);
         OUT_BATCH(state->Buffer[I915_DESTREG_DBUFADDR0]);
         OUT_BATCH(state->Buffer[I915_DESTREG_DBUFADDR1]);
         OUT_RELOC(state->depth_region->buffer,
                   DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                   DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                   state->depth_region->draw_offset);
	 ADVANCE_BATCH();
      }

      BEGIN_BATCH(2, 0);
      OUT_BATCH(state->Buffer[I915_DESTREG_DV0]);
      OUT_BATCH(state->Buffer[I915_DESTREG_DV1]);
      ADVANCE_BATCH();

#if 0
      /* Where does scissor go?
       */
      OUT_BATCH(state->Buffer[I915_DESTREG_SENABLE]);
      OUT_BATCH(state->Buffer[I915_DESTREG_SR0]);
      OUT_BATCH(state->Buffer[I915_DESTREG_SR1]);
      OUT_BATCH(state->Buffer[I915_DESTREG_SR2]);
#endif
   }

   if (dirty & I915_UPLOAD_CTX) {
      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "I915_UPLOAD_CTX:\n");

      /* Immediate state: always goes in the batchbuffer.
       */
      BEGIN_BATCH(5, 0);
      OUT_BATCH(state->Ctx[I915_CTXREG_LI]);
      OUT_BATCH(state->Ctx[I915_CTXREG_LIS2]);
      OUT_BATCH(state->Ctx[I915_CTXREG_LIS4]);
      OUT_BATCH(state->Ctx[I915_CTXREG_LIS5]);
      OUT_BATCH(state->Ctx[I915_CTXREG_LIS6]);
      ADVANCE_BATCH();
      
      emit_indirect(intel, 
		    LI0_STATE_DYNAMIC_INDIRECT,
		    state->Ctx + I915_CTXREG_STATE4, 
		    4 * sizeof(GLuint) );
   }


   /* Combine all the dirty texture state into a single command to
    * avoid lockups on I915 hardware. 
    */
   if (dirty & I915_UPLOAD_TEX_ALL) {
      GLuint offset;
      GLuint *p;
      int i, nr = 0;

      for (i = 0; i < I915_TEX_UNITS; i++)
         if (dirty & I915_UPLOAD_TEX(i))
            nr++;

      /* A bit of a nasty kludge so that we can setup the relocation
       * information for the buffer address in the indirect state
       * packet:
       */
      offset = emit_indirect(intel, 
			     LI0_STATE_MAP,
			     NULL,
			     (2 + nr * 3) * sizeof(GLuint) );
      
      p = (GLuint *)(intel->batch->map + offset);
      
      OUT(_3DSTATE_MAP_STATE | (3 * nr));
      OUT((dirty & I915_UPLOAD_TEX_ALL) >> I915_UPLOAD_TEX_0_SHIFT);

      for (i = 0; i < I915_TEX_UNITS; i++)
	 if (dirty & I915_UPLOAD_TEX(i)) {
	    if (state->tex_buffer[i]) {	  
	       intel_batchbuffer_set_reloc( intel->batch,
					    ((GLubyte *)p) - intel->batch->map,
					    state->tex_buffer[i],
					    DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
					    DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
					    state->tex_offset[i]);
	       OUT(0);		/* placeholder */
	    }
	    else {
	       assert(i == 0);
	       assert(state == &i915->meta);
	       OUT(0);
	    }

	    OUT(state->Tex[i][I915_TEXREG_MS3]);
	    OUT(state->Tex[i][I915_TEXREG_MS4]);
	 }



      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "UPLOAD SAMPLERS:\n");

      offset = emit_indirect(intel, 
			     LI0_STATE_SAMPLER,
			     NULL,
			     (2 + nr * 3) * sizeof(GLuint) );

      
      p = (GLuint *)(intel->batch->map + offset);


      OUT(_3DSTATE_SAMPLER_STATE | (3 * nr));
      OUT((dirty & I915_UPLOAD_TEX_ALL) >> I915_UPLOAD_TEX_0_SHIFT);
      for (i = 0; i < I915_TEX_UNITS; i++) {
	 if (dirty & I915_UPLOAD_TEX(i)) {
	    OUT(state->Tex[i][I915_TEXREG_SS2]);
	    OUT(state->Tex[i][I915_TEXREG_SS3]);
	    OUT(state->Tex[i][I915_TEXREG_SS4]);
	 }
      }
   }

   if (dirty & I915_UPLOAD_PROGRAM) {
      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "I915_UPLOAD_PROGRAM:\n");

      assert((state->Program[0] & 0x1ff) + 2 == state->ProgramSize);

      if (INTEL_DEBUG & DEBUG_STATE)
         i915_disassemble_program(state->Program, state->ProgramSize);
   }


   if (dirty & I915_UPLOAD_CONSTANTS) {
      if (INTEL_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "I915_UPLOAD_CONSTANTS:\n");

   }


   state->emitted |= dirty;
}

#endif


static void
i915_destroy_context(struct intel_context *intel)
{
   _tnl_free_vertices(&intel->ctx);
}



static GLuint
i915_flush_cmd(void)
{
   return MI_FLUSH | FLUSH_MAP_CACHE;
}


void
i915InitVtbl(struct i915_context *i915)
{
   i915->intel.vtbl.destroy = i915_destroy_context;
   i915->intel.vtbl.flush_cmd = i915_flush_cmd;
}
