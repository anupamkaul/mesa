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


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

//#include "mtypes.h"
//#include "context.h"
//#include "swrast/swrast.h"

#include "intel_context.h"
#include "intel_ioctl.h"
//#include "intel_blit.h"
#include "intel_fbo.h"
//#include "intel_regions.h"
#include "intel_lock.h"
#include "drm.h"

#define FILE_DEBUG_FLAG DEBUG_IOCTL

int
intelEmitIrqLocked(struct intel_context *intel)
{
   drmI830IrqEmit ie;
   int ret, seq;

   assert(((*(int *) intel->driHwLock) & ~DRM_LOCK_CONT) ==
          (DRM_LOCK_HELD | intel->hHWContext));

   ie.irq_seq = &seq;

   ret = drmCommandWriteRead(intel->driFd, DRM_I830_IRQ_EMIT,
                             &ie, sizeof(ie));
   if (ret) {
      fprintf(stderr, "%s: drmI830IrqEmit: %d\n", __FUNCTION__, ret);
      exit(1);
   }

   DBG("%s -->  %d\n", __FUNCTION__, seq);

   return seq;
}

void
intelWaitIrq(struct intel_context *intel, int seq)
{
   int ret;

   DBG("%s %d\n", __FUNCTION__, seq);

   intel->iw.irq_seq = seq;

   do {
      ret =
         drmCommandWrite(intel->driFd, DRM_I830_IRQ_WAIT, &intel->iw,
                         sizeof(intel->iw));
   } while (ret == -EAGAIN || ret == -EINTR);

   if (ret) {
      fprintf(stderr, "%s: drmI830IrqWait: %d\n", __FUNCTION__, ret);
      exit(1);
   }
}


void
intel_batch_ioctl( struct intel_context *intel,
		   GLuint start_offset,
		   GLuint used )
{
   drmI830BatchBuffer batch;

   assert(intel->locked);
   assert(used);

   DBG("%s used %d offset %x..%x ignore_cliprects %d\n",
       __FUNCTION__,
       used, start_offset, start_offset + used);

   batch.start = start_offset;
   batch.used = used;
   batch.cliprects = 0;
   batch.num_cliprects = 0;
   batch.DR1 = 0;
   batch.DR4 = 0;

   if (drmCommandWrite(intel->driFd, DRM_I830_BATCHBUFFER, &batch,
		       sizeof(batch))) {
      fprintf(stderr, "DRM_I830_BATCHBUFFER: %d\n", -errno);
      UNLOCK_HARDWARE(intel);
      exit(1);
   }

   /* FIXME: use hardware contexts to avoid 'losing' hardware after
    * each buffer flush.
    */
   intel_lost_hardware(intel);
}




void
intel_cliprect_batch_ioctl(struct intel_context *intel,
			   GLuint start_offset,
			   GLuint used )
{
   drmI830BatchBuffer batch;
   int ret;

   assert(intel->locked);
   assert(used);

   DBG("%s used %d offset %x..%x cliprects %d\n",
       __FUNCTION__,
       used, start_offset, start_offset + used, intel->numClipRects);


   batch.start = start_offset;
   batch.used = used;
   batch.cliprects = intel->pClipRects;
   batch.num_cliprects = intel->numClipRects;
   batch.DR1 = 0;
   batch.DR4 = ((((GLuint) intel->drawX) & 0xffff) |
		(((GLuint) intel->drawY) << 16));


   do {
      ret = drmCommandWrite(intel->driFd, 
			    DRM_I830_BATCHBUFFER, &batch,
			    sizeof(batch));
   } while (ret == -EAGAIN);

   /* FIXME: use hardware contexts to avoid 'losing' hardware after
    * each buffer flush.
    */
   intel_lost_hardware(intel);
}


void
intel_cliprect_hwz_ioctl(struct intel_context *intel,
			 GLuint pf_current_page, 
			 GLuint start_offset,
			 GLuint used,
			 GLuint state_offset,
			 GLuint state_size )
{
   struct intel_framebuffer *intel_fb = intel_get_fb(intel);
   struct intel_renderbuffer *intel_rb =
      intel_get_renderbuffer(&intel_fb->Base,
			     intel_fb->Base._ColorDrawBufferMask[0] ==
			     BUFFER_BIT_FRONT_LEFT ? BUFFER_FRONT_LEFT :
			     BUFFER_BACK_LEFT);
   drm_i915_hwz_t hwz;
   int ret;

   assert(intel->locked);
   assert(used);

   DBG("%s used %d offset %x..%x nr_cliprects %d\n",
       __FUNCTION__,
       used, start_offset, start_offset + used, intel->numClipRects);


   hwz.op = DRM_I915_HWZ_RENDER;
   hwz.arg.render.bpl_num = pf_current_page;
   hwz.arg.render.batch_start = start_offset;
   hwz.arg.render.static_state_offset = state_offset;
   hwz.arg.render.static_state_size = state_size;
   hwz.arg.render.DR1 = 0;
   hwz.arg.render.DR4 = ((((GLuint) intel->drawX) & 0xffff) |
			 (((GLuint) intel->drawY) << 16));

   if (intel_fb->Base.Name == 0 && 
       intel_rb->pf_pending == intel_fb->pf_seq)
   {
      hwz.arg.render.wait_flips = intel_fb->pf_pipes;
   } else {
      hwz.arg.render.wait_flips = 0;
   }

   do {
      ret = drmCommandWrite(intel->driFd, DRM_I915_HWZ, &hwz, sizeof(hwz));
   } while (ret == -EAGAIN);

   /* FIXME: use hardware contexts to avoid 'losing' hardware after
    * each buffer flush.
    */
   intel_lost_hardware(intel);
}
