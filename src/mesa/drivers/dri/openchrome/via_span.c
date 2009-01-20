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

#include "main/glheader.h"
#include "main/macros.h"
#include "main/mtypes.h"
#include "main/colormac.h"

#include "via_context.h"
#include "via_span.h"
#include "via_ioctl.h"
#include "swrast/swrast.h"
#include "via_fbo.h"
#include "wsbm_manager.h"
#include "via_tex.h"
#include "via_3d_reg.h"

#define DBG 0

#define Y_FLIP(_y) ((rb->Name == 0) ? height - (_y) - 1 : (_y))

#define HW_LOCK()

#define HW_UNLOCK()

#undef HW_CLIPLOOP
#define HW_CLIPLOOP()						\
    do {								\
	int _nc = vmesa->numDrawClipRects;				\
	while (!vmesa->spanError &&  _nc-- ) {				\
	    int minx = (short) vmesa->pDrawClipRects[_nc].x1;		\
	    int miny = (short) vmesa->pDrawClipRects[_nc].y1;		\
	    int maxx = (short) vmesa->pDrawClipRects[_nc].x2;		\
	    int maxy = (short) vmesa->pDrawClipRects[_nc].y2;

#undef HW_READ_CLIPLOOP
#define HW_READ_CLIPLOOP()						\
  do {									\
        int _nc = vmesa->numReadClipRects;					\
	while ( !vmesa->spanError &&_nc-- ) {				\
	    int minx = (short) vmesa->pReadClipRects[_nc].x1;		\
	    int miny = (short) vmesa->pReadClipRects[_nc].y1;		\
	    int maxx = (short) vmesa->pReadClipRects[_nc].x2;		\
	    int maxy = (short) vmesa->pReadClipRects[_nc].y2;

#undef LOCAL_VARS
#define LOCAL_VARS							\
    struct via_renderbuffer *vrb = via_renderbuffer(rb);		\
    struct via_context *vmesa = VIA_CONTEXT(ctx);			\
    GLuint pitch = vrb->pitch;						\
    GLuint height = rb->Height;						\
    GLint p = 0;							\
    char *buf = (char *)vrb->map + vrb->origMapAdd;			\
    (void) p;

/* ================================================================
 * Color buffer
 */

/* 16 bit, RGB565 / BGR color spanline and pixel functions
 */
#define SPANTMP_PIXEL_FMT GL_RGB
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_SHORT_5_6_5

#define TAG(x)    via##x##_rgb565
#define TAG2(x,y) via##x##_rgb565##y
#include "spantmp2.h"

/* 32 bit, ARGB8888 color spanline and pixel functions
 */
#define SPANTMP_PIXEL_FMT GL_BGRA
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV

#define TAG(x)    via##x##_argb8888
#define TAG2(x,y) via##x##_argb8888##y
#include "spantmp2.h"

/* 16 bit depthbuffer functions.
 */

#define LOCAL_DEPTH_VARS						\
    struct via_renderbuffer *vrb = via_renderbuffer(rb);		\
    struct via_context *vmesa = VIA_CONTEXT(ctx);			\
    GLuint depth_pitch = vrb->pitch;					\
    GLuint height = rb->Height;						\
    char *buf = (char *)vrb->map + vrb->origMapAdd;			\
    assert(buf != 0);

#define VALUE_TYPE GLushort

#define WRITE_DEPTH(_x, _y, d)                      \
    *(GLushort *)(buf + (_x) * 2 + (_y) * depth_pitch) = d;

#define READ_DEPTH(d, _x, _y)                       \
    d = *(volatile GLushort *)(buf + (_x) * 2 + (_y) * depth_pitch);

#define TAG(x) via##x##_z16
#include "depthtmp.h"

/* 32 bit depthbuffer functions.
 */

#define VALUE_TYPE GLuint

#define WRITE_DEPTH(_x, _y, d)                      \
    *(GLuint *)(buf + (_x) * 4 + (_y) * depth_pitch) = d;

#define READ_DEPTH(d, _x, _y)                       \
    d = *(volatile GLuint *)(buf + (_x) * 4 + (_y) * depth_pitch);

#define TAG(x) via##x##_z32
#include "depthtmp.h"

/*
 * 24/8-bit interleaved depth/stencil functions
 * Note: we're actually reading back combined depth+stencil values.
 * The wrappers in main/depthstencil.c are used to extract the depth
 * and stencil values.
 */

#define VALUE_TYPE GLuint

#define WRITE_DEPTH( _x, _y, d ) {				\
	GLuint tmp = d;						\
	*(GLuint *)(buf + (_x)*4 + (_y)*depth_pitch) = tmp;	\
}

#define READ_DEPTH( d, _x, _y )				\
    d = (*(volatile GLuint *)(buf + (_x)*4 + (_y)*depth_pitch));

#define TAG(x) via##x##_z24_s8
#include "depthtmp.h"

/* Move locking out to get reasonable span performance.
 */
void
viaSpanRenderStart(GLcontext * ctx)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    uint32_t drawMapFlags = WSBM_SYNCCPU_WRITE;
    struct via_renderbuffer *draw_depth;
    struct via_renderbuffer *read_depth;
    struct via_renderbuffer *draw;
    struct via_renderbuffer *read;
    int ret;

    /*
     * Flush DMA and wait idle before locking.
     * The wsbmBOMap() operations will wait for idle anyway.
     */

    via_wait_context_idle(vmesa);

    if (vmesa->firstDrawAfterSwap) {
	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);
	UNLOCK_HARDWARE(vmesa);
	vmesa->firstDrawAfterSwap = GL_FALSE;
    }

    /*
     * We're accessing buffer maps, so prevent eviction
     * during swrast.
     */

    draw = viaDrawRenderBuffer(vmesa);
    read = viaReadRenderBuffer(vmesa);

    if ((draw != NULL && draw->isSharedFrontBuffer) ||
	(read != NULL && read->isSharedFrontBuffer)) {
	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);
    }

    if (draw != NULL) {
	if (draw == read)
	    drawMapFlags |= WSBM_SYNCCPU_READ;
	draw->map = wsbmBOMap(draw->buf, drawMapFlags);
	if (!draw->map)
	    goto out_err0;

	vmesa->drawSyncFlags = drawMapFlags;
	ret = wsbmBOSyncForCpu(draw->buf, vmesa->drawSyncFlags);
	if (ret)
	    goto out_err1;

	draw->viaImage.image.Data = draw->map;
    }

    if (read != NULL && draw != read) {
	read->map = wsbmBOMap(read->buf, WSBM_ACCESS_READ);
	if (!read->map)
	    goto out_err2;
	ret = wsbmBOSyncForCpu(read->buf, WSBM_SYNCCPU_READ);
	if (ret)
	    goto out_err3;
    }

    draw_depth = via_get_renderbuffer(&viaDrawFrameBuffer(vmesa)->Base,
				      BUFFER_DEPTH);
    read_depth = via_get_renderbuffer(&viaReadFrameBuffer(vmesa)->Base,
				      BUFFER_DEPTH);

    if (draw_depth != NULL) {
	drawMapFlags = WSBM_SYNCCPU_WRITE;
	if (draw_depth == read_depth)
	    drawMapFlags |= WSBM_SYNCCPU_READ;

	draw_depth->map = wsbmBOMap(draw_depth->buf, drawMapFlags);
	if (!draw_depth->map)
	    goto out_err4;
	vmesa->depthSyncFlags = drawMapFlags;
	ret = wsbmBOSyncForCpu(draw_depth->buf, vmesa->depthSyncFlags);
	if (ret)
	    goto out_err5;
    }

    if (read_depth != draw_depth && read_depth != NULL) {
	read_depth->map = wsbmBOMap(read_depth->buf, WSBM_SYNCCPU_READ);
	if (!read_depth->map)
	    goto out_err6;
	ret = wsbmBOSyncForCpu(draw_depth->buf, WSBM_SYNCCPU_READ);
	if (ret)
	    goto out_err7;
    }

    ret = via_map_unmap_texunit(&ctx->Texture.Unit[0], GL_TRUE);
    if (ret)
	goto out_err8;

    ret = via_map_unmap_texunit(&ctx->Texture.Unit[1], GL_TRUE);
    if (ret)
	goto out_err9;

    vmesa->spanError = GL_FALSE;

    return;

  out_err9:
    (void)via_map_unmap_texunit(&ctx->Texture.Unit[0], GL_FALSE);
  out_err8:
    if (read_depth != draw_depth && read_depth != NULL)
	(void)wsbmBOReleaseFromCpu(read_depth->buf, WSBM_SYNCCPU_READ);
  out_err7:
    if (read_depth != draw_depth && read_depth != NULL) {
	(void)wsbmBOUnmap(read_depth->buf);
	read_depth->map = NULL;
    }
  out_err6:
    if (draw_depth != NULL)
	(void)wsbmBOReleaseFromCpu(draw_depth->buf, vmesa->depthSyncFlags);
  out_err5:
    if (draw_depth != NULL) {
	(void)wsbmBOUnmap(draw_depth->buf);
	draw_depth->map = NULL;
    }
  out_err4:
    if (read != NULL && draw != read)
	(void)wsbmBOReleaseFromCpu(read->buf, WSBM_SYNCCPU_READ);
  out_err3:
    if (read != NULL && draw != read) {
	(void)wsbmBOUnmap(read->buf);
	read->map = NULL;
    }
  out_err2:
    if (draw != NULL)
	(void)wsbmBOReleaseFromCpu(draw->buf, vmesa->drawSyncFlags);
  out_err1:
    if (draw != NULL) {
	(void)wsbmBOUnmap(draw->buf);
	draw->map = NULL;
    }

  out_err0:
    if ((draw != NULL && draw->isSharedFrontBuffer) ||
	(read != NULL && read->isSharedFrontBuffer)) {
	UNLOCK_HARDWARE(vmesa);
	fflush(stderr);
    }

    vmesa->spanError = GL_TRUE;
}

void
viaSpanRenderFinish(GLcontext * ctx)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_renderbuffer *draw_depth;
    struct via_renderbuffer *read_depth;
    struct via_renderbuffer *draw;
    struct via_renderbuffer *read;

    _swrast_flush(ctx);

    if (vmesa->spanError)
	return;

    draw = viaDrawRenderBuffer(vmesa);
    read = viaReadRenderBuffer(vmesa);

    if ((draw != NULL && draw->isSharedFrontBuffer) ||
	(read != NULL && read->isSharedFrontBuffer)) {
	UNLOCK_HARDWARE(vmesa);
	fflush(stderr);
    }

    draw_depth = via_get_renderbuffer(&viaDrawFrameBuffer(vmesa)->Base,
				      BUFFER_DEPTH);
    read_depth = via_get_renderbuffer(&viaReadFrameBuffer(vmesa)->Base,
				      BUFFER_DEPTH);

    if (draw_depth != NULL) {
	(void)wsbmBOReleaseFromCpu(draw_depth->buf, vmesa->depthSyncFlags);
	(void)wsbmBOUnmap(draw_depth->buf);
	draw_depth->map = NULL;
    }
    if (read_depth != draw_depth && read_depth != NULL) {
	(void)wsbmBOReleaseFromCpu(draw_depth->buf, WSBM_SYNCCPU_READ);
	(void)wsbmBOUnmap(read_depth->buf);
	read_depth->map = NULL;
    }

    if (draw != NULL) {
	(void)wsbmBOReleaseFromCpu(draw->buf, vmesa->drawSyncFlags);
	(void)wsbmBOUnmap(draw->buf);
	draw->map = NULL;
    }
    if (draw != read) {
	(void)wsbmBOReleaseFromCpu(read->buf, WSBM_SYNCCPU_READ);
	(void)wsbmBOUnmap(read->buf);
	read->map = NULL;
    }

    via_map_unmap_texunit(&ctx->Texture.Unit[0], GL_FALSE);
    via_map_unmap_texunit(&ctx->Texture.Unit[1], GL_FALSE);

}

void
viaInitSpanFuncs(GLcontext * ctx)
{
    struct swrast_device_driver *swdd = _swrast_GetDeviceDriverReference(ctx);

    swdd->SpanRenderStart = viaSpanRenderStart;
    swdd->SpanRenderFinish = viaSpanRenderFinish;
}

/**
 * Plug in the Get/Put routines for the given driRenderbuffer.
 */
void
via_set_span_functions(struct gl_renderbuffer *rb)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);

    switch (rb->_ActualFormat) {
    case GL_RGB5:
	viaInitPointers_rgb565(rb);
	if (viarb->hwformat != HC_HDBFM_RGB565)
	    _mesa_problem(NULL, "Unexpected hardware format in "
			  "via_set_span_functions.");
	break;
    case GL_RGBA8:
	viaInitPointers_argb8888(rb);
	if (viarb->hwformat != HC_HDBFM_ARGB8888 &&
	    viarb->hwformat != HC_HDBFM_ARGB0888)
	    _mesa_problem(NULL, "Unexpected hardware format in "
			  "via_set_span_functions.");
	break;
    case GL_DEPTH_COMPONENT16:
	viaInitDepthPointers_z16(rb);
	break;
    case GL_DEPTH24_STENCIL8_EXT:
	viaInitDepthPointers_z24_s8(rb);
	break;
    case GL_DEPTH_COMPONENT32:
	viaInitDepthPointers_z32(rb);
	break;
    default:
	_mesa_problem(NULL, "Unexpected _ActualFormat in "
		      "via_set_span_functions.");
    }
}
