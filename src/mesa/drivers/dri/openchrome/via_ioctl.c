/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright 2006, 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
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
#include <stdio.h>
#include <unistd.h>

#include "main/glheader.h"
#include "main/mtypes.h"
#include "main/macros.h"
#include "main/colormac.h"
#include "main/context.h"
#include "main/dd.h"
#include "swrast/swrast.h"
#include "../common/xmlpool/options.h"

#include "via_context.h"
#include "via_tris.h"
#include "via_ioctl.h"
#include "via_state.h"
#include "via_3d_reg.h"

#include "vblank.h"
#include "drm.h"
#include "xf86drm.h"
#include <sys/ioctl.h>
#include <errno.h>

#include "wsbm_fencemgr.h"
#include "wsbm_manager.h"
#include "via_buffer_objects.h"

#define VIA_REG_STATUS          0x400
#define VIA_REG_GEMODE          0x004
#define VIA_REG_SRCBASE         0x030
#define VIA_REG_DSTBASE         0x034
#define VIA_REG_PITCH           0x038
#define VIA_REG_SRCCOLORKEY     0x01C
#define VIA_REG_KEYCONTROL      0x02C
#define VIA_REG_SRCPOS          0x008
#define VIA_REG_DSTPOS          0x00C
#define VIA_REG_GECMD           0x000
#define VIA_REG_DIMENSION       0x010  /* width and height */
#define VIA_REG_FGCOLOR         0x018

#define VIA_GEM_8bpp            0x00000000
#define VIA_GEM_16bpp           0x00000100
#define VIA_GEM_32bpp           0x00000300
#define VIA_GEC_BLT             0x00000001
#define VIA_PITCH_ENABLE        0x80000000
#define VIA_GEC_INCX            0x00000000
#define VIA_GEC_DECY            0x00004000
#define VIA_GEC_INCY            0x00000000
#define VIA_GEC_DECX            0x00008000
#define VIA_GEC_FIXCOLOR_PAT    0x00002000

struct via_validate_buffer
{
    uint64_t flags;
    uint32_t offset;
    int po_correct;
};

struct via_reloc_savestate
{
    struct via_reloc_bufinfo *info;
    struct drm_via_reloc_header *first_header;
    struct drm_via_reloc_header *cur_header;
    struct drm_via_reloc_header old_header;
};

struct via_reloc_bufinfo
{
    struct drm_via_reloc_header *first_header;
    struct drm_via_reloc_header *cur_header;
};

static void
dump_dma(struct via_context *vmesa)
{
    GLuint i;
    GLuint *data = (GLuint *) vmesa->dma;

    for (i = 0; i < vmesa->dmaLow; i += 16) {
	fprintf(stderr, "%04x:   ", i);
	fprintf(stderr, "%08x  ", *data++);
	fprintf(stderr, "%08x  ", *data++);
	fprintf(stderr, "%08x  ", *data++);
	fprintf(stderr, "%08x\n", *data++);
    }
    fprintf(stderr, "******************************************\n");
    fflush(stderr);
}

#if 0
int
via_intersect_drm_rect(struct drm_via_clip_rect *out,
		       struct drm_via_clip_rect *a, struct drm_clip_rect *b)
{
    *out = *a;

    if (0)
	fprintf(stderr, "intersect %d,%d %d,%d and %d,%d %d,%d\n",
		a->x1, a->y1, a->x2, a->y2, b->x1, b->y1, b->x2, b->y2);

    if (b->x1 > out->x1)
	out->x1 = b->x1;
    if (b->x2 < out->x2)
	out->x2 = b->x2;
    if (out->x1 >= out->x2)
	return 0;

    if (b->y1 > out->y1)
	out->y1 = b->y1;
    if (b->y2 < out->y2)
	out->y2 = b->y2;
    if (out->y1 >= out->y2)
	return 0;

    return 1;
}
#endif

int
via_intersect_via_rect(struct drm_via_clip_rect *out,
		       struct drm_via_clip_rect *a,
		       struct drm_via_clip_rect *b)
{
    *out = *a;

    if (0)
	fprintf(stderr, "intersect %d,%d %d,%d and %d,%d %d,%d\n",
		a->x1, a->y1, a->x2, a->y2, b->x1, b->y1, b->x2, b->y2);

    if (b->x1 > out->x1)
	out->x1 = b->x1;
    if (b->x2 < out->x2)
	out->x2 = b->x2;
    if (out->x1 >= out->x2)
	return 0;

    if (b->y1 > out->y1)
	out->y1 = b->y1;
    if (b->y2 < out->y2)
	out->y2 = b->y2;
    if (out->y1 >= out->y2)
	return 0;

    return 1;
}

void
viaCheckDma(struct via_context *vmesa, GLuint bytes)
{
    VIA_FINISH_PRIM(vmesa);
    if (vmesa->dmaLow + bytes > VIA_DMA_HIGHWATER) {
	viaFlushDma(vmesa);
    }
}

#define SetReg2DAGP(nReg, nData) do {		\
    OUT_RING( ((nReg) >> 2) | 0xF0000000 );	\
    OUT_RING( nData );				\
} while (0)

void
viaBlit(struct via_context *vmesa, GLuint bpp,
	struct _WsbmBufferObject *srcBuf,
	struct _WsbmBufferObject *dstBuf,
	GLuint srcDelta, GLuint srcPitch,
	GLuint dstDelta, GLuint dstPitch,
	GLint xdir, GLint ydir,
	GLuint w, GLuint h, GLuint blitMode, GLuint color, GLuint nMask)
{
    GLuint dwGEMode, cmd;
    uint32_t *fillin;
    uint32_t pos = 0;
    int ret;

    RING_VARS;

    if (!w || !h)
	return;

    switch (bpp) {
    case 16:
	dwGEMode = VIA_GEM_16bpp;
	break;
    case 32:
	dwGEMode = VIA_GEM_32bpp;
	break;
    default:
	return;
    }

    switch (blitMode) {
    case VIA_BLIT_FILL:
	cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | (VIA_BLIT_FILL << 24);
	break;
    case VIA_BLIT_COPY:
	cmd = VIA_GEC_BLT | (VIA_BLIT_COPY << 24);
	break;
    default:
	return;
    }

    if (xdir) {
	cmd |= VIA_GEC_DECX;
	pos += (w - 1);
    }

    if (ydir) {
	cmd |= VIA_GEC_DECY;
	pos += ((h - 1) << 16);
    }

    BEGIN_RING(22);
    SetReg2DAGP(VIA_REG_GEMODE, dwGEMode);
    SetReg2DAGP(VIA_REG_FGCOLOR, color);
    SetReg2DAGP(0x2C, nMask);

    fillin = ACCESS_RING();
    SetReg2DAGP(VIA_REG_SRCBASE, 0);
    SetReg2DAGP(VIA_REG_SRCPOS, 0);
    ret = via_2d_relocation(vmesa, &fillin, srcBuf,
			    srcDelta, bpp, pos,
			    WSBM_PL_FLAG_VRAM |
			    VIA_ACCESS_READ,
			    WSBM_PL_MASK_MEM | VIA_ACCESS_READ);
    assert(ret == 0);

    SetReg2DAGP(VIA_REG_DSTBASE, 0);
    SetReg2DAGP(VIA_REG_DSTPOS, 0);
    ret = via_2d_relocation(vmesa, &fillin, dstBuf,
			    dstDelta, bpp, pos,
			    WSBM_PL_FLAG_VRAM |
			    VIA_ACCESS_WRITE,
			    WSBM_PL_MASK_MEM | VIA_ACCESS_WRITE);
    assert(ret == 0);

    SetReg2DAGP(VIA_REG_PITCH, VIA_PITCH_ENABLE |
		(srcPitch >> 3) | ((dstPitch >> 3) << 16));
    SetReg2DAGP(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    SetReg2DAGP(VIA_REG_GECMD, cmd);
    SetReg2DAGP(0x2C, 0x00000000);
    ADVANCE_RING();
}

static void
viaFillBuffer(struct via_context *vmesa,
	      struct via_renderbuffer *buffer,
	      struct drm_via_clip_rect *pbox,
	      struct drm_via_clip_rect *pbox2, int nboxes, GLuint pixel,
	      GLuint mask)
{
    GLuint bytePerPixel = buffer->bpp >> 3;
    GLuint i;
    int x;
    int y;
    int w;
    int h;
    int offset;

    for (i = 0; i < nboxes; i++) {
	if (pbox2) {
	    x = pbox2[i].x1;
	    y = pbox2[i].y1;
	    w = pbox2[i].x2 - pbox2[i].x1;
	    h = pbox2[i].y2 - pbox2[i].y1;
	} else {
	    x = pbox[i].x1;
	    y = pbox[i].y1;
	    w = pbox[i].x2 - pbox[i].x1;
	    h = pbox[i].y2 - pbox[i].y1;
	}

	offset = (buffer->origAdd +
		  y * buffer->pitch + (x + vmesa->drawXoff) * bytePerPixel);

	viaBlit(vmesa,
		buffer->bpp,
		buffer->buf,
		buffer->buf,
		offset, buffer->pitch,
		offset, buffer->pitch, 0, 0, w, h, VIA_BLIT_FILL, pixel,
		mask);
    }
}

static __inline__ GLuint
viaPackColor(GLuint bpp, GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    switch (bpp) {
    case 16:
	return PACK_COLOR_565(r, g, b);
    case 32:
	return PACK_COLOR_8888(a, r, g, b);
    default:
	assert(0);
	return 0;
    }
}

#define VIA_FRONT   (1 << 0)
#define VIA_BACK    (1 << 1)
#define VIA_DEPTH   (1 << 2)
#define VIA_STENCIL (1 << 3)
#define VIA_COLOR0 (VIA_STENCIL << 1)

static void
viaClear(GLcontext * ctx, GLbitfield mask)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_framebuffer *viafb = viaDrawFrameBuffer(vmesa);
    struct via_renderbuffer *viarb;
    __DRIdrawablePrivate *dPriv = vmesa->driDrawable;
    int flag = 0;
    GLuint i;
    GLuint clear_depth_mask = 0xf << 28;
    GLuint clear_depth = 0;

    VIA_FLUSH_DMA(vmesa);

    if (mask & BUFFER_BIT_FRONT_LEFT) {
	flag |= VIA_FRONT;
	mask &= ~BUFFER_BIT_FRONT_LEFT;
    }

    if (mask & BUFFER_BIT_BACK_LEFT) {
	flag |= VIA_BACK;
	mask &= ~BUFFER_BIT_BACK_LEFT;
    }

    for (i = 0; i < 8; i++) {
	if (mask & (BUFFER_BIT_COLOR0 << i)) {
	    flag |= VIA_COLOR0 << i;
	    mask &= ~(BUFFER_BIT_COLOR0 << i);
	}
    }

    if (mask & BUFFER_BIT_DEPTH) {
	viarb = viaDrawDepthRenderBuffer(vmesa);
	if (viarb) {
	    flag |= VIA_DEPTH;
	    clear_depth = (GLuint) (ctx->Depth.Clear *
				    (viarb->Base.DepthBits ==
				     24 ? 0xffffff00
				     : ((1 << viarb->Base.DepthBits) - 1)));
	    clear_depth_mask &=
		~((viarb->Base.DepthBits == 24 ? 0xe : 0xf) << 28);

	    mask &= ~BUFFER_BIT_DEPTH;
	}
    }

    if (mask & BUFFER_BIT_STENCIL) {
	if (viaDrawDepthRenderBuffer(vmesa)) {
	    if ((ctx->Stencil.WriteMask[0] & 0xff) == 0xff) {
		flag |= VIA_DEPTH;
		clear_depth &= ~0xff;
		clear_depth |= (ctx->Stencil.Clear & 0xff);
		clear_depth_mask &= ~(0x1 << 28);

		mask &= ~BUFFER_BIT_STENCIL;
	    } else {
		if (VIA_DEBUG & DEBUG_2D)
		    fprintf(stderr, "Clear stencil writemask %x\n",
			    ctx->Stencil.WriteMask[0]);
	    }
	}
    }

    /* 16bpp doesn't support masked clears */
    if (vmesa->ClearMask & 0xf0000000) {
	if (vmesa->viaScreen->bitsPerPixel == 16) {
	    if (flag & VIA_FRONT)
		mask |= BUFFER_BIT_FRONT_LEFT;
	    if (flag & VIA_BACK)
		mask |= BUFFER_BIT_BACK_LEFT;
	    flag &= ~(VIA_FRONT | VIA_BACK);
	}

	for (i = 0; i < 8; i++) {
	    if ((flag & (VIA_COLOR0 << i)) &&
		via_get_renderbuffer(&viafb->Base,
				     BUFFER_COLOR0 + i)->bpp == 16) {
		mask |= (BUFFER_BIT_COLOR0 << i);
		flag &= ~(VIA_COLOR0 << i);
	    }
	}
    }

    if (flag) {
	struct drm_via_clip_rect *boxes = NULL, *tmp_boxes = NULL;
	struct drm_via_clip_rect *boxes2 = NULL;

	int nr = 0;
	GLint cx, cy, cw, ch;
	GLboolean all;

	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);

	/* get region after locking: */
	cx = ctx->DrawBuffer->_Xmin;
	cy = ctx->DrawBuffer->_Ymin;
	cw = ctx->DrawBuffer->_Xmax - cx;
	ch = ctx->DrawBuffer->_Ymax - cy;

	all = (cw == ctx->DrawBuffer->Width && ch == ctx->DrawBuffer->Height);

	if (viafb->Base.Name == 0) {
	    /* flip top to bottom */
	    cy = dPriv->h - cy - ch;
	}

	if (!all) {
	    /* create a cliprect of the selcetion in window coords */
	    struct drm_via_clip_rect a;

	    viaCreateSafeClip(&a, cx, cy, cw, ch);

	    struct drm_via_clip_rect *b = vmesa->pDrawClipRects;

	    boxes = tmp_boxes =
		(struct drm_via_clip_rect *)malloc(vmesa->numDrawClipRects *
						   sizeof(struct
							  drm_via_clip_rect));
	    if (!boxes) {
		UNLOCK_HARDWARE(vmesa);
		return;
	    }

	    for (i = 0; i < vmesa->numDrawClipRects; i++) {
		if (via_intersect_via_rect(&boxes[nr], &a, &b[i]))
		    nr++;
	    }
	} else {
	    boxes2 = vmesa->pDrawClipRects;
	    nr = vmesa->numDrawClipRects;
	}

	if (flag & VIA_FRONT) {
	    viarb = via_get_renderbuffer(&viafb->Base, BUFFER_FRONT_LEFT);
	    if (viarb) {
		viaFillBuffer(vmesa, viarb, boxes, boxes2, nr,
			      viaPackColor(viarb->bpp, vmesa->ClearColor[0],
					   vmesa->ClearColor[1],
					   vmesa->ClearColor[2],
					   vmesa->ClearColor[3]),
			      vmesa->ClearMask);
	    }
	}

	if (flag & VIA_BACK) {
	    viarb = via_get_renderbuffer(&viafb->Base, BUFFER_BACK_LEFT);
	    if (viarb) {
		viaFillBuffer(vmesa, viarb, boxes, boxes2, nr,
			      viaPackColor(viarb->bpp, vmesa->ClearColor[0],
					   vmesa->ClearColor[1],
					   vmesa->ClearColor[2],
					   vmesa->ClearColor[3]),
			      vmesa->ClearMask);
	    }
	}

	for (i = 0; i < 8; i++) {
	    if (flag & (VIA_COLOR0 << i)) {
		viarb = via_get_renderbuffer(&viafb->Base, BUFFER_COLOR0 + i);
		if (viarb) {
		    viaFillBuffer(vmesa, viarb, boxes, boxes2, nr,
				  viaPackColor(viarb->bpp,
					       vmesa->ClearColor[0],
					       vmesa->ClearColor[1],
					       vmesa->ClearColor[2],
					       vmesa->ClearColor[3]),
				  vmesa->ClearMask);
		}
	    }
	}

	if (flag & VIA_DEPTH) {
	    viarb = viaDrawDepthRenderBuffer(vmesa);
	    if (viarb)
		viaFillBuffer(vmesa, viarb, boxes, boxes2, nr, clear_depth,
			      clear_depth_mask);
	}

	via_execbuf(vmesa, VIA_NO_CLIPRECTS);
	UNLOCK_HARDWARE(vmesa);

	if (tmp_boxes)
	    free(tmp_boxes);
    }

    if (mask)
	_swrast_Clear(ctx, mask);
}

static void
viaDoSwapBuffers(struct via_context *vmesa,
		 struct via_framebuffer *vfb,
		 struct drm_via_clip_rect * backClipRect)
{
    GLuint bytePerPixel = vmesa->viaScreen->bitsPerPixel >> 3;
    struct via_renderbuffer *front = via_get_renderbuffer(&vfb->Base,
							  BUFFER_FRONT_LEFT);
    struct via_renderbuffer *back = via_get_renderbuffer(&vfb->Base,
							 BUFFER_BACK_LEFT);
    struct drm_via_clip_rect intersectRect;
    struct drm_via_clip_rect *b = vfb->pFrontClipRects;
    GLuint i;
    GLint x;
    GLint y;
    GLint w;
    GLint h;
    GLuint src;
    GLuint dest;
    GLuint nbox;

    nbox = vfb->numFrontClipRects;

    for (i = 0; i < nbox; i++, b++) {

	if (!via_intersect_via_rect(&intersectRect, backClipRect, b))
	    continue;

	x = intersectRect.x1;
	y = intersectRect.y1;
	w = intersectRect.x2 - intersectRect.x1;
	h = intersectRect.y2 - intersectRect.y1;

	src = y * back->pitch + x * bytePerPixel;
	dest =
	    front->origAdd + y * front->pitch + (x +
						 vfb->xoff) * bytePerPixel;

	viaBlit(vmesa,
		bytePerPixel << 3,
		back->buf,
		front->buf,
		src, back->pitch,
		dest, front->pitch, 0, 0, w, h, VIA_BLIT_COPY, 0, 0);
    }
    via_execbuf(vmesa, VIA_NO_CLIPRECTS);
}

/* Wait for command stream to be processed *and* the next vblank to
 * occur.  Equivalent to calling WAIT_IDLE() and then WaitVBlank,
 * except that WAIT_IDLE() will spin the CPU polling, while this is
 * IRQ driven.
 */
static void
viaWaitIdleVBlank(__DRIdrawablePrivate * dPriv, struct via_context *vmesa)
{
    GLboolean missed_target;
    __DRIscreenPrivate *sPriv = dPriv->driScreenPriv;

    VIA_FLUSH_DMA(vmesa);

    do {
	driWaitForVBlank(dPriv, &missed_target);
	if (missed_target) {
	    vmesa->swap_missed_count++;
	    (sPriv->systemTime->getUST) (&vmesa->swap_missed_ust);
	}
	if (vmesa->last_fence == NULL)
	    break;

	if (wsbmFenceSignaled(vmesa->last_fence, 0x1))
	    wsbmFenceUnreference(&vmesa->last_fence);
    } while (vmesa->last_fence != NULL);

    vmesa->swap_count++;
}

static void
viaDoPageFlipLocked(struct via_context *vmesa, GLuint offset)
{
    RING_VARS;

    if (VIA_DEBUG & DEBUG_2D)
	fprintf(stderr, "%s %x\n", __FUNCTION__, offset);

    if (!vmesa->nDoneFirstFlip) {
	vmesa->nDoneFirstFlip = GL_TRUE;
	BEGIN_RING(4);
	OUT_RING(HALCYON_HEADER2);
	OUT_RING(0x00fe0000);
	OUT_RING(0x0000000e);
	OUT_RING(0x0000000e);
	ADVANCE_RING();
    }

    BEGIN_RING(4);
    OUT_RING(HALCYON_HEADER2);
    OUT_RING(0x00fe0000);
    OUT_RING((HC_SubA_HFBBasL << 24) | (offset & 0xFFFFF8) | 0x2);
    OUT_RING((HC_SubA_HFBDrawFirst << 24) |
	     ((offset & 0xFF000000) >> 24) | 0x0100);
    ADVANCE_RING();

    vmesa->pfCurrentOffset = vmesa->sarea->pfCurrentOffset = offset;

    via_execbuf(vmesa, VIA_NO_CLIPRECTS);	/* often redundant */
}

void
viaResetPageFlippingLocked(struct via_context *vmesa)
{
    if (VIA_DEBUG & DEBUG_2D)
	fprintf(stderr, "%s\n", __FUNCTION__);

    struct via_framebuffer *viafb = viaDrawFrameBuffer(vmesa);
    struct via_renderbuffer *front = via_get_renderbuffer(&viafb->Base,
							  BUFFER_FRONT_LEFT);
    struct via_renderbuffer *back = via_get_renderbuffer(&viafb->Base,
							 BUFFER_BACK_LEFT);

    viaDoPageFlipLocked(vmesa, 0);

    if (wsbmBOOffsetHint(front->buf) != 0) {
	struct via_renderbuffer buffer_tmp;
	memcpy(&buffer_tmp, back, sizeof(struct via_renderbuffer));
	memcpy(back, front, sizeof(struct via_renderbuffer));
	memcpy(front, &buffer_tmp, sizeof(struct via_renderbuffer));
    }

    assert(wsbmBOOffsetHint(front->buf) == 0);
    vmesa->doPageFlip = vmesa->allowPageFlip = 0;
}

/*
 * Copy the back buffer to the front buffer.
 */
void
viaCopyBuffer(__DRIdrawablePrivate * dPriv)
{
    GET_CURRENT_CONTEXT(ctx);
    struct via_context *vmesa;
    struct gl_framebuffer *fb = dPriv->driverPrivate;
    struct via_framebuffer *vfb = via_framebuffer(fb);
    struct drm_via_clip_rect backClipRect;
    __DRIscreenPrivate *sPriv = dPriv->driScreenPriv;
    int i;

    if (ctx == NULL) {
	/*
	 * FIXME: Handle this with the dummy context.
	 */
	return;
    }

    vmesa = VIA_CONTEXT(ctx);
    VIA_FLUSH_DMA(vmesa);

    if (vmesa->vblank_flags == VBLANK_FLAG_SYNC)
	viaWaitIdleVBlank(dPriv, vmesa);
    else if (vfb->swap_fences[0]) {
	wsbmFenceFinish(vfb->swap_fences[0], 1,
			vfb->fthrottle_mode == DRI_CONF_FTHROTTLE_USLEEPS);
	wsbmFenceUnreference(&vfb->swap_fences[0]);
    }

    LOCK_HARDWARE(vmesa);
    /*
     * Save away the back cliprect before it is updated:
     */
    backClipRect = vfb->allClipRect;
    viaUpdateFrameBufferLocked(vmesa, dPriv, NULL);

#if 0
    /* Catch and cleanup situation where we were pageflipping but have
     * stopped.
     */
    if (vmesa->numDrawClipRects && vmesa->sarea->pfCurrentOffset != 0) {
	viaResetPageFlippingLocked(vmesa);
	UNLOCK_HARDWARE(vmesa);
	return;
    }
#endif

    viaDoSwapBuffers(vmesa, vfb, &backClipRect);
    viaValidateDrawablesLocked(vmesa);

    for (i = 0; i < vfb->used_swap_fences - 1; ++i) {
	vfb->swap_fences[i] = vfb->swap_fences[i + 1];
    }
    vfb->swap_fences[vfb->used_swap_fences - 1] =
	wsbmFenceReference(vmesa->last_fence);

    UNLOCK_HARDWARE(vmesa);

    (*sPriv->systemTime->getUST) (&vmesa->swap_ust);
}

void
viaPageFlip(__DRIdrawablePrivate * dPriv)
{
    struct via_context *vmesa =
	(struct via_context *)dPriv->driContextPriv->driverPrivate;
    struct via_renderbuffer buffer_tmp;
    struct via_framebuffer *viafb =
	(struct via_framebuffer *)dPriv->driverPrivate;
    struct via_renderbuffer *front = via_get_renderbuffer(&viafb->Base,
							  BUFFER_FRONT_LEFT);
    struct via_renderbuffer *back = via_get_renderbuffer(&viafb->Base,
							 BUFFER_BACK_LEFT);
    __DRIscreenPrivate *sPriv = dPriv->driScreenPriv;

    VIA_FLUSH_DMA(vmesa);
    if (vmesa->vblank_flags == VBLANK_FLAG_SYNC)
	viaWaitIdleVBlank(dPriv, vmesa);
    else {
	//FIXME:
	//  viaWaitIdleVBlank(dPriv, vmesa, vmesa->lastSwap[0]);
    }
    LOCK_HARDWARE(vmesa);
    viaDoPageFlipLocked(vmesa, wsbmBOOffsetHint(back->buf));

    UNLOCK_HARDWARE(vmesa);

    (*sPriv->systemTime->getUST) (&vmesa->swap_ust);

    /* KW: FIXME: When buffers are freed, could free frontbuffer by
     * accident:
     */
    memcpy(&buffer_tmp, back, sizeof(struct via_renderbuffer));
    memcpy(back, front, sizeof(struct via_renderbuffer));
    memcpy(front, &buffer_tmp, sizeof(struct via_renderbuffer));
}

void
viaWrapPrimitive(struct via_context *vmesa)
{
    GLenum renderPrimitive = vmesa->renderPrimitive;
    GLenum hwPrimitive = vmesa->hwPrimitive;

    if (VIA_DEBUG & DEBUG_PRIMS)
	fprintf(stderr, "%s\n", __FUNCTION__);

    if (vmesa->dmaLastPrim)
	viaFinishPrimitive(vmesa);

    viaFlushDma(vmesa);

    if (renderPrimitive != GL_POLYGON + 1)
	viaRasterPrimitive(vmesa->glCtx, renderPrimitive, hwPrimitive);

}

/*
 * This function must be safe to call from within a locked region,
 * since viaValidateDrawablesLocked may hit paths during
 * renderbuffer resizing where we need to flush DMA.
 * Hence the ugly use of vmesa->isLocked.
 */

void
viaFlushDma(struct via_context *vmesa)
{
    if (vmesa->dmaLow) {
	GLboolean doUnlock = GL_FALSE;
	struct via_renderbuffer *drawBuf = viaDrawRenderBuffer(vmesa);

	assert(!vmesa->dmaLastPrim);
	assert(drawBuf != NULL);

	if (!vmesa->isLocked) {
	    LOCK_HARDWARE(vmesa);
	    doUnlock = GL_TRUE;
	}

	if (drawBuf->isSharedFrontBuffer && vmesa->lostLock)
	    viaUpdateFrameBufferLocked(vmesa,
				       viaDrawFrameBuffer(vmesa)->dPriv,
				       NULL);
	via_execbuf(vmesa, 0);
	if (drawBuf->isSharedFrontBuffer && vmesa->lostLock)
	    viaValidateDrawablesLocked(vmesa);

	if (doUnlock)
	    UNLOCK_HARDWARE(vmesa);
    }
}

static void
viaFlush(GLcontext * ctx)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);

    VIA_FLUSH_DMA(vmesa);
}

static void
viaFinish(GLcontext * ctx)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);

    via_wait_context_idle(vmesa);
}

static void
viaClearStencil(GLcontext * ctx, int s)
{
    return;
}

void
viaInitIoctlFuncs(GLcontext * ctx)
{
    ctx->Driver.Flush = viaFlush;
    ctx->Driver.Clear = viaClear;
    ctx->Driver.Finish = viaFinish;
    ctx->Driver.ClearStencil = viaClearStencil;
}

GLboolean
viaCreateSafeClip(struct drm_via_clip_rect *r, int x, int y, int w, int h)
{
    if (w < 0 || h < 0)
	goto err;

    r->x1 = x;
    r->y1 = y;
    r->x2 = x + w;
    r->y2 = y + h;
    return GL_TRUE;

  err:
    r->x1 = 0;
    r->y1 = 0;
    r->x2 = 0;
    r->y2 = 0;
    return GL_FALSE;
}

static int
via_apply_texture_reloc(uint32_t ** cmdbuf,
			uint32_t num_buffers,
			struct via_validate_buffer *buffers,
			const struct drm_via_texture_reloc *reloc)
{
    const struct drm_via_reloc_bufaddr *baddr = reloc->addr;
    uint32_t baseh[4];
    uint32_t *buf = *cmdbuf + reloc->base.offset;
    uint32_t val;
    int i;
    int basereg;
    int shift;
    uint64_t flags = 0;
    uint32_t reg_tex_fm;

    memset(baseh, 0, sizeof(baseh));

    for (i = 0; i <= (reloc->hi_mip - reloc->low_mip); ++i) {
	if (baddr->index > num_buffers) {
	    *cmdbuf = buf;
	    return -EINVAL;
	}

	val = buffers[baddr->index].offset + baddr->delta;
	if (i == 0)
	    flags = buffers[baddr->index].flags;

	*buf++ = ((HC_SubA_HTXnL0BasL + i) << 24) | (val & 0x00FFFFFF);

	basereg = i / 3;
	shift = (3 - (i % 3)) << 3;

	baseh[basereg] |= (val & 0xFF000000) >> shift;
	baddr++;
    }

    if (reloc->low_mip < 3)
	*buf++ = baseh[0] | (HC_SubA_HTXnL012BasH << 24);
    if (reloc->low_mip < 6 && reloc->hi_mip > 2)
	*buf++ = baseh[1] | (HC_SubA_HTXnL345BasH << 24);
    if (reloc->low_mip < 9 && reloc->hi_mip > 5)
	*buf++ = baseh[2] | (HC_SubA_HTXnL678BasH << 24);
    if (reloc->hi_mip > 8)
	*buf++ = baseh[3] | (HC_SubA_HTXnL9abBasH << 24);

    reg_tex_fm = reloc->reg_tex_fm & ~HC_HTXnLoc_MASK;

    if (flags & WSBM_PL_FLAG_VRAM) {
	reg_tex_fm |= HC_HTXnLoc_Local;
    } else if (flags & WSBM_PL_FLAG_TT) {
	reg_tex_fm |= HC_HTXnLoc_AGP;
    } else
	abort();

    *buf++ = reg_tex_fm;
    *cmdbuf = buf;

    return 0;
}

static int
via_apply_zbuf_reloc(uint32_t ** cmdbuf,
		     uint32_t num_buffers,
		     struct via_validate_buffer *buffers,
		     const struct drm_via_zbuf_reloc *reloc)
{
    uint32_t *buf = *cmdbuf + reloc->base.offset;
    const struct drm_via_reloc_bufaddr *baddr = &reloc->addr;
    const struct via_validate_buffer *val_buf;
    uint32_t val;

    if (baddr->index > num_buffers)
	return -EINVAL;

    val_buf = &buffers[baddr->index];
    if (val_buf->po_correct)
	return 0;

    val = val_buf->offset + baddr->delta;
    *buf++ = (HC_SubA_HZWBBasL << 24) | (val & 0xFFFFFF);
    *buf++ = (HC_SubA_HZWBBasH << 24) | ((val & 0xFF000000) >> 24);

    *cmdbuf = buf;
    return 0;
}

static int
via_apply_dest_reloc(uint32_t ** cmdbuf,
		     uint32_t num_buffers,
		     struct via_validate_buffer *buffers,
		     const struct drm_via_zbuf_reloc *reloc)
{
    uint32_t *buf = *cmdbuf + reloc->base.offset;
    const struct drm_via_reloc_bufaddr *baddr = &reloc->addr;
    const struct via_validate_buffer *val_buf;
    uint32_t val;

    if (baddr->index > num_buffers)
	return -EINVAL;

    val_buf = &buffers[baddr->index];
    if (val_buf->po_correct)
	return 0;

    val = val_buf->offset + baddr->delta;
    *buf++ = (HC_SubA_HDBBasL << 24) | (val & 0xFFFFFF);
    *buf++ = (HC_SubA_HDBBasH << 24) | ((val & 0xFF000000) >> 24);

    *cmdbuf = buf;
    return 0;
}

static int
via_apply_2d_reloc(uint32_t ** cmdbuf,
		   uint32_t num_buffers,
		   const struct via_validate_buffer *buffers,
		   const struct drm_via_2d_reloc *reloc)
{
    uint32_t *buf = *cmdbuf + reloc->base.offset;
    const struct drm_via_reloc_bufaddr *baddr = &reloc->addr;
    const struct via_validate_buffer *val_buf;
    uint32_t val;
    uint32_t x;

    if (baddr->index > num_buffers)
	return -EINVAL;

    val_buf = &buffers[baddr->index];
    if (val_buf->po_correct)
	return 0;

    val = val_buf->offset + baddr->delta;
    x = val & 0x1f;

    if (reloc->bpp == 32)
	x >>= 2;
    else if (reloc->bpp == 16)
	x >>= 1;

    *buf = (val & ~0x1f) >> 3;
    buf += 2;
    *buf++ = reloc->pos + x;

    *cmdbuf = buf;
    return 0;
}

struct via_reloc_savestate *
via_reloc_save_state_alloc(void)
{
    return (struct via_reloc_savestate *)
	malloc(sizeof(struct via_reloc_savestate));
}

/*
 * Save reloc state for subsequent restoring.
 */

void
via_reloc_state_save(struct via_reloc_bufinfo *info,
		     struct via_reloc_savestate *save_state)
{
    save_state->info = info;
    save_state->first_header = info->first_header;
    save_state->cur_header = info->cur_header;
    save_state->old_header = *info->cur_header;
}

/*
 * Free a chain of reloc buffers.
 */

static void
via_free_reloc_header(struct drm_via_reloc_header *header)
{
    struct drm_via_reloc_header *new_header;

    while (header) {
	new_header = (struct drm_via_reloc_header *)(unsigned long)
	    header->next_header;
	free(header);
	header = new_header;
    }
}

/*
 * Restore the reloc chain to a previous state.
 */
void
via_reloc_state_restore(struct via_reloc_savestate *save_state)
{
    struct drm_via_reloc_header *added_chain;
    struct via_reloc_bufinfo *info = save_state->info;

    assert(info->first_header == save_state->first_header);
    assert(save_state->old_header.next_header == 0ULL);

    added_chain = (struct drm_via_reloc_header *)(unsigned long)
	info->cur_header->next_header;

    if (added_chain)
	via_free_reloc_header(added_chain);

    *save_state->cur_header = save_state->old_header;
}

/*
 * Add a reloc buffer to a chain.
 */

static int
via_add_reloc_buffer(struct via_reloc_bufinfo *info)
{
    struct drm_via_reloc_header *header;

    header = malloc(VIA_RELOC_BUF_SIZE);

    if (!header)
	return -ENOMEM;

    info->cur_header->next_header = (uint64_t) (unsigned long)header;
    info->cur_header = header;
    header->used = sizeof(*header);
    header->num_relocs = 0;
    header->next_header = 0ULL;

    return 0;
}

/*
 * Reset a reloc chain, freeing up all but the first buffer.
 */

static int
via_reset_reloc_buffer(struct via_reloc_bufinfo *info)
{
    struct drm_via_reloc_header *header;

    if (info->first_header == NULL) {
	header = info->first_header = malloc(VIA_RELOC_BUF_SIZE);
	if (!info->first_header)
	    return -ENOMEM;
    } else {
	header = info->first_header;
	via_free_reloc_header((struct drm_via_reloc_header *)(unsigned long)
			      header->next_header);
    }
    header->next_header = 0ULL;
    header->used = sizeof(*header);
    info->cur_header = header;

    header->num_relocs = 0;
    return 0;
}

struct via_reloc_bufinfo *
via_create_reloc_buffer(void)
{
    struct via_reloc_bufinfo *tmp;
    int ret;

    tmp = calloc(1, sizeof(struct via_reloc_bufinfo));
    if (!tmp)
	return NULL;

    ret = via_reset_reloc_buffer(tmp);
    if (ret) {
	free(tmp);
	return NULL;
    }

    return tmp;
}

void
via_free_reloc_buffer(struct via_reloc_bufinfo *info)
{
    (void)via_reset_reloc_buffer(info);
    if (info->first_header)
	free(info->first_header);
    free(info);
}

/*
 * Add a relocation to a chain.
 */

int
via_add_reloc(struct via_reloc_bufinfo *info, void *reloc, size_t size)
{
    struct drm_via_reloc_header *header = info->cur_header;

    if (header->used + size > VIA_RELOC_BUF_SIZE) {
	int ret;

	ret = via_add_reloc_buffer(info);
	if (ret)
	    return ret;
	header = info->cur_header;
    }
    memcpy((unsigned char *)header + header->used, reloc, size);
    header->used += size;
    header->num_relocs++;
    return 0;
}

int
via_reset_cmdlists(struct via_context *vmesa)
{
    int ret;

    ret = via_reset_reloc_buffer(vmesa->reloc_info);
    if (ret)
	return ret;

    ret = wsbmBOResetList(vmesa->validate_list);
    if (ret)
	return ret;

    return 0;
}

int
via_depth_relocation(struct via_context *vmesa,
		     uint32_t ** cmdbuf,
		     struct _WsbmBufferObject *depthBuffer,
		     uint64_t flags, uint64_t mask)
{
    struct drm_via_zbuf_reloc reloc;
    struct via_validate_buffer fake;
    int itemLoc;
    struct _ValidateNode *node;
    struct drm_via_validate_req *val_req;
    int ret;
    uint32_t tmp;

    ret = wsbmBOAddListItem(vmesa->validate_list, depthBuffer,
			    flags, mask, &itemLoc, &node);
    if (ret)
	return ret;

    val_req = viaValReq(node);

    if (!(val_req->presumed_flags & VIA_USE_PRESUMED)) {
	val_req->presumed_gpu_offset = (uint64_t)
	    wsbmBOOffsetHint(depthBuffer) - wsbmBOPoolOffset(depthBuffer);
	val_req->presumed_flags |= VIA_USE_PRESUMED;
    }

    fake.po_correct = 0;
    fake.offset = val_req->presumed_gpu_offset;

    reloc.base.type = VIA_RELOC_ZBUF;
    reloc.base.offset = 0;
    reloc.addr.index = 0;
    reloc.addr.delta = wsbmBOPoolOffset(depthBuffer);

    tmp = *cmdbuf - (uint32_t *) vmesa->dma;
    ret = via_apply_zbuf_reloc(cmdbuf, 1, &fake, &reloc);

    reloc.addr.index = itemLoc;
    reloc.base.offset = tmp;

    assert(ret == 0);

    return via_add_reloc(vmesa->reloc_info, &reloc, sizeof(reloc));
}

static int
via_dest_relocation(struct via_context *vmesa,
		    uint32_t ** cmdbuf,
		    struct _WsbmBufferObject *destBuffer,
		    uint32_t delta, uint64_t flags, uint64_t mask)
{
    struct drm_via_zbuf_reloc reloc;
    struct via_validate_buffer fake;
    int itemLoc;
    struct _ValidateNode *node;
    struct drm_via_validate_req *val_req;
    int ret;
    uint32_t tmp;

    ret = wsbmBOAddListItem(vmesa->validate_list, destBuffer,
			    flags, mask, &itemLoc, &node);
    if (ret)
	return ret;

    val_req = viaValReq(node);

    if (!(val_req->presumed_flags & VIA_USE_PRESUMED)) {
	val_req->presumed_gpu_offset = (uint64_t)
	    wsbmBOOffsetHint(destBuffer) - wsbmBOPoolOffset(destBuffer);
	val_req->presumed_flags |= VIA_USE_PRESUMED;
    }

    fake.po_correct = 0;
    fake.offset = val_req->presumed_gpu_offset;

    reloc.base.type = VIA_RELOC_DSTBUF;
    reloc.base.offset = 0;
    reloc.addr.index = 0;
    reloc.addr.delta = delta + wsbmBOPoolOffset(destBuffer);

    tmp = *cmdbuf - (uint32_t *) vmesa->dma;
    ret = via_apply_dest_reloc(cmdbuf, 1, &fake, &reloc);

    reloc.addr.index = itemLoc;
    reloc.base.offset = tmp;

    assert(ret == 0);

    return via_add_reloc(vmesa->reloc_info, &reloc, sizeof(reloc));
}

int
via_2d_relocation(struct via_context *vmesa,
		  uint32_t ** cmdbuf,
		  struct _WsbmBufferObject *buffer,
		  uint32_t delta, uint32_t bpp, uint32_t pos,
		  uint64_t flags, uint64_t mask)
{
    struct drm_via_2d_reloc reloc;
    struct via_validate_buffer fake;
    int itemLoc;
    struct _ValidateNode *node;
    struct drm_via_validate_req *val_req;
    int ret;
    uint32_t tmp;

    ret = wsbmBOAddListItem(vmesa->validate_list, buffer,
			    flags, mask, &itemLoc, &node);
    if (ret)
	return ret;

    val_req = viaValReq(node);

    if (!(val_req->presumed_flags & VIA_USE_PRESUMED)) {
	val_req->presumed_gpu_offset = (uint64_t)
	    (wsbmBOOffsetHint(buffer) - wsbmBOPoolOffset(buffer));
	val_req->presumed_flags |= VIA_USE_PRESUMED;
    }

    fake.po_correct = 0;
    fake.offset = val_req->presumed_gpu_offset;

    reloc.base.type = VIA_RELOC_2D;
    reloc.base.offset = 1;
    reloc.addr.index = 0;
    reloc.addr.delta = delta + wsbmBOPoolOffset(buffer);
    reloc.bpp = bpp;
    reloc.pos = pos;

    tmp = *cmdbuf - (uint32_t *) vmesa->dma;

    ret = via_apply_2d_reloc(cmdbuf, 1, &fake, &reloc);

    reloc.addr.index = itemLoc;
    reloc.base.offset = tmp + 1;

    assert(ret == 0);

    return via_add_reloc(vmesa->reloc_info, &reloc, sizeof(reloc));
}

/*
 * This is nasty. We might want to have a single
 * buffer object for the miptree.
 */

int
via_tex_relocation(struct via_context *vmesa,
		   uint32_t ** cmdbuf,
		   const struct via_reloc_texlist *addr,
		   uint32_t low_mip,
		   uint32_t hi_mip,
		   uint32_t reg_tex_fm, uint64_t flags, uint64_t mask)
{
    struct drm_via_texture_reloc reloc;
    struct via_validate_buffer fake[12];
    struct drm_via_reloc_bufaddr fake_addr[12];
    struct drm_via_reloc_bufaddr real_addr[12];
    int itemLoc;
    struct _ValidateNode *node;
    struct drm_via_validate_req *val_req;
    int ret;
    uint32_t tmp;
    int i;
    int count = 0;
    size_t size;

    for (i = 0; i <= (hi_mip - low_mip); ++i) {
	ret = wsbmBOAddListItem(vmesa->validate_list, addr[i].buf,
				flags, mask, &itemLoc, &node);
	if (ret)
	    return ret;

	val_req = viaValReq(node);

	if (!(val_req->presumed_flags & VIA_USE_PRESUMED)) {
	    val_req->presumed_flags = VIA_USE_PRESUMED;
	    val_req->presumed_gpu_offset = (uint64_t)
		(wsbmBOOffsetHint(addr[i].buf) -
		 wsbmBOPoolOffset(addr[i].buf));
	    if (wsbmBOPlacementHint(addr[i].buf) &
		(WSBM_PL_FLAG_TT | VIA_PL_FLAG_AGP))
		val_req->presumed_flags |= VIA_PRESUMED_AGP;
	}

	fake[count].po_correct = 0;
	fake[count].offset = val_req->presumed_gpu_offset;
	fake[count].flags = (val_req->presumed_flags & VIA_PRESUMED_AGP) ?
	    WSBM_PL_FLAG_TT : WSBM_PL_FLAG_VRAM;
	real_addr[count].index = itemLoc;
	real_addr[count].delta =
	    addr[i].delta + wsbmBOPoolOffset(addr[i].buf);
	fake_addr[count].index = count;
	fake_addr[count].delta = real_addr[count].delta;
	count++;
    }

    reloc.base.type = VIA_RELOC_TEX;
    reloc.base.offset = 0;
    reloc.low_mip = low_mip;
    reloc.hi_mip = hi_mip;
    reloc.reg_tex_fm = reg_tex_fm;
    memcpy(reloc.addr, fake_addr,
	   count * sizeof(struct drm_via_reloc_bufaddr));
    tmp = *cmdbuf - (uint32_t *) vmesa->dma;

    ret = via_apply_texture_reloc(cmdbuf, count, fake, &reloc);

    memcpy(reloc.addr, real_addr,
	   count * sizeof(struct drm_via_reloc_bufaddr));
    reloc.base.offset = tmp;

    size = offsetof(struct drm_via_texture_reloc, addr) -
	offsetof(struct drm_via_texture_reloc, base) +
	count * sizeof(struct drm_via_reloc_bufaddr);

    assert(ret == 0);

    return via_add_reloc(vmesa->reloc_info, &reloc, size);
}

void
via_drop_cmdbuf(struct via_context *vmesa)
{
    vmesa->lostStateDmaLow = 0;
    vmesa->dmaLow = 0;
    vmesa->dmaCliprectAddr = ~0;
    vmesa->lostState = 1;

    wsbmBOUnrefUserList(vmesa->validate_list);
    (void)via_reset_reloc_buffer(vmesa->reloc_info);
}

static void
via_abort_cmdbuf(struct via_context *vmesa)
{
    UNLOCK_HARDWARE(vmesa);
#if 0
    fprintf(stderr, "Unexpected command submission failure.\n"
	    "  This is probably an out-of-memory condition.\n"
	    "  Trying to recover.\n\n");
    fflush(stderr);
#endif
    LOCK_HARDWARE(vmesa);
    via_drop_cmdbuf(vmesa);
}

static int
via_setup_clip(struct via_context *vmesa,
	       uint32_t * exec_flags,
	       struct drm_via_clip_rect **clip,
	       unsigned int *num_clip, GLboolean * free_clips)
{
    uint32_t *vb;
    GLuint pitch;
    GLuint delta;
    GLuint format;
    struct _WsbmBufferObject *buf;
    int ret;

    *free_clips = GL_FALSE;
    if (vmesa->dmaCliprectAddr == ~0)
	return -EINVAL;

    *exec_flags |= DRM_VIA_HAVE_CLIP;

    vb = (uint32_t *) (vmesa->dma + vmesa->dmaCliprectAddr);

    if (vmesa->meta_flags & VIA_META_DST_ENABLE) {
	struct via_meta_state *meta = &vmesa->meta;

	format = meta->dst_format;
	pitch = meta->dst_pitch;
	buf = meta->dst_buf;
	*clip = &meta->dst_clip;
	*num_clip = 1;
	delta = 0;
    } else {
	struct via_renderbuffer *buffer = viaDrawRenderBuffer(vmesa);
	struct via_framebuffer *vfb = viaDrawFrameBuffer(vmesa);
	struct drm_via_clip_rect *vclip;
	struct drm_via_clip_rect *pvclip;
	struct drm_via_clip_rect *dclip;
	struct drm_via_clip_rect scissor;
	unsigned int nclip;
	unsigned int i;
	int xoff;

	format = buffer->hwformat;
	pitch = buffer->pitch;
	buf = buffer->buf;
	delta = buffer->origAdd;

	if (buffer->isSharedFrontBuffer) {
	    nclip = vfb->numFrontClipRects;
	    dclip = vfb->pFrontClipRects;
	    xoff = vfb->xoff;
	} else {
	    nclip = 1;
	    dclip = &vfb->allClipRect;
	    xoff = 0;
	}

	if (nclip <= VIA_CLIP_CACHE_SIZE) {
	    vclip = vmesa->clip_cache;
	} else {
	    vclip = malloc(nclip * sizeof(struct drm_via_clip_rect));
	    if (vclip == NULL)
		return -ENOMEM;
	    *free_clips = GL_TRUE;
	}

	pvclip = vclip;
	if (vmesa->scissor) {
	    scissor = vmesa->scissorRect;
	    if (buffer->Base.Name == 0) {
		int tmp = scissor.y1;

		scissor.y1 = buffer->Base.Height - scissor.y2;
		scissor.y2 = buffer->Base.Height - tmp;
	    }
	}

	for (i = 0; i < nclip; ++i, ++pvclip, ++dclip) {
	    if (vmesa->scissor) {
		if (!via_intersect_via_rect(pvclip, dclip, &scissor))
		    continue;
	    } else {
		*pvclip = *dclip;
	    }

	    pvclip->x1 += xoff;
	    pvclip->x2 += xoff;

	}
	nclip = pvclip - vclip;
	*num_clip = nclip;
	if (nclip == 0) {
	    if (*free_clips) {
		*free_clips = GL_FALSE;
		free(vclip);
	    }
	    return -EINVAL;
	}
	*clip = vclip;
    }

    *vb++ = HC_HEADER2;
    *vb++ = (HC_ParaType_NotTex << 16);
    ret = via_dest_relocation(vmesa, &vb, buf, delta,
			      WSBM_PL_FLAG_VRAM, WSBM_PL_MASK_MEM);

    vb += 2;			       /* Cliprect lands here, inserted by DRM. */

    *vb++ = (HC_SubA_HSPXYOS << 24);
    *vb++ = (HC_SubA_HDBFM << 24) | HC_HDBLoc_Local | format | pitch;

    return 0;
}

/*
 * The driver can not rewind if this function fails. Therefore, drop the current
 * dma buffer and continue as if nothing happened. The only errors we should really see
 * here are OOMs. Either AGP / VRAM space OOMs or malloc OOMS.
 */

void
via_execbuf(struct via_context *vmesa, GLuint fire_flags)
{
    struct drm_via_ttm_execbuf_arg arg;
    struct drm_via_ttm_execbuf_control control;
    struct _ValidateNode *node;
    struct _ViaDrmValidateNode *viaNode;
    struct _ValidateList *valList;
    struct drm_via_validate_arg *val_arg;
    int ret;
    struct drm_via_clip_rect *clip = NULL;
    struct drm_via_validate_req *req;
    struct drm_via_validate_rep *rep;
    struct drm_via_ttm_fence_rep *fence_rep = &control.rep;
    unsigned int num_clip = 0;
    uint32_t exec_flags = 0x00000000;
    void *iterator;
    uint64_t first = 0;
    uint64_t *prevNext = NULL;
    int count = 0;
    GLboolean free_clips = GL_FALSE;
    struct _WsbmFenceObject *fence;
    struct _WsbmFenceMgr *fence_mgr = vmesa->viaScreen->fence_mgr;

    if (vmesa->dmaLow == 0) {
	via_abort_cmdbuf(vmesa);
	return;
    }

    if (!(fire_flags & VIA_NO_CLIPRECTS)) {
#if 0
	via_abort_cmdbuf(vmesa);
	return;
#endif
	ret = via_setup_clip(vmesa, &exec_flags, &clip, &num_clip,
			     &free_clips);
	if (ret) {
	    if (free_clips)
		free(clip);
	    via_abort_cmdbuf(vmesa);
	    return;
	}
    }

    /*
     * Validate our buffers.
     */

#if 0
    ret = wsbmBOValidateUserList(vmesa->validate_list);
    if (ret) {
	via_abort_cmdbuf(vmesa);
	return;
    }
#endif

    valList = wsbmGetKernelValidateList(vmesa->validate_list);
    iterator = validateListIterator(valList);
    while (iterator) {
	node = validateListNode(iterator);
	viaNode = containerOf(node, struct _ViaDrmValidateNode, base);

	val_arg = &viaNode->val_arg;
	val_arg->handled = 0;
	val_arg->ret = 0;
	req = &val_arg->d.req;

	if (!first)
	    first = (uint64_t) (unsigned long)val_arg;
	if (prevNext)
	    *prevNext = (uint64_t) (unsigned long)val_arg;
	prevNext = &req->next;

	req->buffer_handle = wsbmKBufHandle((struct _WsbmKernelBuf *)
					    node->buf);
	req->set_flags = node->set_flags;
	req->clear_flags = node->clr_flags;

	iterator = validateListNext(valList, iterator);
	++count;
    }

    /*
     * Fill in the execbuf arg itself.
     */

    if (count == 0)
	abort();

    memset(&control, 0, sizeof(control));

    arg.buffer_list = first;
    arg.num_buffers = count;
    arg.reloc_list = (uint64_t) (unsigned long)
	vmesa->reloc_info->first_header;
    arg.cmd_buffer = (uint64_t) (unsigned long)
	vmesa->dma;
    arg.control = (uint64_t) (unsigned long)&control;
    arg.cmd_buffer_size = vmesa->dmaLow;
    arg.mechanism = _VIA_MECHANISM_AGP;
    arg.exec_flags = exec_flags;
    arg.cliprect_offset = (vmesa->dmaCliprectAddr >> 2) + 4;
    arg.num_cliprects = num_clip;
    arg.cliprect_addr = (unsigned long)clip;
    arg.context = vmesa->hHWContext;

    wsbmFenceCmdLock(fence_mgr, VIA_ENGINE_CMD);

    do {
	ret = drmCommandWrite(vmesa->driFd, vmesa->execIoctlOffset,
			      &arg, sizeof(arg));
    } while (ret == -EAGAIN || ret == -EINTR || ret == -ERESTART);

    if (ret == -EFAULT)
	abort();

    if (free_clips)
	free(clip);

    iterator = validateListIterator(valList);

    /*
     * Update all user-space cached offsets and flags for kernel
     * buffers involved in this commands.
     */

    while (iterator) {
	node = validateListNode(iterator);
	viaNode = containerOf(node, struct _ViaDrmValidateNode, base);

	val_arg = &viaNode->val_arg;

	if (!val_arg->handled)
	    break;

	if (val_arg->ret != 0) {
	    _mesa_printf("Failed a buffer validation: \"%s\".\n",
			 strerror(-val_arg->ret));
	    iterator = validateListNext(valList, iterator);
	    continue;
	}

	rep = &val_arg->d.rep;
	wsbmUpdateKBuf((struct _WsbmKernelBuf *)node->buf,
		       rep->gpu_offset, rep->placement, rep->fence_type_mask);

	iterator = validateListNext(valList, iterator);
    }

    if (ret) {
	UNLOCK_HARDWARE(vmesa);
	fprintf(stderr, "Flush dma returned %d: %s\n", ret, strerror(-ret));
	dump_dma(vmesa);
	LOCK_HARDWARE(vmesa);

	/*
	 * Fall through and go on fencing with a NULL fence.
	 * The engine has been idled at this point.
	 */
    } else {
	if (fence_rep->handle == ~0) {

	    /*
	     * fence creation failed and engines have been idled.
	     */

	    fence = NULL;
	} else {
	    fence = wsbmFenceCreate(fence_mgr,
				    fence_rep->fence_class,
				    fence_rep->fence_type,
				    (void *)(unsigned long)fence_rep->handle,
				    0);
	}

	if (vmesa->last_fence != NULL)
	    wsbmFenceUnreference(&vmesa->last_fence);

	vmesa->last_fence = fence;
    }

    wsbmFenceCmdUnlock(fence_mgr, VIA_ENGINE_CMD);
    via_drop_cmdbuf(vmesa);
    return;
}
