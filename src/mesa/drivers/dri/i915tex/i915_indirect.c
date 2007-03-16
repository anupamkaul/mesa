/* Emit packets to preserved batch buffers, which can then be
 * referenced by the LOAD_INDIRECT command.
 *
 * Need to figure out what STATIC vs DYNAMIC state is supposed to be.
 */



static GLuint i915_emit_indirect(struct intel_context *intel, 
				 GLuint flag,
				 const GLuint *state,
				 GLuint size )
{
   GLuint delta;
   GLuint segment;

   switch (flag) {
   case LI0_STATE_DYNAMIC_INDIRECT:
      segment = SEGMENT_DYNAMIC_INDIRECT;

      /* Dynamic indirect state is different - tell it the ending
       * address, it will execute from either the previous end address
       * or the beginning of the 4k page, depending on what it feels
       * like.
       */
      delta = ((intel->batch->segment_finish_offset[segment] + size - 4) |
	       DIS0_BUFFER_VALID | 
	       DIS0_BUFFER_RESET);


      BEGIN_BATCH(2,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | flag | (1<<14) | 0);
      OUT_RELOC( intel->batch->buffer, 
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		 delta );
      ADVANCE_BATCH();
      break;

   default:
      segment = SEGMENT_OTHER_INDIRECT;

      /* Other state is more conventional: tell the hardware the start
       * point and size.
       */
      delta = (intel->batch->segment_finish_offset[segment] |
	       SIS0_FORCE_LOAD | /* XXX: fix me */
	       SIS0_BUFFER_VALID);

      BEGIN_BATCH(3,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | flag | (1<<14) | 1);
      OUT_RELOC( intel->batch->buffer, 
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		 delta );
      OUT_BATCH( (size/4)-1 );
      ADVANCE_BATCH();

      
      break;
   }

   { 
      GLuint offset = intel->batch->segment_finish_offset[segment];
      intel->batch->segment_finish_offset[segment] += size;
      
      if (state != NULL)
	 memcpy(intel->batch->map + offset, state, size);

      return offset;
   }
}


/* "constant state", or constant-ish?? 
 */
static void emit_static_indirect_state()
{
}

/* "slow state", compared probably to the state in
 * LOAD_STATE_IMMEDIATE.
 */
static void emit_dynamic_indirect_state()
{
}

/* Need to figure out how to do driBO relocations on addresses in the
 * indirect state buffers.  When do the relocations become invalid?
 */

static void emit_sampler_state()
{
}

static void emit_map_state()
{
}

static void emit_program()
{
}

static void emit_constants()
{
}

void emit_indirect_state()
{
   /* Just emit the packet straight to batch: 
    */

   /* Look at the dirty flags and figure out what needs to be sent. 
    */
}

const struct i915_tracked_state i915_indirect_state = {
   .dirty = {
      .mesa  = 0,
      .i915   = I915_NEW_STATE_MODE,
      .indirect = (INDIRECT_NEW_STATIC |
		   INDIRECT_NEW_DYNAMIC |
		   INDIRECT_NEW_SAMPLER |
		   INDIRECT_NEW_MAP |
		   INDIRECT_NEW_PROGRAM |
		   INDIRECT_NEW_CONSTANTS);
   },
   .update = upload_indirect_state
};


#if 0
      GLuint size = I915_DYNAMIC_SIZE * 4;
      GLuint flag = i915->dyn_indirect.done_reset ? 0 : DIS0_BUFFER_RESET;
      
      GLuint delta = ( (intel->batch->segment_finish_offset[segment] + size - 4) |
		       DIS0_BUFFER_VALID | 
		       flag );

      BEGIN_BATCH(2,0);
      OUT_BATCH( _3DSTATE_LOAD_INDIRECT | LI0_STATE_DYNAMIC_INDIRECT | (1<<14) | 0);
      OUT_RELOC( intel->batch->buffer, 
		 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
		 DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,
		 delta );
      ADVANCE_BATCH();


      {
	 GLuint segment = SEGMENT_DYNAMIC_INDIRECT;
	 GLuint offset = intel->batch->segment_finish_offset[segment];
	 intel->batch->segment_finish_offset[segment] += size;
	 
	 if (state != NULL)
	    memcpy(intel->batch->map + offset, state, size);

	 return offset;
      }
#endif
