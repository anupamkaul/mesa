/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIAINIT_H
#define _VIAINIT_H

#include "glapi/glthread.h"
#include "via_fbo.h"
#include <sys/time.h>
#include "dri_util.h"
#include "via_dri.h"
#include "xmlconfig.h"
#include "wsbm_pool.h"
#include <errno.h>
#include "ochr_drm.h"

struct via_fencemgr;

struct via_tex_buffer
{
    struct via_tex_buffer *next, *prev;
    struct via_texture_image *image;
    unsigned long index;
    unsigned long offset;
    GLuint size;
    GLuint memType;
    unsigned char *bufAddr;
    GLuint texBase;
    GLuint lastUsed;
};

typedef struct _viaScreenPrivate
{
    int deviceID;
    int mem;
    __DRIscreenPrivate *driScrnPriv;
    unsigned int sareaPrivOffset;

    /* Configuration cache with default values for all contexts */
    driOptionCache optionCache;
    driOptionCache parsedCache;

    struct _WsbmBufferPool *mallocPool;
    struct _WsbmBufferPool *bufferPool;

    /* Dummy hw context */
#if 0
    __DRIid dummyContextID;
    drm_context_t dummyContext;
#endif

    struct _WsbmFenceMgr *fence_mgr;
    GLuint bitsPerPixel;
    GLboolean irqEnabled;
    int execIoctlOffset;

    const __DRIextension *extensions[5];
} viaScreenPrivate;

extern GLboolean
viaCreateContext(const __GLcontextModes * mesaVis,
		 __DRIcontextPrivate * driContextPriv,
		 void *sharedContextPrivate);

extern void viaDestroyContext(__DRIcontextPrivate * driContextPriv);

extern GLboolean viaUnbindContext(__DRIcontextPrivate * driContextPriv);

extern GLboolean
viaMakeCurrent(__DRIcontextPrivate * driContextPriv,
	       __DRIdrawablePrivate * driDrawPriv,
	       __DRIdrawablePrivate * driReadPriv);

extern void viaSwapBuffers(__DRIdrawablePrivate * drawablePrivate);

/*
 * Fencing functions.
 */

extern struct _WsbmFenceObject *via_fence_create(struct via_fencemgr
						 *fence_mgr);
extern void via_fence_seq_increment(struct via_fencemgr *fence_mgr,
				    uint32_t * value, uint32_t * offset);
extern uint32_t via_fence_seq(struct via_fencemgr *fence_mgr);
extern uint32_t via_fence_last_read(struct via_fencemgr *fence_mgr);

extern void via_fencemgr_takedown(struct via_fencemgr *fence_mgr);
extern struct via_fencemgr *via_fencemgr_init(const struct _viaScreenPrivate
					      *via_screen);
extern struct _WsbmBufferPool *driViaPoolInit(int fd, void *vramAddr,
					      void *agpAddr,
					      unsigned long vramOffset,
					      unsigned long agpOffset,
					      unsigned long frontOffset,
					      void *frontMap,
					      uint32_t frontHandle);
extern void via_release_delayed_buffers(struct _WsbmBufferPool *driPool);

static inline GLenum
via_sys_to_gl_err(int err)
{
    switch (-err) {
    case ENOMEM:
	return GL_OUT_OF_MEMORY;
    default:
	return GL_INVALID_OPERATION;
    }
    return GL_INVALID_OPERATION;
}
#endif
