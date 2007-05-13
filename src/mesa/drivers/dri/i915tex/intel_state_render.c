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
#include "intel_frame_tracker.h"
#include "intel_fbo.h"

#include "clip/clip_context.h"



#define FILE_DEBUG_FLAG DEBUG_RENDER

static GLboolean check_hwz( struct intel_context *intel )
{
   /* _NEW_BUFFERS ???
    */
   struct intel_framebuffer *intel_fb = intel_get_fb( intel );

   return intel_fb && intel_fb->Base.Name == 0 && intel_fb->hwz;
}


static GLboolean check_swz( struct intel_context *intel )
{
   /* _NEW_BUFFERS ???
    */
   if (!intel->cmdstream.size)
      return GL_FALSE;

   /* _NEW_BUFFERS, INTEL_NEW_FRAME, INTEL_NEW_WINDOW_DIMENSIONS
    *
    * Requires a tiled surface, fbo's currently aren't (which should
    * be fixed)
    */
   if (intel->state.DrawBuffer == NULL ||
       intel->state.DrawBuffer->Name != 0 ||
       intel->numClipRects != 1 ||
       intel_frame_predict_resize( intel->ft ))
      return GL_FALSE;

   return GL_TRUE;
}


static void choose_render( struct intel_context *intel )
{
   struct clip_render *render = NULL;

   if (intel->Fallback) {
      /* INTEL_NEW_FALLBACK
       */
      render = intel->swrender;
   }
   else if (intel->clip_vb_state.active_prims & intel->fallback_prims) {
      /* INTEL_NEW_VB_STATE, INTEL_NEW_FALLBACK_PRIMS
       */
      if (0 & intel->clip_vb_state.active_prims & ~intel->fallback_prims) {
	 /* classic + swrast - not done yet */
	 render = intel->mixed; 
      }
      else {
	 render = intel->swrender;
      }
   }
   else if (check_hwz( intel )) {
      render = intel->hwz;
   }
   else if (check_swz( intel )) {
      render = intel->swz;
   }
   else {
      render = intel->classic;
   }

   if (render != intel->render) {
      DBG("%s %s\n", __FUNCTION__, render->name);
      clip_set_render( intel->clip, render );
      intel->render = render;
   }
}

const struct intel_tracked_state intel_update_clip_render = {
   .dirty = {
      .mesa = (_NEW_BUFFERS),
      .intel  = (INTEL_NEW_FALLBACK_PRIMS |
		 INTEL_NEW_FALLBACK |
		 INTEL_NEW_VB_STATE | 
		 INTEL_NEW_WINDOW_DIMENSIONS | 
		 INTEL_NEW_FRAME),
      .extra = 0
   },
   .update = choose_render
};





