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
#include "intel_swapbuffers.h"

#include "i915_context.h"
#include "i915_reg.h"
#include "i915_cache.h"
#include "intel_reg.h"
#include "intel_metaops.h"
#include "intel_state.h"
#include "intel_lock.h"
#include "context.h"
#include "utils.h"
#include "drirenderbuffer.h"
#include "framebuffer.h"
#include "swrast/swrast.h"
#include "vblank.h"


/* This block can be removed when libdrm >= 2.3.1 is required */

#ifndef DRM_VBLANK_FLIP

#define DRM_VBLANK_FLIP 0x8000000

typedef struct drm_i915_flip {
   int pipes;
} drm_i915_flip_t;

#undef DRM_IOCTL_I915_FLIP
#define DRM_IOCTL_I915_FLIP DRM_IOW(DRM_COMMAND_BASE + DRM_I915_FLIP, \
				    drm_i915_flip_t)

#endif


#define FILE_DEBUG_FLAG DEBUG_BLIT

/**
 * Copy the back color buffer to the front color buffer. 
 * Used for SwapBuffers().
 */
static void
intelCopyBuffer(struct intel_context *intel,
		const __DRIdrawablePrivate * dPriv,
                const drm_clip_rect_t * rect)
{

   const intelScreenPrivate *intelScreen = intel->intelScreen;

   DBG("%s\n", __FUNCTION__);

   if (intel->last_swap_fence) {
      driFenceFinish(intel->last_swap_fence, DRM_FENCE_TYPE_EXE, GL_TRUE);
      driFenceUnReference(intel->last_swap_fence);
      intel->last_swap_fence = NULL;
   }
   intel->last_swap_fence = intel->first_swap_fence;
   intel->first_swap_fence = NULL;

   /* The LOCK_HARDWARE is required for the cliprects.  Buffer offsets
    * should work regardless.
    */
   LOCK_HARDWARE(intel);
   UPDATE_CLIPRECTS(intel);

   if (dPriv->numClipRects) {
      struct intel_framebuffer *intel_fb = dPriv->driverPrivate;
      const struct intel_region *frontRegion
	 = intel_get_rb_region(&intel_fb->Base, BUFFER_FRONT_LEFT);
      const struct intel_region *backRegion
	 = intel_get_rb_region(&intel_fb->Base, BUFFER_BACK_LEFT);
      const int nbox = dPriv->numClipRects;
      const drm_clip_rect_t *pbox = dPriv->pClipRects;
      const int pitch = frontRegion->pitch;
      const int cpp = frontRegion->cpp;
      int i;

      ASSERT(intel_fb);
      ASSERT(intel_fb->Base.Name == 0);    /* Not a user-created FBO */
      ASSERT(frontRegion);
      ASSERT(backRegion);
      ASSERT(frontRegion->pitch == backRegion->pitch);
      ASSERT(frontRegion->cpp == backRegion->cpp);

      for (i = 0; i < nbox; i++, pbox++) {
	 drm_clip_rect_t box = *pbox;

	 if (box.x1 > box.x2 ||
	     box.y1 > box.y2 ||
	     box.x2 > intelScreen->width || 
	     box.y2 > intelScreen->height)
	    continue;

	 if (rect) {
	    if (rect->x1 > box.x1)
	       box.x1 = rect->x1;
	    if (rect->y1 > box.y1)
	       box.y1 = rect->y1;
	    if (rect->x2 < box.x2)
	       box.x2 = rect->x2;
	    if (rect->y2 < box.y2)
	       box.y2 = rect->y2;

	    if (box.x1 > box.x2 || 
		box.y1 > box.y2)
	       continue;
	 }

	 intelEmitCopyBlit( intel,
			    cpp,
			    pitch, backRegion->buffer, 0,
			    pitch, frontRegion->buffer, 0,
			    box.x1, box.y1,
			    box.x1, box.y1,
			    box.x2 - box.x1,
			    box.y2 - box.y1,
			    GL_COPY );
      }

      if (intel->first_swap_fence)
	 driFenceUnReference(intel->first_swap_fence);
      intel->first_swap_fence = intel_batchbuffer_flush(intel->batch, GL_TRUE);
      driFenceReference(intel->first_swap_fence);
   }

   UNLOCK_HARDWARE(intel);
}







/* Flip the front & back buffers
 */
static GLboolean
intelPageFlip(struct intel_context *intel, const __DRIdrawablePrivate * dPriv)
{
   struct intel_framebuffer *intel_fb = dPriv->driverPrivate;
   int ret;

   if (INTEL_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s\n", __FUNCTION__);

   assert(dPriv);
   assert(dPriv->driContextPriv);
   assert(dPriv->driContextPriv->driverPrivate);

   if (intel->intelScreen->drmMinor < 9)
      return GL_FALSE;

   intelFlush(&intel->ctx);

   ret = 0;

   LOCK_HARDWARE(intel);
   UPDATE_CLIPRECTS(intel);

   if (dPriv->numClipRects && intel_fb->pf_active) {
      drm_i915_flip_t flip;

      flip.pipes = intel_fb->pf_pipes;

      ret = drmCommandWrite(intel->driFd, DRM_I915_FLIP, &flip, sizeof(flip));
   }

   UNLOCK_HARDWARE(intel);

   if (ret || !intel_fb->pf_active)
      return GL_FALSE;

   if (!dPriv->numClipRects) {
      usleep(10000);	/* throttle invisible client 10ms */
   }

   intel_fb->pf_current_page = (intel->sarea->pf_current_page >>
				(intel_fb->pf_pipes & 0x2)) & 0x3;

   if (dPriv->numClipRects != 0) {
      intel_get_renderbuffer(&intel_fb->Base, BUFFER_FRONT_LEFT)->pf_pending =
      intel_get_renderbuffer(&intel_fb->Base, BUFFER_BACK_LEFT)->pf_pending =
	 ++intel_fb->pf_seq;
   }

   intel_flip_renderbuffers(intel_fb);
   intel_draw_buffer(&intel->ctx, &intel_fb->Base);

   return GL_TRUE;
}

static GLboolean
intelScheduleSwap(struct intel_context *intel,
		  const __DRIdrawablePrivate * dPriv, 
		  GLboolean *missed_target)
{
   struct intel_framebuffer *intel_fb = dPriv->driverPrivate;
   unsigned int interval = driGetVBlankInterval(dPriv, intel_fb->vblank_flags);
   const intelScreenPrivate *intelScreen = intel->intelScreen;
   unsigned int target;
   drm_i915_vblank_swap_t swap;
   GLboolean ret;

   if ((intel_fb->vblank_flags & VBLANK_FLAG_NO_IRQ) ||
       intelScreen->current_rotation != 0 ||
       intelScreen->drmMinor < (intel_fb->pf_active ? 9 : 6))
      return GL_FALSE;

   swap.seqtype = DRM_VBLANK_ABSOLUTE;

   if (intel_fb->vblank_flags & VBLANK_FLAG_SYNC) {
      swap.seqtype |= DRM_VBLANK_NEXTONMISS;
   } else if (interval == 0) {
      return GL_FALSE;
   }

   swap.drawable = dPriv->hHWDrawable;
   target = swap.sequence = intel_fb->vbl_seq + interval;

   if ( intel_fb->vblank_flags & VBLANK_FLAG_SECONDARY ) {
      swap.seqtype |= DRM_VBLANK_SECONDARY;
   }

   LOCK_HARDWARE(intel);
   UPDATE_CLIPRECTS(intel);

   assert(intel_frame_mode( intel->ft ) == INTEL_FT_SWAP_BUFFERS);

   if ( intel_fb->pf_active ) {
      swap.seqtype |= DRM_VBLANK_FLIP;

      intel_fb->pf_current_page = (((intel->sarea->pf_current_page >>
				     (intel_fb->pf_pipes & 0x2)) & 0x3) + 1) %
				  intel_fb->pf_num_pages;
   }

   if (!drmCommandWriteRead(intel->driFd, DRM_I915_VBLANK_SWAP, &swap,
			    sizeof(swap))) {
      intel_fb->vbl_seq = swap.sequence;
      swap.sequence -= target;
      *missed_target = swap.sequence > 0 && swap.sequence <= (1 << 23);

      intel_get_renderbuffer(&intel_fb->Base, BUFFER_BACK_LEFT)->vbl_pending =
	 intel_get_renderbuffer(&intel_fb->Base,
				BUFFER_FRONT_LEFT)->vbl_pending =
	 intel_fb->vbl_seq;

      if (swap.seqtype & DRM_VBLANK_FLIP) {
	 intel_flip_renderbuffers(intel_fb);
	 intel_draw_buffer(&intel->ctx, intel->ctx.DrawBuffer);
      }

      ret = GL_TRUE;
   } else {
      if (swap.seqtype & DRM_VBLANK_FLIP) {
	 intel_fb->pf_current_page = ((intel->sarea->pf_current_page >>
					(intel_fb->pf_pipes & 0x2)) & 0x3) %
				     intel_fb->pf_num_pages;
      }

      ret = GL_FALSE;
   }

   UNLOCK_HARDWARE(intel);

   return ret;
}
 









void
intelSwapBuffers(__DRIdrawablePrivate * dPriv)
{
   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      GET_CURRENT_CONTEXT(ctx);
      struct intel_context *intel;

      if (ctx == NULL)
	 return;

      intel = intel_context(ctx);

      if (ctx->Visual.doubleBufferMode) {
         intelScreenPrivate *screen = intel->intelScreen;
	 GLboolean missed_target;
	 struct intel_framebuffer *intel_fb = dPriv->driverPrivate;
	 int64_t ust;

	 _mesa_notifySwapBuffers(ctx);  /* flush pending tnl vertices */

         intel_frame_set_mode( intel->ft, INTEL_FT_SWAP_BUFFERS );

         if (screen->current_rotation != 0 ||
	     !intelScheduleSwap(intel, dPriv, &missed_target)) {

	    driWaitForVBlank(dPriv, &intel_fb->vbl_seq, intel_fb->vblank_flags,
			     &missed_target);

	    if (screen->current_rotation != 0 || 
		!intelPageFlip(intel, dPriv)) {
	       intelCopyBuffer(intel, dPriv, NULL);
	    }

	    if (screen->current_rotation != 0) {
	       intelRotateWindow(intel, dPriv, BUFFER_BIT_FRONT_LEFT);
	    }
	 }

	 /* XXX: do this twice, as it gets reset, eg by blits above:
	  */
         intel_frame_set_mode( intel->ft, INTEL_FT_SWAP_BUFFERS );


	 intel_fb->swap_count++;
	 (*dri_interface->getUST) (&ust);
	 if (missed_target) {
	    intel_fb->swap_missed_count++;
	    intel_fb->swap_missed_ust = ust - intel_fb->swap_ust;
	 }

	 intel_fb->swap_ust = ust;
      }
   }
   else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      fprintf(stderr, "%s: drawable has no context!\n", __FUNCTION__);
   }
}

void
intelCopySubBuffer(__DRIdrawablePrivate * dPriv, int x, int y, int w, int h)
{
   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      struct intel_context *intel =
         (struct intel_context *) dPriv->driContextPriv->driverPrivate;
      GLcontext *ctx = &intel->ctx;

      if (ctx->Visual.doubleBufferMode) {
         drm_clip_rect_t rect;
         rect.x1 = x + dPriv->x;
         rect.y1 = (dPriv->h - y - h) + dPriv->y;
         rect.x2 = rect.x1 + w;
         rect.y2 = rect.y1 + h;
         _mesa_notifySwapBuffers(ctx);  /* flush pending rendering comands */
         intelCopyBuffer(intel, dPriv, &rect);
      }
   }
   else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      fprintf(stderr, "%s: drawable has no context!\n", __FUNCTION__);
   }
}




/* Emit wait for pending flips */
GLboolean
intel_emit_wait_flips(struct intel_context *intel, 
		      GLuint **ptr )
{
   struct intel_framebuffer *intel_fb = intel_get_fb(intel);
   struct intel_renderbuffer *intel_rb =
      intel_get_renderbuffer(&intel_fb->Base,
			     intel_fb->Base._ColorDrawBufferMask[0] ==
			     BUFFER_BIT_FRONT_LEFT ? BUFFER_FRONT_LEFT :
			     BUFFER_BACK_LEFT);

   if (intel_fb->Base.Name == 0 && 
       intel_rb->pf_pending == intel_fb->pf_seq &&
       intel_fb->pf_pipes) 
   {
      GLint pf_pipes = intel_fb->pf_pipes;
      /* Wait for pending flips to take effect */
      
      (*ptr)[0] = ( (pf_pipes & 0x1) ? 
		    (MI_WAIT_FOR_EVENT | MI_WAIT_FOR_PLANE_A_FLIP) :
		    0 );

      (*ptr)[1] = ( (pf_pipes & 0x2) ? 
		    (MI_WAIT_FOR_EVENT | MI_WAIT_FOR_PLANE_B_FLIP) :
		    0 );

      *ptr += 2;

      intel_rb->pf_pending--;
      return GL_TRUE;
   }

   return GL_FALSE;
}


#define INTEL_WAIT_FLIP_BYTES (2*sizeof(int))
 
void intel_wait_flips_batch( struct intel_context *intel,
			     GLboolean do_flush ) 
{
   GLuint bytes = INTEL_WAIT_FLIP_BYTES;
   GLuint *ptr = (GLuint *)intel_batchbuffer_get_space( intel->batch, 
							0, 
							bytes,
							0 );

   if (intel_emit_wait_flips( intel, &ptr ))
   {
      /* Flush the batchbuffer.  Later call to intelSpanRenderStart will
       * ensure we wait for completion.
       */
      if (do_flush) {
	 intel_batchbuffer_flush(intel->batch, GL_TRUE);
	 intel_batchbuffer_wait_last_fence( intel->batch );
      }
   }
   else {
      intel_batchbuffer_put_back_space( intel->batch, 0,
					INTEL_WAIT_FLIP_BYTES );
   }
}
