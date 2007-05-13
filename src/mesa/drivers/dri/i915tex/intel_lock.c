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
#include "i830_dri.h"

#include "intel_context.h"
#include "intel_ioctl.h"
#include "intel_batchbuffer.h"
#include "intel_buffers.h"
#include "intel_fbo.h"
#include "intel_lock.h"

#include "drirenderbuffer.h"
#include "vblank.h"

_glthread_DECLARE_STATIC_MUTEX(lockMutex);


void UPDATE_CLIPRECTS(struct intel_context *intel)
{
   __DRIdrawablePrivate *dPriv = intel->driDrawable;
   __DRIscreenPrivate *sPriv = intel->driScreen;
   intelScreenPrivate *intelScreen = (intelScreenPrivate *) sPriv->private;
   drmI830Sarea *sarea = intel->sarea;

   if (!intel->contended_lock)
      return;

   if (INTEL_DEBUG & DEBUG_LOCK)
      _mesa_printf("%s - got contended lock\n", __progname);

   /* If the window moved, may need to set a new cliprect now.
    *
    * NOTE: This releases and regains the hw lock, so all state
    * checking must be done *after* this call:
    */
   if (dPriv)
      DRI_VALIDATE_DRAWABLE_INFO(sPriv, dPriv);

   if (sarea->width != intelScreen->width ||
       sarea->height != intelScreen->height ||
       sarea->rotation != intelScreen->current_rotation) {

      intelUpdateScreenRotation(sPriv, sarea);
   }

   if (sarea->width != intel->width ||
       sarea->height != intel->height ||
       sarea->rotation != intel->current_rotation) {
      
      /*
       * FIXME: Really only need to do this when drawing to a
       * common back- or front buffer.
       */

      /*
       * This will drop the outstanding batchbuffer on the floor
       */
//      intel_frame_set_mode( intel->ft, INTEL_FT_FLUSHED );
      assert(0);

      intel_batchbuffer_unmap(intel->batch);
      intel_batchbuffer_reset(intel->batch);
      intel_batchbuffer_unmap(intel->batch);

      /* re-emit all state */
      intel_lost_hardware(intel);

      /* force window update */
      intel->lastStamp = 0;

      intel->width = sarea->width;
      intel->height = sarea->height;
      intel->current_rotation = sarea->rotation;
   }

   /* Drawable changed?
    */
   if (dPriv && intel->lastStamp != dPriv->lastStamp) {
      intel->state.dirty.intel |= INTEL_NEW_MESA;
      intel->state.dirty.mesa |= _NEW_BUFFERS;
      intelWindowMoved(intel);
      intel->lastStamp = dPriv->lastStamp;
   }
}


void WAIT_VBLANK( struct intel_context *intel )
{
    struct intel_framebuffer *intel_fb = intel_get_fb(intel);
    struct intel_renderbuffer *intel_rb = NULL;

    if (intel_fb && intel_fb->Base.Name == 0) {
	  intel_rb =
	     intel_get_renderbuffer(&intel_fb->Base,
				    intel_fb->Base._ColorDrawBufferMask[0] ==
				    BUFFER_BIT_FRONT_LEFT ? BUFFER_FRONT_LEFT :
				    BUFFER_BACK_LEFT);
    }

    if (intel_rb && (intel_fb->vbl_waited - intel_rb->vbl_pending) > (1<<23)) {
	drmVBlank vbl;

	vbl.request.type = DRM_VBLANK_ABSOLUTE;

	if ( intel_fb->vblank_flags & VBLANK_FLAG_SECONDARY ) {
	    vbl.request.type |= DRM_VBLANK_SECONDARY;
	}

	vbl.request.sequence = intel_rb->vbl_pending;
	drmWaitVBlank(intel->driFd, &vbl);
	intel_fb->vbl_waited = vbl.reply.sequence;
    }
}


/* Lock the hardware and validate our state.  
 */
void LOCK_HARDWARE( struct intel_context *intel )
{
    char __ret=0;
    assert(!intel->locked);

    _glthread_LOCK_MUTEX(lockMutex);

    DRM_CAS(intel->driHwLock, intel->hHWContext,
        (DRM_LOCK_HELD|intel->hHWContext), __ret);

    if (__ret) {
       drmGetLock(intel->driFd, intel->hHWContext, 0);
       intel->contended_lock = 1;
    }

    if (INTEL_DEBUG & DEBUG_LOCK)
      _mesa_printf("%s - locked\n", __progname);

    intel->locked = 1;
}


  /* Unlock the hardware using the global current context 
   */
void UNLOCK_HARDWARE( struct intel_context *intel )
{
   intel->locked = 0;

   DRM_UNLOCK(intel->driFd, intel->driHwLock, intel->hHWContext);

   _glthread_UNLOCK_MUTEX(lockMutex);

   if (INTEL_DEBUG & DEBUG_LOCK)
      _mesa_printf("%s - unlocked\n", __progname);
} 

