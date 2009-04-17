/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 * Copyright (C) 2008-2009  Matthias Hopf
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Matthias Hopf
 *   Richard Li <RichardZ.Li@amd.com>, <richardradeon@gmail.com>
 *   CooperYuan <cooper.yuan@amd.com>, <cooperyuan@gmail.com>
 */
 
#ifndef __R600_LOCK_H__
#define __R600_LOCK_H__

#include "r600_context.h"

/*
 * Turn DEBUG_LOCKING on to find locking conflicts.
 */
#define DEBUG_LOCKING	0

#if DEBUG_LOCKING
extern char *prevLockFile;
extern int prevLockLine;

#define DEBUG_LOCK()                                            \
    do                                                          \
    {                                                           \
        prevLockFile = (__FILE__);                              \
        prevLockLine = (__LINE__);                              \
    } while (0)

#define DEBUG_RESET()                                           \
    do                                                          \
    {                                                           \
        prevLockFile = 0;                                       \
        prevLockLine = 0;                                       \
    } while (0)

#define DEBUG_CHECK_LOCK()                                      \
    do                                                          \
    {                                                           \
        if (prevLockFile)                                       \
        {                                                       \
            fprintf(stderr,                                     \
            "LOCK SET!\n\tPrevious %s:%d\n\tCurrent: %s:%d\n",  \
            prevLockFile, prevLockLine, __FILE__, __LINE__);    \
            exit(1);                                            \
        }                                                       \
    } while (0)

#else

#define DEBUG_LOCK()
#define DEBUG_RESET()
#define DEBUG_CHECK_LOCK()

#endif

/* 
 * Update the hardware state.  This is called if another context has
 * grabbed the hardware lock, which includes the X server.  This
 * function also updates the driver's window state after the X server
 * moves, resizes or restacks a window -- the change will be reflected
 * in the drawable position and clip rects.  Since the X server grabs
 * the hardware lock when it changes the window state, this routine will
 * automatically be called after such a change.
 */
static inline void r600GetLock(context_t *context, GLuint flags)
{
    __DRIdrawablePrivate *const drawable = context->currentDraw;
    __DRIdrawablePrivate *const readable = context->currentRead;
    __DRIscreenPrivate *sPriv            = context->screen->driScreen;
    drm_radeon_sarea_t *sarea            = context->sarea;

    assert(drawable != NULL);

    drmGetLock(context->fd, context->hwContext, flags);

    /* 
     * The window might have moved, so we might need to get new clip
     * rects.
     *
     * NOTE: This releases and regrabs the hw lock to allow the X server
     * to respond to the DRI protocol request for new drawable info.
     * Since the hardware state depends on having the latest drawable
     * clip rects, all state checking must be done _after_ this call.
     */
#if 0
    DRI_VALIDATE_DRAWABLE_INFO(sPriv, drawable);
    if (drawable != readable) 
    {
        DRI_VALIDATE_DRAWABLE_INFO(sPriv, readable);
    }

    if (context->lastStamp != drawable->lastStamp) 
    {
        radeonUpdatePageFlipping(context);
        radeonSetCliprects(context);
        r300UpdateViewportOffset(context->glCtx);
        driUpdateFramebufferSize(context->glCtx, drawable);
    }

    if (sarea->ctx_owner != context->dri.hwContext) 
    {
        int i;

        sarea->ctx_owner = context->dri.hwContext;
        for (i = 0; i < r300->nr_heaps; i++) 
        {
            DRI_AGE_TEXTURES(r300->texture_heaps[i]);
        }
    }

    context->lost_context = GL_TRUE;
#endif    
}


/*
 * !!! We may want to separate locks from locks with validation.  This
 * could be used to improve performance for those things commands that
 * do not do any drawing !!!
 */

/* Lock the hardware and validate our state.
 */
#define R600_LOCK_HARDWARE( context )                           \
    do                                                          \
    {                                                           \
        char __ret = 0;                                         \
        DEBUG_CHECK_LOCK();                                     \
        DRM_CAS((context)->hwLock, (context)->hwContext,        \
        (DRM_LOCK_HELD | (context)->hwContext), __ret);         \
        if (__ret)                                              \
            r600GetLock(context,0);                             \
        DEBUG_LOCK();                                           \
    } while (0)

#define R600_UNLOCK_HARDWARE( context )                         \
    do                                                          \
    {                                                           \
        DRM_UNLOCK((context)->fd,                               \
                   (context)->hwLock,                           \
                   (context)->hwContext);                       \
        DEBUG_RESET();                                          \
    } while (0)

#endif  /* __R600_LOCK_H__ */
