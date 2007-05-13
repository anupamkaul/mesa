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
#include "intel_swz.h"

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
 * XXX move this into a new dri/common/cliprects.c file.
 */
GLboolean
intel_intersect_cliprects(drm_clip_rect_t * dst,
                          const drm_clip_rect_t * a,
                          const drm_clip_rect_t * b)
{
   GLint bx = b->x1;
   GLint by = b->y1;
   GLint bw = b->x2 - bx;
   GLint bh = b->y2 - by;

   if (bx < a->x1)
      bw -= a->x1 - bx, bx = a->x1;
   if (by < a->y1)
      bh -= a->y1 - by, by = a->y1;
   if (bx + bw > a->x2)
      bw = a->x2 - bx;
   if (by + bh > a->y2)
      bh = a->y2 - by;
   if (bw <= 0)
      return GL_FALSE;
   if (bh <= 0)
      return GL_FALSE;

   dst->x1 = bx;
   dst->y1 = by;
   dst->x2 = bx + bw;
   dst->y2 = by + bh;

   return GL_TRUE;
}

/**
 * Return pointer to current color drawing region, or NULL.
 */
struct intel_region *
intel_drawbuf_region(struct intel_context *intel)
{
   struct intel_renderbuffer *irbColor =
      intel_renderbuffer(intel->ctx.DrawBuffer->_ColorDrawBuffers[0][0]);
   if (irbColor)
      return irbColor->region;
   else
      return NULL;
}

/**
 * Return pointer to current color reading region, or NULL.
 */
struct intel_region *
intel_readbuf_region(struct intel_context *intel)
{
   struct intel_renderbuffer *irb
      = intel_renderbuffer(intel->ctx.ReadBuffer->_ColorReadBuffer);
   if (irb)
      return irb->region;
   else
      return NULL;
}



/**
 * Update the following fields for rendering to a user-created FBO:
 *   intel->numClipRects
 *   intel->pClipRects
 *   intel->drawX
 *   intel->drawY
 */
static void
intelSetRenderbufferClipRects(struct intel_context *intel)
{
   assert(intel->ctx.DrawBuffer->Width > 0);
   assert(intel->ctx.DrawBuffer->Height > 0);
   intel->fboRect.x1 = 0;
   intel->fboRect.y1 = 0;
   intel->fboRect.x2 = intel->ctx.DrawBuffer->Width;
   intel->fboRect.y2 = intel->ctx.DrawBuffer->Height;
   intel->numClipRects = 1;
   intel->pClipRects = &intel->fboRect;
   intel->drawX = 0;
   intel->drawY = 0;
}


/**
 * As above, but for rendering to front buffer of a window.
 * \sa intelSetRenderbufferClipRects
 */
static void
intelSetFrontClipRects(struct intel_context *intel)
{
   __DRIdrawablePrivate *dPriv = intel->driDrawable;

   if (!dPriv)
      return;

   intel->numClipRects = dPriv->numClipRects;
   intel->pClipRects = dPriv->pClipRects;
   intel->drawX = dPriv->x;
   intel->drawY = dPriv->y;
}


/**
 * As above, but for rendering to back buffer of a window.
 */
static void
intelSetBackClipRects(struct intel_context *intel)
{
   __DRIdrawablePrivate *dPriv = intel->driDrawable;
   struct intel_framebuffer *intel_fb;

   if (!dPriv)
      return;

   intel_fb = dPriv->driverPrivate;

   if (intel_fb->pf_active || dPriv->numBackClipRects == 0) {
      /* use the front clip rects */
      intel->numClipRects = dPriv->numClipRects;
      intel->pClipRects = dPriv->pClipRects;
      intel->drawX = dPriv->x;
      intel->drawY = dPriv->y;
   }
   else {
      /* use the back clip rects */
      intel->numClipRects = dPriv->numBackClipRects;
      intel->pClipRects = dPriv->pBackClipRects;
      intel->drawX = dPriv->backX;
      intel->drawY = dPriv->backY;
   }
}


/**
 * This will be called whenever the currently bound window is moved/resized.
 * XXX: actually, it seems to NOT be called when the window is only moved (BP).
 */
void
intelWindowMoved(struct intel_context *intel)
{
   __DRIdrawablePrivate *dPriv = intel->driDrawable;
   struct intel_framebuffer *intel_fb = dPriv->driverPrivate;

   if (!intel->ctx.DrawBuffer) {
      /* when would this happen? -BP */
      intelSetFrontClipRects(intel);
   }
   else if (intel->ctx.DrawBuffer->Name != 0) {
      /* drawing to user-created FBO - do nothing */
      /* Cliprects would be set from intelDrawBuffer() */
   }
   else {
      /* drawing to a window */
      switch (intel_fb->Base._ColorDrawBufferMask[0]) {
      case BUFFER_BIT_FRONT_LEFT:
         intelSetFrontClipRects(intel);
         break;
      case BUFFER_BIT_BACK_LEFT:
         intelSetBackClipRects(intel);
         break;
      default:
         /* glDrawBuffer(GL_NONE or GL_FRONT_AND_BACK): software fallback */
         intelSetFrontClipRects(intel);
      }
   }

   if (intel->intelScreen->driScrnPriv->ddxMinor >= 7) {
      drmI830Sarea *sarea = intel->sarea;
      drm_clip_rect_t drw_rect = { .x1 = dPriv->x, .x2 = dPriv->x + dPriv->w,
				   .y1 = dPriv->y, .y2 = dPriv->y + dPriv->h };
      drm_clip_rect_t pipeA_rect = { .x1 = sarea->pipeA_x, .y1 = sarea->pipeA_y,
				     .x2 = sarea->pipeA_x + sarea->pipeA_w,
				     .y2 = sarea->pipeA_y + sarea->pipeA_h };
      drm_clip_rect_t pipeB_rect = { .x1 = sarea->pipeB_x, .y1 = sarea->pipeB_y,
				     .x2 = sarea->pipeB_x + sarea->pipeB_w,
				     .y2 = sarea->pipeB_y + sarea->pipeB_h };
      GLint areaA = driIntersectArea( drw_rect, pipeA_rect );
      GLint areaB = driIntersectArea( drw_rect, pipeB_rect );
      GLuint flags = intel_fb->vblank_flags;
      GLboolean pf_active;
      GLint pf_pipes, i;

      /* Update page flipping info
       */
      pf_pipes = 0;

      if (areaA > 0)
	 pf_pipes |= 1;

      if (areaB > 0)
	 pf_pipes |= 2;

      intel_fb->pf_current_page = (intel->sarea->pf_current_page >>
				   (intel_fb->pf_pipes & 0x2)) & 0x3;

      intel_fb->pf_num_pages = intel->intelScreen->third.handle ? 3 : 2;

      pf_active = pf_pipes && (pf_pipes & intel->sarea->pf_active) == pf_pipes;

      if (INTEL_DEBUG & DEBUG_LOCK)
	 if (pf_active != intel_fb->pf_active)
	    _mesa_printf("%s - Page flipping %sactive\n", __progname,
			 pf_active ? "" : "in");

      if (pf_active) {
	 /* Sync pages between pipes if we're flipping on both at the same time */
	 if (pf_pipes == 0x3 &&	pf_pipes != intel_fb->pf_pipes &&
	     (intel->sarea->pf_current_page & 0x3) !=
	     (((intel->sarea->pf_current_page) >> 2) & 0x3)) {
	    drm_i915_flip_t flip;

	    if (intel_fb->pf_current_page ==
		(intel->sarea->pf_current_page & 0x3)) {
	       /* XXX: This is ugly, but emitting two flips 'in a row' can cause
		* lockups for unknown reasons.
		*/
               intel->sarea->pf_current_page =
		  intel->sarea->pf_current_page & 0x3;
	       intel->sarea->pf_current_page |=
		  ((intel_fb->pf_current_page + intel_fb->pf_num_pages - 1) %
		   intel_fb->pf_num_pages) << 2;

	       flip.pipes = 0x2;
	    } else {
               intel->sarea->pf_current_page =
		  intel->sarea->pf_current_page & (0x3 << 2);
	       intel->sarea->pf_current_page |=
		  (intel_fb->pf_current_page + intel_fb->pf_num_pages - 1) %
		  intel_fb->pf_num_pages;

	       flip.pipes = 0x1;
	    }

	    drmCommandWrite(intel->driFd, DRM_I915_FLIP, &flip, sizeof(flip));
	 }

	 intel_fb->pf_pipes = pf_pipes;
      }

      intel_fb->pf_active = pf_active;
      intel_flip_renderbuffers(intel_fb);
      intel_draw_buffer(&intel->ctx, intel->ctx.DrawBuffer);

      /* Update vblank info
       */
      if (areaB > areaA || (areaA == areaB && areaB > 0)) {
	 flags = intel_fb->vblank_flags | VBLANK_FLAG_SECONDARY;
      } else {
	 flags = intel_fb->vblank_flags & ~VBLANK_FLAG_SECONDARY;
      }

      if (flags != intel_fb->vblank_flags) {
	 drmVBlank vbl;
	 int i;

	 vbl.request.type = DRM_VBLANK_ABSOLUTE;

	 if ( intel_fb->vblank_flags & VBLANK_FLAG_SECONDARY ) {
	    vbl.request.type |= DRM_VBLANK_SECONDARY;
	 }

	 for (i = 0; i < intel_fb->pf_num_pages; i++) {
	    if (!intel_fb->color_rb[i])
	       continue;

	    vbl.request.sequence = intel_fb->color_rb[i]->vbl_pending;
	    drmWaitVBlank(intel->driFd, &vbl);
	 }

	 intel_fb->vblank_flags = flags;
	 driGetCurrentVBlank(dPriv, intel_fb->vblank_flags, &intel_fb->vbl_seq);
	 intel_fb->vbl_waited = intel_fb->vbl_seq;

	 for (i = 0; i < intel_fb->pf_num_pages; i++) {
	    if (intel_fb->color_rb[i])
	       intel_fb->color_rb[i]->vbl_pending = intel_fb->vbl_waited;
	 }
      }

      /* Can ZONE_INIT primitives be used for clears with zone rendering? */
      intel_fb->may_use_zone_init = intel->numClipRects > 0;

      for (i = 0; i < intel->numClipRects; i++) {
	 drm_clip_rect_t *pRect = &intel->pClipRects[i];

	 if ((pRect->x1 % ZONE_WIDTH != 0) ||
	     (pRect->y1 % ZONE_HEIGHT != 0) ||
	     ((pRect->x2 + 1) % ZONE_WIDTH != 0) ||
	     ((pRect->y2 + 1) % ZONE_HEIGHT != 0)) {
	    intel_fb->may_use_zone_init = GL_FALSE;
	    break;
	 }
      }

      /* Attempt to allocate HWZ context
       *
       * Requires a tiled surface, fbo's currently aren't (which should
       * be fixed)
       */
      if (intel_fb->Base.Name == 0 && intel->intelScreen->statePool &&
	  _mesa_getenv("INTEL_HWZ")) {
	 drm_i915_hwz_t hwz;

	 hwz.op = DRM_I915_HWZ_ALLOC;
	 hwz.arg.alloc.num_buffers = intel_fb->pf_num_pages;
	 hwz.arg.alloc.num_cliprects = intel->numClipRects;
	 hwz.arg.alloc.cliprects = (unsigned long)intel->pClipRects;

	 intel_fb->hwz = !drmCommandWrite(intel->driFd, DRM_I915_HWZ, &hwz,
					  sizeof(hwz));
      }
      else
	 assert(intel_fb->hwz == 0);
   } else {
      intel_fb->vblank_flags &= ~VBLANK_FLAG_SECONDARY;
   }

   {
      __DRIdrawablePrivate *dPriv = intel->driDrawable;
      struct gl_framebuffer *fb = (struct gl_framebuffer *) dPriv->driverPrivate;
      
      _mesa_printf("%s %dx%d\n", __FUNCTION__, dPriv->w, dPriv->h);

      intel_resize_framebuffer(&intel->ctx, fb, dPriv->w, dPriv->h);
      intel->state.dirty.intel |= INTEL_NEW_WINDOW_DIMENSIONS;


      intel_swz_note_resize(intel->swz);
      intel_frame_note_resize(intel->ft);
   }      
}







/**
 * Update the hardware state for drawing into a window or framebuffer object.
 *
 * Called by glDrawBuffer, glBindFramebufferEXT, MakeCurrent, and other
 * places within the driver.
 *
 * Basically, this needs to be called any time the current framebuffer
 * changes, the renderbuffers change, or we need to draw into different
 * color buffers.
 *
 * XXX: Make this into a tracked state atom...
 */
void
intel_draw_buffer(GLcontext * ctx, struct gl_framebuffer *fb)
{
   struct intel_context *intel = intel_context(ctx);
   struct intel_region *colorRegion, *depthRegion = NULL;
   struct intel_renderbuffer *irbDepth = NULL, *irbStencil = NULL;
   int front = 0;               /* drawing to front color buffer? */

   if (!fb) {
      /* this can happen during the initial context initialization */
      return;
   }

   /* Do this here, note core Mesa, since this function is called from
    * many places within the driver.
    */
   if (ctx->NewState & (_NEW_BUFFERS | _NEW_COLOR | _NEW_PIXEL)) {
      /* this updates the DrawBuffer->_NumColorDrawBuffers fields, etc */
      _mesa_update_framebuffer(ctx);
      /* this updates the DrawBuffer's Width/Height if it's a FBO */
      _mesa_update_draw_buffer_bounds(ctx);
   }

   if (fb->_Status != GL_FRAMEBUFFER_COMPLETE_EXT) {
      /* this may occur when we're called by glBindFrameBuffer() during
       * the process of someone setting up renderbuffers, etc.
       */
      /*_mesa_debug(ctx, "DrawBuffer: incomplete user FBO\n");*/
      return;
   }

   if (fb->Name)
      intel_validate_paired_depth_stencil(ctx, fb);

   /*
    * How many color buffers are we drawing into?
    */
   if (fb->_NumColorDrawBuffers[0] != 1) {
      /* writing to 0 or 2 or 4 color buffers */
      /*_mesa_debug(ctx, "Software rendering\n");*/
      FALLBACK(intel, INTEL_FALLBACK_DRAW_BUFFER, GL_TRUE);
      front = 1;                /* might not have back color buffer */
   }
   else {
      /* draw to exactly one color buffer */
      /*_mesa_debug(ctx, "Hardware rendering\n");*/
      FALLBACK(intel, INTEL_FALLBACK_DRAW_BUFFER, GL_FALSE);
      if (fb->_ColorDrawBufferMask[0] == BUFFER_BIT_FRONT_LEFT) {
         front = 1;
      }
   }

   /*
    * Get the intel_renderbuffer for the colorbuffer we're drawing into.
    * And set up cliprects.
    */
   if (fb->Name == 0) {
      /* drawing to window system buffer */
      if (front) {
         intelSetFrontClipRects(intel);
         colorRegion = intel_get_rb_region(fb, BUFFER_FRONT_LEFT);
      }
      else {
         intelSetBackClipRects(intel);
         colorRegion = intel_get_rb_region(fb, BUFFER_BACK_LEFT);
      }
   }
   else {
      /* drawing to user-created FBO */
      struct intel_renderbuffer *irb;
      intelSetRenderbufferClipRects(intel);
      irb = intel_renderbuffer(fb->_ColorDrawBuffers[0][0]);
      colorRegion = (irb && irb->region) ? irb->region : NULL;
   }


   /***
    *** Get depth buffer region and check if we need a software fallback.
    *** Note that the depth buffer is usually a DEPTH_STENCIL buffer.
    ***/
   if (fb->_DepthBuffer && fb->_DepthBuffer->Wrapped) {
      irbDepth = intel_renderbuffer(fb->_DepthBuffer->Wrapped);
      if (irbDepth && irbDepth->region) {
         FALLBACK(intel, INTEL_FALLBACK_DEPTH_BUFFER, GL_FALSE);
         depthRegion = irbDepth->region;
      }
      else {
         FALLBACK(intel, INTEL_FALLBACK_DEPTH_BUFFER, GL_TRUE);
         depthRegion = NULL;
      }
   }
   else {
      /* not using depth buffer */
      FALLBACK(intel, INTEL_FALLBACK_DEPTH_BUFFER, GL_FALSE);
      depthRegion = NULL;
   }

   /***
    *** Stencil buffer
    *** This can only be hardware accelerated if we're using a
    *** combined DEPTH_STENCIL buffer (for now anyway).
    ***/
   if (fb->_StencilBuffer && fb->_StencilBuffer->Wrapped) {
      irbStencil = intel_renderbuffer(fb->_StencilBuffer->Wrapped);
      if (irbStencil && irbStencil->region) {
         ASSERT(irbStencil->Base._ActualFormat == GL_DEPTH24_STENCIL8_EXT);
         FALLBACK(intel, INTEL_FALLBACK_STENCIL_BUFFER, GL_FALSE);

         if (!depthRegion)
            depthRegion = irbStencil->region;
      }
      else {
         FALLBACK(intel, INTEL_FALLBACK_STENCIL_BUFFER, GL_TRUE);
      }
   }
   else {
      /* XXX FBO: instead of FALSE, pass ctx->Stencil.Enabled ??? */
      FALLBACK(intel, INTEL_FALLBACK_STENCIL_BUFFER, GL_FALSE);
   }


   if (intel->state.draw_region != colorRegion) {
      intel_region_release(&intel->state.draw_region);
      intel_region_reference(&intel->state.draw_region, colorRegion);

      /* Raise a state flag to ensure viewport, scissor get recalculated.
       */
      intel->state.dirty.intel |= INTEL_NEW_CBUF;
   }

   if (intel->state.depth_region != depthRegion) {
      intel_region_release(&intel->state.depth_region);
      intel_region_reference(&intel->state.depth_region, depthRegion);

      /* Raise a state flag to ensure viewport, scissor get recalculated.
       */
      intel->state.dirty.intel |= INTEL_NEW_ZBUF;
   }

}


static void
intelDrawBuffer(GLcontext * ctx, GLenum mode)
{
   intel_draw_buffer(ctx, ctx->DrawBuffer);
}


static void
intelReadBuffer(GLcontext * ctx, GLenum mode)
{
   if (ctx->ReadBuffer == ctx->DrawBuffer) {
      /* This will update FBO completeness status.
       * A framebuffer will be incomplete if the GL_READ_BUFFER setting
       * refers to a missing renderbuffer.  Calling glReadBuffer can set
       * that straight and can make the drawing buffer complete.
       */
      intel_draw_buffer(ctx, ctx->DrawBuffer);
   }
   /* Generally, functions which read pixels (glReadPixels, glCopyPixels, etc)
    * reference ctx->ReadBuffer and do appropriate state checks.
    */
}


void
intelInitBufferFuncs(struct dd_function_table *functions)
{
   functions->DrawBuffer = intelDrawBuffer;
   functions->ReadBuffer = intelReadBuffer;
}
