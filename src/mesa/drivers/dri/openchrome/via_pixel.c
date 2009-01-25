/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Tx. , USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include "main/image.h"
#include "main/state.h"
#include "swrast/swrast.h"

#include "via_pixel.h"
#include "via_context.h"
#include "via_ioctl.h"
#include "via_buffer_objects.h"

#include "wsbm_manager.h"

static GLboolean
via_check_copypixel_blit_fragment_ops(GLcontext * ctx)
{
    if (ctx->NewState)
	_mesa_update_state(ctx);

    /* Could do logicop with the blitter:
     */

    return !(ctx->_ImageTransferState ||
	     ctx->Color.AlphaEnabled ||
	     ctx->Depth.Test ||
	     ctx->Fog.Enabled ||
	     ctx->Stencil.Enabled ||
	     !ctx->Color.ColorMask[0] ||
	     !ctx->Color.ColorMask[1] ||
	     !ctx->Color.ColorMask[2] ||
	     !ctx->Color.ColorMask[3] ||
	     ctx->Texture._EnabledUnits || ctx->Color.BlendEnabled);
}

static GLboolean
via_check_overlap(struct via_renderbuffer *src,
		  struct via_renderbuffer *dst,
		  struct drm_via_clip_rect *src_rect,
		  struct drm_via_clip_rect *dst_rect, int *xdir, int *ydir)
{
    struct drm_via_clip_rect dummy;

    *xdir = 0;
    *ydir = 0;

    if ((src->buf != dst->buf) &&
	(!src->isSharedFrontBuffer || !dst->isSharedFrontBuffer))
	return GL_FALSE;

    if (!via_intersect_via_rect(&dummy, dst_rect, src_rect))
	return GL_FALSE;

    *xdir = (dst_rect->x1 > src_rect->x1);
    *ydir = (dst_rect->y1 > src_rect->y1);

    return GL_TRUE;
}

static GLboolean
via_blit_copypixels(GLcontext * ctx,
		    GLint srcx, GLint srcy,
		    GLsizei width, GLsizei height,
		    GLint dstx, GLint dsty, GLenum type)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_renderbuffer *src_rb = viaReadRenderBuffer(vmesa);
    struct via_renderbuffer *dst_rb = viaDrawRenderBuffer(vmesa);
    struct drm_via_clip_rect scissor_rect;
    struct drm_via_clip_rect dst_rect;
    struct drm_via_clip_rect src_rect;
    struct drm_via_clip_rect tmp_dst_rect;
    struct drm_via_clip_rect tmp_src_rect;
    struct drm_via_clip_rect *src_clip_rects;
    struct drm_via_clip_rect *dst_clip_rects;
    int num_src_clip;
    int num_dst_clip;
    int i;
    int j;
    unsigned int byte_per_pixel;
    short dx;
    short dy;

    /*
     * We can easily support depth_component and stencil by selecting
     * the appropriate renderbuffer and setting the right blit mask,
     * but we'll leave that for now.
     */

    if (type != GL_COLOR)
	return GL_FALSE;

    /*
     * Check fragment ops and zoom. Basically the 2D blitter cannot
     * handle any of these.
     */

    if (!via_check_copypixel_blit_fragment_ops(ctx) ||
	ctx->Pixel.ZoomX != 1.0F || ctx->Pixel.ZoomY != 1.0F)
	return GL_FALSE;

    /*
     * Can't handle incomplete framebuffers or renderbuffers with
     * different hardware orientation, since the 2D blitter cannot
     * flip y direction.
     */

    if (src_rb == NULL ||
	dst_rb == NULL ||
	(src_rb->Base.Name == 0 && dst_rb->Base.Name != 0) ||
	(src_rb->Base.Name != 0 && dst_rb->Base.Name == 0))
	return GL_FALSE;

    /*
     * Can't convert formats.
     */

    if (dst_rb->bpp != src_rb->bpp)
	return GL_FALSE;

    byte_per_pixel = dst_rb->bpp >> 3;

    /*
     * Destination clip rect in GL coordinates.
     */

    dst_rect.x1 = dstx;
    dst_rect.x2 = dstx + width;
    dst_rect.y1 = dsty;
    dst_rect.y2 = dsty + height;

    if (ctx->Scissor.Enabled) {
	scissor_rect.x1 = ctx->Scissor.X;
	scissor_rect.x2 = ctx->Scissor.X + ctx->Scissor.Width;
	scissor_rect.y1 = ctx->Scissor.Y;
	scissor_rect.y2 = ctx->Scissor.Y + ctx->Scissor.Height;
	if (!via_intersect_via_rect(&dst_rect, &dst_rect, &scissor_rect)) {
	    return GL_TRUE;
	}
    }

    src_rect.x1 = srcx;
    src_rect.x2 = srcx + width;
    src_rect.y1 = srcy;
    src_rect.y2 = srcy + height;

    if (vmesa->firstDrawAfterSwap || dst_rb->isSharedFrontBuffer ||
	src_rb->isSharedFrontBuffer) {
	LOCK_HARDWARE(vmesa);
	VIA_FLUSH_DMA(vmesa);
	viaValidateDrawablesLocked(vmesa);
    } else {
	VIA_FLUSH_DMA(vmesa);
    }

    if (dst_rb->Base.Name == 0) {
	short tmp = src_rect.y1;
	short h = src_rb->Base.Height;

	/*
	 * Need to flip clip rects for hardware orientation.
	 */

	src_rect.y1 = h - src_rect.y2;
	src_rect.y2 = h - tmp;

	tmp = dst_rect.y1;
	h = dst_rb->Base.Height;

	dst_rect.y1 = h - dst_rect.y2;
	dst_rect.y2 = h - tmp;

	dy = (h - dsty - height) - src_rect.y1;
    } else {
	dy = dsty - srcy;
    }

    dx = dst_rect.x1 - src_rect.x1;

    num_src_clip = vmesa->numReadClipRects;
    src_clip_rects = vmesa->pReadClipRects;

    num_dst_clip = vmesa->numDrawClipRects;
    dst_clip_rects = vmesa->pDrawClipRects;

    for (i = 0; i < num_src_clip; ++i) {
	if (!via_intersect_via_rect
	    (&tmp_src_rect, &src_rect, src_clip_rects + i))
	    continue;

	tmp_src_rect.x1 += dx;
	tmp_src_rect.x2 += dx;
	tmp_src_rect.y1 += dy;
	tmp_src_rect.y2 += dy;

	for (j = 0; j < num_dst_clip; ++j) {
	    struct drm_via_clip_rect final_dst_rect;
	    struct drm_via_clip_rect final_src_rect;
	    int xdir;
	    int ydir;

	    if (!via_intersect_via_rect
		(&tmp_dst_rect, &dst_rect, dst_clip_rects + j))
		continue;

	    if (!via_intersect_via_rect(&final_dst_rect, &tmp_src_rect,
					&tmp_dst_rect))
		continue;

	    final_src_rect = final_dst_rect;
	    final_src_rect.x1 -= dx;
	    final_src_rect.x2 -= dx;
	    final_src_rect.y1 -= dy;
	    final_src_rect.y2 -= dy;

	    (void)via_check_overlap(src_rb, dst_rb,
				    &final_src_rect,
				    &final_dst_rect, &xdir, &ydir);
	    viaBlit(vmesa,
		    dst_rb->bpp,
		    src_rb->buf,
		    dst_rb->buf,
		    src_rb->origAdd +
		    final_src_rect.y1 * src_rb->pitch +
		    (final_src_rect.x1 + vmesa->readXoff) * byte_per_pixel,
		    src_rb->pitch,
		    dst_rb->origAdd +
		    final_dst_rect.y1 * dst_rb->pitch +
		    (final_dst_rect.x1 + vmesa->drawXoff) * byte_per_pixel,
		    dst_rb->pitch,
		    xdir, ydir,
		    final_src_rect.x2 - final_src_rect.x1,
		    final_src_rect.y2 - final_src_rect.y1,
		    VIA_BLIT_COPY, 0x0, 0x0);
	}
    }

    if (!vmesa->isLocked) {
	LOCK_HARDWARE(vmesa);
    }
    via_execbuf(vmesa, VIA_NO_CLIPRECTS);
    UNLOCK_HARDWARE(vmesa);

    return GL_TRUE;
}

/*
 * Check whether we can support the current image transfer ops with the
 * 3D engine.
 */

GLboolean
via_compute_image_transfer(GLcontext * ctx,
			   uint32_t * scale_rgba, uint32_t * bias_rgba)
{
    float biasMax = 127.f / 255.f;
    float redBias;
    float greenBias;
    float blueBias;
    float alphaBias;
    struct gl_pixel_attrib *pixel = &ctx->Pixel;
    GLbitfield transferOps = ctx->_ImageTransferState;

    /*
     * We can only do scale and bias in hardware.
     */

    if (transferOps & ~(IMAGE_SCALE_BIAS_BIT))
	return GL_FALSE;

    if (transferOps == 0) {
	*scale_rgba = 0xFFFFFFFF;
	*bias_rgba = 0x00000000;
	return GL_TRUE;
    }

    /*
     * Check hardware limits. We can scale from 0 to 1 and
     * bias from -128 / 255 to 127 / 255, using the texenv
     * functions. Unichrome bias must be pre-multiplied with
     * the scale though.
     */

    if (pixel->RedScale > 1.f || pixel->RedScale < 0.f ||
	pixel->GreenScale > 1.f || pixel->GreenScale < 0.f ||
	pixel->BlueScale > 1.f || pixel->BlueScale < 0.f ||
	pixel->AlphaScale > 1.f || pixel->AlphaScale < 0.f)
	return GL_FALSE;

    redBias = pixel->RedScale * pixel->RedBias;
    greenBias = pixel->GreenScale * pixel->GreenBias;
    blueBias = pixel->BlueScale * pixel->BlueBias;
    alphaBias = pixel->AlphaScale * pixel->AlphaBias;

    if (fabs(redBias) > biasMax ||
	fabs(greenBias) > biasMax ||
	fabs(blueBias) > biasMax || fabs(alphaBias) > biasMax)
	return GL_FALSE;

    *scale_rgba =
	((((unsigned int)(pixel->RedScale * 255.f)) & 0xFF) << 24) |
	((((unsigned int)(pixel->GreenScale * 255.f)) & 0xFF) << 16) |
	((((unsigned int)(pixel->BlueScale * 255.f)) & 0xFF) << 8) |
	(((unsigned int)(pixel->AlphaScale * 255.f)) & 0xFF);

    *bias_rgba =
	((((int)(redBias * 255.f)) & 0xFF) << 24) |
	((((int)(greenBias * 255.f)) & 0xFF) << 16) |
	((((int)(blueBias * 255.f)) & 0xFF) << 8) |
	(((int)(alphaBias * 255.f)) & 0xFF);

    return GL_TRUE;
}

/*
 * Check whether we can support the current state when doing
 * draw- and copypixels with the 3D engine.
 */

static GLboolean
via_check_fragment_ops(GLcontext * ctx)
{
    if (ctx->NewState)
	_mesa_update_state(ctx);

    return !(ctx->Texture._EnabledUnits);
}

void
via_read_pixels(GLcontext * ctx,
		GLint x, GLint y, GLsizei width, GLsizei height,
		GLenum format, GLenum type,
		const struct gl_pixelstore_attrib *pack, GLvoid * pixels)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_buffer_object *dst = via_buffer_object(pack->BufferObj);
    uint32_t cpp;
    uint32_t src_cpp;
    uint32_t dst_fmt;
    uint32_t src_fmt;
    uint32_t stride;
    uint32_t dst_offset;
    uint32_t dst_xoff;
    uint32_t xReadable = 0;
    uint32_t src_offset = 0;
    uint32_t readableWidth;
    uint32_t scale_rgba;
    uint32_t bias_rgba;
    struct drm_via_clip_rect clip;
    struct drm_via_clip_rect dst_clip;
    struct drm_via_clip_rect tmp_clip;
    struct drm_via_clip_rect *cur_clip;
    int i;
    struct via_framebuffer *vfb = viaReadFrameBuffer(vmesa);
    struct via_renderbuffer *read_buf = viaReadRenderBuffer(vmesa);

    if (!read_buf)
	goto out_sw;

    if (!via_compute_image_transfer(ctx, &scale_rgba, &bias_rgba)) {
	goto out_sw;
    }

    if (!dst) {
	goto out_sw;
    }

    src_fmt = read_buf->texFormat;
    if (src_fmt == VIA_FMT_ERROR)
	goto out_sw;

    src_cpp = (read_buf->bpp + 7) >> 3;

    dst_fmt = via_dst_format_to_hw(format, type, &cpp);
    if (dst_fmt == VIA_FMT_ERROR) {
	goto out_sw;
    }

    stride = cpp * ((pack->RowLength > 0) ? pack->RowLength : width);
    stride = (stride + pack->Alignment - 1) & ~(pack->Alignment - 1);

    if (stride & 0x0f)
	goto out_sw;

    dst_offset = (unsigned long)_mesa_image_address(2, pack, pixels,
						    width, height,
						    format, type, 0, 0, 0);
    if (dst_offset % cpp != 0) {
	goto out_sw;
    }

    readableWidth = vfb->Base.Width;
    if (read_buf->isSharedFrontBuffer) {
	volatile struct drm_via_scanout *front = &vmesa->sarea->scanouts[0];

	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);
	src_offset = read_buf->pitch * vfb->drawY;
	readableWidth = front->width;
	xReadable = vfb->drawX;
    }

    dst_xoff = (dst_offset & 0x0f) / cpp;
    clip.x1 = dst_xoff;
    clip.x2 = width + dst_xoff;
    clip.y1 = 0;
    clip.y2 = height;
    dst_offset &= ~0x0f;
    via_meta_install_src(vmesa, scale_rgba, bias_rgba, src_fmt,
			 1.0f, 1.0f,
			 readableWidth, vfb->Base.Height,
			 read_buf->pitch, src_offset,
			 (vfb->Base.Name == 0), read_buf->buf);

    vmesa->clearTexCache = 1;
    via_meta_install_dst_bufobj(vmesa, dst_fmt, stride, height, dst_offset,
				dst_xoff, &clip, via_bufferobj_buffer(dst));

    clip.x1 = x;
    clip.x2 = x + width;
    clip.y1 = y;
    clip.y2 = y + height;

    cur_clip = vmesa->pReadClipRects;
    for (i = 0; i < vmesa->numReadClipRects; ++i) {
	tmp_clip = *cur_clip;

	if (vfb->Base.Name == 0) {

	    /*
	     * Window system FB. Flip Y.
	     */

	    tmp_clip.y1 = vfb->Base.Height - cur_clip->y2;
	    tmp_clip.y2 = vfb->Base.Height - cur_clip->y1;
	}

	if (via_intersect_via_rect(&dst_clip, &clip, &tmp_clip)) {

	    dst_clip.x1 -= x;
	    dst_clip.x2 -= x;
	    dst_clip.y1 -= y;
	    dst_clip.y2 -= y;

	    via_meta_emit_src_clip(vmesa, x + xReadable, y, 0, 0,
				   &dst_clip, 0.5);
	}

	cur_clip++;
    }

    VIA_FLUSH_DMA(vmesa);

    if (read_buf->isSharedFrontBuffer) {
	UNLOCK_HARDWARE(vmesa);
    }

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;

    return;
  out_sw:
    _swrast_ReadPixels(ctx, x, y, width, height, format, type, pack, pixels);

    return;
}

void
via_draw_pixels(GLcontext * ctx,
		GLint x, GLint y,
		GLsizei width, GLsizei height,
		GLenum format,
		GLenum type,
		const struct gl_pixelstore_attrib *unpack,
		const GLvoid * pixels)
{
    uint32_t scale_rgba;
    uint32_t bias_rgba;
    uint32_t src_fmt;
    struct via_buffer_object *src = via_buffer_object(unpack->BufferObj);
    struct _WsbmBufferObject *buf = NULL;
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    uint32_t cpp;
    uint32_t stride;
    uint32_t src_offset;
    struct drm_via_clip_rect clip;
    struct via_renderbuffer *draw_buf = viaDrawRenderBuffer(vmesa);

    if (!draw_buf)
	goto out_sw;

    if (!via_check_fragment_ops(ctx) ||
	!via_compute_image_transfer(ctx, &scale_rgba, &bias_rgba)) {
	goto out_sw;
    }

    src_fmt = via_src_fmt_to_hw(format, type, &cpp);
    if (src_fmt == VIA_FMT_ERROR) {
	goto out_sw;
    }

    stride = cpp * ((unpack->RowLength > 0) ? unpack->RowLength : width);
    stride = (stride + unpack->Alignment - 1) & ~(unpack->Alignment - 1);

    if (src) {
	if (stride & 0x0f)
	    goto out_sw;

	src_offset = (unsigned long)_mesa_image_address(2, unpack, pixels,
							width, height,
							format, type, 0, 0,
							0);

	buf = wsbmBOReference(via_bufferobj_buffer(src));
    } else if (!src) {
	int ret;
	unsigned char *map, *savemap;
	const unsigned char *tmp_pixels;
	int col;
	uint32_t data_stride = width * cpp;
	uint32_t new_stride = (data_stride + 0x0f) & ~0x0f;

	/*
	 * Pipelined drawpixels from system memory.
	 */

	ret = wsbmGenBuffers(vmesa->viaScreen->bufferPool,
			     1, &buf, 0, VIA_PL_FLAG_AGP);
	if (ret)
	    goto out_sw;

	ret = wsbmBOData(buf, new_stride * height, NULL, NULL, 0);
	if (ret)
	    goto out_sw;

	map = wsbmBOMap(buf, WSBM_ACCESS_WRITE);
	if (!map)
	    goto out_sw;

	data_stride = cpp * ((unpack->RowLength > 0) ?
			     unpack->RowLength : width);
	data_stride = (data_stride + unpack->Alignment - 1) &
	    ~(unpack->Alignment - 1);

	tmp_pixels = _mesa_image_address(2, unpack, pixels,
					 width, height,
					 format, type, 0, 0, 0);
	if (unpack->Invert)
	    data_stride -= data_stride;

	savemap = map;
	for (col = 0; col < height; ++col) {
	    memcpy(map, tmp_pixels, data_stride);
	    map += new_stride;
	    tmp_pixels += stride;
	}

	wsbmBOUnmap(buf);

	src_offset = 0;
	stride = new_stride;
    }

    via_meta_install_src(vmesa, scale_rgba, bias_rgba, src_fmt,
			 ctx->Pixel.ZoomX, ctx->Pixel.ZoomY, width, height,
			 stride, src_offset, GL_FALSE, buf);
    via_meta_install_dst_fb(vmesa);

    vmesa->clearTexCache = 1;

    clip.x1 = 0;
    clip.x2 = width;
    clip.y1 = 0;
    clip.y2 = height;

    via_meta_emit_src_clip(vmesa, 0, 0,
			   x, y, &clip, ctx->Current.RasterPos[2]);

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;
    wsbmBOUnreference(&buf);

    return;
  out_sw:
    wsbmBOUnreference(&buf);
    _swrast_DrawPixels(ctx, x, y, width, height, format, type, unpack,
		       pixels);

    return;
}

static void
via_copy_pixels(GLcontext * ctx,
		GLint srcx, GLint srcy,
		GLsizei width, GLsizei height,
		GLint destx, GLint desty, GLenum type)
{
    if (via_blit_copypixels
	(ctx, srcx, srcy, width, height, destx, desty, type))
	return;

    _swrast_CopyPixels(ctx, srcx, srcy, width, height, destx, desty, type);
}

void
viaInitPixelFuncs(struct dd_function_table *functions)
{
    functions->Accum = _swrast_Accum;
    functions->Bitmap = _swrast_Bitmap;
    functions->CopyPixels = via_copy_pixels;
    functions->ReadPixels = via_read_pixels;
    functions->DrawPixels = via_draw_pixels;
}
