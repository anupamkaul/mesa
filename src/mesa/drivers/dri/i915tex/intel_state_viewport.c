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


#include "context.h"
#include "colormac.h"
#include "intel_context.h"
#include "intel_fbo.h"




/**
 * Update the viewport transformation matrix.  Depends on:
 *  - viewport pos/size
 *  - depthrange
 *  - window pos/size or FBO size
 */
static void update_viewport( struct intel_context *intel )
{
   const GLframebuffer *DrawBuffer = intel->state.DrawBuffer;
   GLfloat yScale = 1.0;
   GLfloat yBias = 0.0;
   GLfloat depthScale = 1.0;

   /* _NEW_BUFFERS
    */
   if (DrawBuffer) {
      depthScale = 1.0F / DrawBuffer->_DepthMaxF;

      if (DrawBuffer->Name) {
	 /* User created FBO */
	 struct intel_renderbuffer *irb
	    = intel_renderbuffer(DrawBuffer->_ColorDrawBuffers[0][0]);
	 if (irb && !irb->RenderToTexture) {
	    /* y=0=top */
	    yScale = -1.0;
	    yBias = irb->Base.Height;
	 }
	 else {
	    /* y=0=bottom */
	    yScale = 1.0;
	    yBias = 0.0;
	 }
      }
      else {
	 /* window buffer, y=0=top */
	 yScale = -1.0;
	 yBias = DrawBuffer->Height;
      }
   }

   {
      /* _NEW_VIEWPORT 
       */
      const GLfloat *v = intel->state.Viewport->_WindowMap.m;
      GLfloat scale[4];
      GLfloat trans[4];
      

      scale[0] = v[MAT_SX];      
      scale[1] = v[MAT_SY] * yScale;
      scale[2] = v[MAT_SZ] * depthScale;
      scale[3] = 1;

      trans[0] = v[MAT_TX] + SUBPIXEL_X;
      trans[1] = v[MAT_TY] * yScale + yBias + SUBPIXEL_Y;
      trans[2] = v[MAT_TZ] * depthScale;
      trans[3] = 0;

      /* Update both hw and clip-setup viewports:
       */
      clip_set_viewport( intel->clip, scale, trans );
   }
}


const struct intel_tracked_state intel_update_clip_viewport = {
   .dirty = {
      .mesa = _NEW_BUFFERS | _NEW_VIEWPORT,
      .intel = INTEL_NEW_WINDOW_DIMENSIONS,
      .extra = 0
   },
   .update = update_viewport
};
