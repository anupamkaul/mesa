/* Emit packets to preserved batch buffers, which can then be
 * referenced by the LOAD_INDIRECT command.
 *
 * Need to figure out what STATIC vs DYNAMIC state is supposed to be.
 */

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
