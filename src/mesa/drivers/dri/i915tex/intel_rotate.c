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

#include "intel_screen.h"
#include "intel_context.h"
#include "intel_blit.h"
#include "intel_buffers.h"
#include "intel_depthstencil.h"
#include "intel_fbo.h"
#include "intel_regions.h"
#include "intel_batchbuffer.h"
#include "intel_frame_tracker.h"

#include "i915_context.h"
#include "i915_reg.h"
#include "i915_cache.h"
#include "intel_reg.h"
#include "intel_metaops.h"
#include "intel_state.h"
#include "context.h"
#include "utils.h"
#include "drirenderbuffer.h"
#include "framebuffer.h"
#include "swrast/swrast.h"
#include "vblank.h"



/**
 * Copy the window contents named by dPriv to the rotated (or reflected)
 * color buffer.
 * srcBuf is BUFFER_BIT_FRONT_LEFT or BUFFER_BIT_BACK_LEFT to indicate the source.
 */
void
intelRotateWindow(struct intel_context *intel,
                  __DRIdrawablePrivate * dPriv, GLuint srcBuf)
{
   intelScreenPrivate *screen = intel->intelScreen;
   drm_clip_rect_t fullRect;
   struct intel_framebuffer *intel_fb;
   struct intel_region *src;
   const drm_clip_rect_t *clipRects;
   int numClipRects;
   int i;
   GLenum format, type;

   int xOrig, yOrig;
   int origNumClipRects;
   drm_clip_rect_t *origRects;

   /*
    * set up hardware state
    */
   intelFlush(&intel->ctx);

   LOCK_HARDWARE(intel);

   if (!intel->numClipRects) {
      UNLOCK_HARDWARE(intel);
      return;
   }

   intel_install_meta_state(intel);

   intel_meta_no_depth_write(intel);
   intel_meta_no_stencil_write(intel);
   intel_meta_color_mask(intel, GL_FALSE);


   /* save current drawing origin and cliprects (restored at end) */
   xOrig = intel->drawX;
   yOrig = intel->drawY;
   origNumClipRects = dPriv->numClipRects;
   origRects = dPriv->pClipRects;

   /*
    * set drawing origin, cliprects for full-screen access to rotated screen
    */
   fullRect.x1 = 0;
   fullRect.y1 = 0;
   fullRect.x2 = screen->rotatedWidth;
   fullRect.y2 = screen->rotatedHeight;
   intel->drawX = 0;
   intel->drawY = 0;
   intel->numClipRects = 1;
   intel->pClipRects = &fullRect;

   intel_meta_draw_region(intel, screen->rotated_region, NULL);    /* ? */

   intel_fb = dPriv->driverPrivate;

   if ((srcBuf == BUFFER_BIT_BACK_LEFT && !intel_fb->pf_active)) {
      src = intel_get_rb_region(&intel_fb->Base, BUFFER_BACK_LEFT);
   }
   else {
      src = intel_get_rb_region(&intel_fb->Base, BUFFER_FRONT_LEFT);
   }
      clipRects = dPriv->pClipRects;
      numClipRects = dPriv->numClipRects;

   if (src->cpp == 4) {
      format = GL_BGRA;
      type = GL_UNSIGNED_BYTE;
   }
   else {
      format = GL_BGR;
      type = GL_UNSIGNED_SHORT_5_6_5_REV;
   }

   /* set the whole screen up as a texture to avoid alignment issues */
   intel_meta_tex_rect_source(intel,
			      src, 0,
			      format, type);

   intel_meta_texture_blend_replace(intel);

   /*
    * loop over the source window's cliprects
    */
   for (i = 0; i < numClipRects; i++) {
      int srcX0 = clipRects[i].x1;
      int srcY0 = clipRects[i].y1;
      int srcX1 = clipRects[i].x2;
      int srcY1 = clipRects[i].y2;
      int j;

      struct intel_metaops_tex_vertex vertex[4];

      /* build vertices for four corners of clip rect */
      vertex[0].xyz[0] = srcX0;
      vertex[0].xyz[1] = srcY0;
      vertex[0].xyz[2] = 0;
      vertex[0].st[0]  = srcX0;
      vertex[0].st[1]  = srcY0;

      vertex[1].xyz[0] = srcX1;
      vertex[1].xyz[1] = srcY0;
      vertex[1].xyz[2] = 0;
      vertex[1].st[0]  = srcX1;
      vertex[1].st[1]  = srcY0;

      vertex[2].xyz[0] = srcX1;
      vertex[2].xyz[1] = srcY1;
      vertex[2].xyz[2] = 0;
      vertex[2].st[0]  = srcX1;
      vertex[2].st[1]  = srcY1;

      vertex[3].xyz[0] = srcX0;
      vertex[3].xyz[1] = srcY1;
      vertex[3].xyz[2] = 0;
      vertex[3].st[0]  = srcX0;
      vertex[3].st[1]  = srcY1;

      /* transform coords to rotated screen coords */
      for (j = 0; j < 4; j++) {
         matrix23TransformCoordf(&screen->rotMatrix,
                                 &vertex[j].xyz[0], 
				 &vertex[j].xyz[1]);
      }


      /* draw polygon to map source image to dest region */
      intel_meta_draw_textured_quad(intel, vertex);

   }                            /* cliprect loop */

   intel_leave_meta_state(intel);
   intel_batchbuffer_flush(intel->batch, GL_TRUE);

   /* restore original drawing origin and cliprects */
   intel->drawX = xOrig;
   intel->drawY = yOrig;
   intel->numClipRects = origNumClipRects;
   intel->pClipRects = origRects;

   UNLOCK_HARDWARE(intel);
}
