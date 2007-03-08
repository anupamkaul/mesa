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

static void
i915_render_start(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context(&intel->ctx);

   i915ValidateFragmentProgram(i915);
}


static void
i915_reduced_primitive_state(struct intel_context *intel, GLenum rprim)
{
}





static void
i915_emit_invarient_state(struct intel_context *intel)
{
}




/* Push the state into the sarea and/or texture memory.
 */
static void
i915_emit_state(struct intel_context *intel)
{
}

static void
i915_destroy_context(struct intel_context *intel)
{
   _tnl_free_vertices(&intel->ctx);
}


/**
 * Set the drawing regions for the color and depth/stencil buffers.
 * This involves setting the pitch, cpp and buffer ID/location.
 * Also set pixel format for color and Z rendering
 * Used for setting both regular and meta state.
 */
void
i915_state_draw_region(struct intel_context *intel,
                       struct i915_hw_state *state,
                       struct intel_region *color_region,
                       struct intel_region *depth_region)
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   GLuint value;

   ASSERT(state == &i915->state || state == &i915->meta);

   if (state->draw_region != color_region) {
      intel_region_release(&state->draw_region);
      intel_region_reference(&state->draw_region, color_region);
   }
   if (state->depth_region != depth_region) {
      intel_region_release(&state->depth_region);
      intel_region_reference(&state->depth_region, depth_region);
   }

   /* Everything else will get done later:
    */
   I915_STATECHANGE(i915, I915_UPLOAD_BUFFERS);
}


static void
i915_set_draw_region(struct intel_context *intel,
                     struct intel_region *color_region,
                     struct intel_region *depth_region)
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   i915_state_draw_region(intel, &i915->state, color_region, depth_region);
}



static void
i915_lost_hardware(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   i915->state.emitted = 0;
}

static GLuint
i915_flush_cmd(void)
{
   return MI_FLUSH | FLUSH_MAP_CACHE;
}

static void 
i915_assert_not_dirty( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context(&intel->ctx);
   struct i915_hw_state *state = i915->current;
   GLuint dirty = get_dirty(state);
   assert(!dirty);
}


void
i915InitVtbl(struct i915_context *i915)
{
   i915->intel.vtbl.destroy = i915_destroy_context;
   i915->intel.vtbl.emit_state = i915_emit_state;
   i915->intel.vtbl.lost_hardware = i915_lost_hardware;
   i915->intel.vtbl.reduced_primitive_state = i915_reduced_primitive_state;
   i915->intel.vtbl.render_start = i915_render_start;
   i915->intel.vtbl.set_draw_region = i915_set_draw_region;
   i915->intel.vtbl.update_texture_state = i915UpdateTextureState;
   i915->intel.vtbl.flush_cmd = i915_flush_cmd;
   i915->intel.vtbl.assert_not_dirty = i915_assert_not_dirty;
}
