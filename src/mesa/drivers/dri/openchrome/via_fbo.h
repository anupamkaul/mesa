/**************************************************************************
 *
 * Copyright 2006, 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
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
#ifndef VIA_FBO_H
#define VIA_FBO_H

#include <stdlib.h>
#include "utils.h"
#include "dri_util.h"
#include "via_tex.h"
#include "wsbm_pool.h"

struct via_context;

#define VIA_MAX_DRAWXOFF 8
#define VIA_RB_CLASS 0x12345679
#define VIA_MAX_SWAP_FENCES 5
#define VIA_FMT_ERROR 0xF0000000

/**
 * Derived from gl_renderbuffer.
 */
struct via_renderbuffer
{
    struct gl_renderbuffer Base;       /* must be first! */

    int isSharedFrontBuffer;

    drmSize size;
    GLuint pitch;
    GLuint bpp;
    GLuint hwformat;
    GLuint texFormat;
    char *map;
    GLuint origAdd;
    GLuint origMapAdd;

    struct _viaScreenPrivate *viaScreen;
    struct _WsbmBufferObject *buf;

    /*
     * for span render-to-texture. We need to copy
     * The whole structure since the backing
     * texImage might be deleted at any point.
     */

    struct via_texture_image viaImage;
    GLint zOffset;		       /* For 3D textures */
    StoreTexelFunc Store;

    GLuint PairedDepth;	  /**< only used if this is a depth renderbuffer */
    GLuint PairedStencil; /**< only used if this is a stencil renderbuffer */
};

struct via_framebuffer
{
    struct gl_framebuffer Base;

    int drawX;			       /* origin of drawable in draw buffer */
    int drawY;
    int xoff;			       /* xoff is needed because the hw render
				        * base address must be a multiple of
				        * a number of pixels. */

    drm_clip_rect_t allClipRect;
    GLuint numFrontClipRects;	       /* cliprects for front buffer */
    drm_clip_rect_t *pFrontClipRects;
    __DRIdrawablePrivate *dPriv;

    struct _WsbmFenceObject *swap_fences[VIA_MAX_SWAP_FENCES];

    GLuint fthrottle_mode;
    GLuint used_swap_fences;
    GLuint lastStamp;
    GLuint sizeStamp;
};

extern struct via_renderbuffer *via_create_renderbuffer(GLenum intFormat,
							__DRIscreenPrivate *
							sPriv, int common);
extern struct via_renderbuffer *via_get_renderbuffer(struct gl_framebuffer
						     *fb, GLuint attIndex);
extern void via_fbo_init(struct via_context *vmesa);

static inline struct via_framebuffer *
via_framebuffer(struct gl_framebuffer *fb)
{
    if (fb == NULL)
	return NULL;
    return containerOf(fb, struct via_framebuffer, Base);
}

static inline struct via_renderbuffer *
via_renderbuffer(struct gl_renderbuffer *rb)
{
    if (rb == NULL || rb->ClassID != VIA_RB_CLASS)
	return NULL;
    return containerOf(rb, struct via_renderbuffer, Base);
}

struct _viaScreenPrivate;

static inline struct _viaScreenPrivate *
via_vrb_screen(struct via_renderbuffer *vrb)
{
    return (struct _viaScreenPrivate *)vrb->viaScreen;
}

#endif
