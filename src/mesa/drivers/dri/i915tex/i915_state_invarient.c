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

#include "intel_batchbuffer.h"
#include "i915_context.h"
#include "i915_reg.h"




/***********************************************************************
 * Misc invarient state packets
 */

static void upload_invarient_state( struct intel_context *intel )
{
   static GLuint invarient_state[] = {

      (_3DSTATE_AA_CMD |
       AA_LINE_ECAAR_WIDTH_ENABLE |
       AA_LINE_ECAAR_WIDTH_1_0 |
       AA_LINE_REGION_WIDTH_ENABLE | 
       AA_LINE_REGION_WIDTH_1_0),

      /* Could use these to reduce the size of vertices when the incoming
       * array is constant.
       */
      (_3DSTATE_DFLT_DIFFUSE_CMD),
      (0),

      (_3DSTATE_DFLT_SPEC_CMD),
      (0),

      (_3DSTATE_DFLT_Z_CMD),
      (0),

      /* We support texture crossbar via the fragment shader, rather than
       * with this mechanism.
       */
      (_3DSTATE_COORD_SET_BINDINGS |
       CSB_TCB(0, 0) |
       CSB_TCB(1, 1) |
       CSB_TCB(2, 2) |
       CSB_TCB(3, 3) |
       CSB_TCB(4, 4) |
       CSB_TCB(5, 5) | 
       CSB_TCB(6, 6) | 
       CSB_TCB(7, 7)),

      /* Setup OpenGL rasterization state:
       */
      (_3DSTATE_RASTER_RULES_CMD |
       ENABLE_POINT_RASTER_RULE |
       OGL_POINT_RASTER_RULE |
       ENABLE_LINE_STRIP_PROVOKE_VRTX |
       ENABLE_TRI_FAN_PROVOKE_VRTX |
       LINE_STRIP_PROVOKE_VRTX(1) |
       TRI_FAN_PROVOKE_VRTX(2) | 
       ENABLE_TEXKILL_3D_4D | 
       TEXKILL_4D),

      /* Need to initialize this to zero.
       */
      (_3DSTATE_LOAD_STATE_IMMEDIATE_1 | 
       I1_LOAD_S(3) | 
       (1)),
      (0),

      (_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT),
      (_3DSTATE_SCISSOR_RECT_0_CMD),
      (0),
      (0),


      /* For private depth buffers but shared color buffers, eg
       * front-buffer rendering with a private depthbuffer.  We don't do
       * this.
       */
      (_3DSTATE_DEPTH_SUBRECT_DISABLE),

      /* Disable indirect state for now.
       */
      (_3DSTATE_LOAD_INDIRECT | 0),
      (0),

      (_3DSTATE_BACKFACE_STENCIL_OPS | BFO_ENABLE_STENCIL_TWO_SIDE | 0)
   };

   {
      GLuint i;
      
      BEGIN_BATCH(sizeof(invarient_state)/4, 0);
   
      for (i = 0; i < sizeof(invarient_state)/4; i++)
	 OUT_BATCH( invarient_state[i] );

      ADVANCE_BATCH();
   }
}

const struct intel_tracked_state i915_invarient_state = {
   .dirty = {
      .mesa = 0,
      .intel = INTEL_NEW_CONTEXT, /* or less frequently? */
      .extra = 0
   },
   .update = upload_invarient_state
};



