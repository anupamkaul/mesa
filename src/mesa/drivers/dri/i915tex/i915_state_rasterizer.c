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
 

#include "intel_context.h"
#include "intel_draw.h"
#include "i915_context.h"



static void choose_rasterizer( struct intel_context *intel )
{
   struct intel_render *render = NULL;

   /* INTEL_NEW_FALLBACK, INTEL_NEW_ACTIVE_PRIMS
    */
   if (intel->Fallback) {
      render = intel->swrender;
   }
   else if (0 & intel->active_prims & intel->fallback_prims) {
      if (intel->active_prims & ~intel->fallback_prims) {
	 render = intel->mixed; /* classic + swrast */
      }
      else {
	 render = intel->swrender;
      }
   }
#if 0
   else if (check_hwz( intel )) {
      render = intel->hwz;
   }
   else if (check_swz( intel )) {
      render = intel->swz;
   }
#endif
   else {
      render = intel->classic;
   }

   if (render != intel->current) {
      intel_draw_set_render( intel->draw, render );
      intel->current = render;
   }
}

const struct intel_tracked_state i915_choose_rasterizer = {
   .dirty = {
      .mesa = (_NEW_LINE | _NEW_POINT),
      .intel  = (INTEL_NEW_FALLBACK_PRIMS |
		 INTEL_NEW_FALLBACK |
		 INTEL_NEW_ACTIVE_PRIMS),
      .extra = 0
   },
   .update = choose_rasterizer
};





