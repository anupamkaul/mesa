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

#include "via_pixel.h"
#include "via_context.h"
#include "via_ioctl.h"
#include "via_buffer_objects.h"

#include "wsbm_manager.h"

GLboolean
via_generate_one_mipmap(GLcontext * ctx,
			GLuint x,
			GLuint y,
			GLuint width,
			GLuint height,
			struct via_texture_image * src,
			struct via_texture_image * dst)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct gl_texture_image *srcImage = &src->image;
    struct gl_texture_image *dstImage = &dst->image;
    uint32_t dst_fmt;
    uint32_t src_fmt;
    uint32_t stride;
    uint32_t src_stride;
    struct drm_via_clip_rect clip;

    if (src->texHwFormat == VIA_FMT_ERROR ||
	dst->dstHwFormat == VIA_FMT_ERROR)
	return GL_FALSE;

    src_fmt = src->texHwFormat;
    dst_fmt = dst->dstHwFormat;
    stride = via_teximage_stride(dst);

    if (stride & 0x0F)
	return GL_FALSE;

    src_stride = via_teximage_stride(src);
    if (src_stride & 0x0F)
	return GL_FALSE;

    via_meta_install_src(vmesa, 0xFFFFFFFF, 0x00000000, src_fmt,
			 0.5f, 0.5f, srcImage->Width,
			 srcImage->Height, src_stride, 0, GL_FALSE, src->buf);

    clip.x1 = 0;
    clip.x2 = dstImage->Width;
    clip.y1 = 0;
    clip.y2 = dstImage->Height;

    vmesa->clearTexCache = 1;
    via_meta_install_dst_bufobj(vmesa, dst_fmt, stride, dstImage->Height, 0,
				0, &clip, dst->buf);

    clip.x1 = x & ~1;
    clip.x2 = (x + width + 1) & ~1;
    clip.y1 = y & ~1;
    clip.y2 = (y + height + 1) & ~1;

    via_meta_emit_src_clip(vmesa, 0, 0, 0, 0, &clip, 0.5f);

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;

    return GL_TRUE;
}

GLboolean
via_try_3d_upload(GLcontext * ctx,
		  GLint xoffset, GLint yoffset,
		  GLsizei width, GLsizei height,
		  GLenum format,
		  GLenum type,
		  const void *pixels,
		  const struct gl_pixelstore_attrib * packing,
		  struct via_texture_image * viaImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    uint32_t cpp;
    uint32_t src_cpp;
    uint32_t dst_fmt;
    uint32_t src_fmt;
    uint32_t stride;
    uint32_t src_offset;
    uint32_t scale_rgba;
    uint32_t bias_rgba;
    uint32_t src_stride;
    struct drm_via_clip_rect clip;
    struct via_buffer_object *src = via_buffer_object(packing->BufferObj);

    if (!src)
	return GL_FALSE;

    if (!via_compute_image_transfer(ctx, &scale_rgba, &bias_rgba))
	return GL_FALSE;

    src_fmt = via_src_fmt_to_hw(format, type, &src_cpp);

    if (src_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    dst_fmt = viaImage->dstHwFormat;
    if (dst_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    cpp = viaImage->image.TexFormat->TexelBytes;
    stride = via_teximage_stride(viaImage);
    if (stride & 0x0F)
	return GL_FALSE;

    src_stride = src_cpp * ((packing->RowLength > 0) ?
			    packing->RowLength : width);
    src_stride = (src_stride + (packing->Alignment - 1)) &
	~(packing->Alignment - 1);
    if (src_stride & 0x0F)
	return GL_FALSE;

    src_offset = (unsigned long)_mesa_image_address(2, packing, pixels,
						    width, height,
						    format, type, 0, 0, 0);

    via_meta_install_src(vmesa, scale_rgba, bias_rgba, src_fmt,
			 1.0f, 1.0f, width, height,
			 src_stride, src_offset,
			 GL_FALSE, via_bufferobj_buffer(src));

    clip.x1 = 0;
    clip.x2 = width;
    clip.y1 = 0;
    clip.y2 = height;

    vmesa->clearTexCache = 1;
    via_meta_install_dst_bufobj(vmesa, dst_fmt, stride, height, 0,
				0, &clip, viaImage->buf);

    clip.x1 = 0;
    clip.x2 = width;
    clip.y1 = 0;
    clip.y2 = height;

    via_meta_emit_src_clip(vmesa, 0, 0, xoffset, yoffset, &clip, 0.5f);

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;

    return GL_TRUE;
}

GLboolean
via_try_3d_download(GLcontext * ctx,
		    GLint x, GLint y,
		    GLsizei width, GLsizei height,
		    GLenum format,
		    GLenum type,
		    const void *pixels,
		    const struct gl_pixelstore_attrib * packing,
		    struct via_texture_image * viaImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    uint32_t cpp;
    uint32_t src_cpp;
    uint32_t dst_fmt;
    uint32_t src_fmt;
    uint32_t stride;
    uint32_t scale_rgba;
    uint32_t bias_rgba;
    uint32_t src_stride;
    uint32_t dst_offset;
    uint32_t dst_xoff;
    struct drm_via_clip_rect clip;
    struct via_buffer_object *dst = via_buffer_object(packing->BufferObj);

    if (!dst)
	return GL_FALSE;

    if (!via_compute_image_transfer(ctx, &scale_rgba, &bias_rgba))
	return GL_FALSE;

    src_fmt = viaImage->texHwFormat;
    if (src_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    dst_fmt = via_dst_format_to_hw(format, type, &cpp);
    if (dst_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    src_cpp = viaImage->image.TexFormat->TexelBytes;
    src_stride = via_teximage_stride(viaImage);
    if (src_stride & 0x0F)
	return GL_FALSE;

    stride = cpp * ((packing->RowLength > 0) ? packing->RowLength : width);
    stride = (stride + packing->Alignment - 1) & ~(packing->Alignment - 1);
    if (stride & 0x0F)
	return GL_FALSE;

    dst_offset = (unsigned long)_mesa_image_address(2, packing, pixels,
						    width, height,
						    format, type, 0, 0, 0);
    if (dst_offset % cpp != 0)
	return GL_FALSE;

    via_meta_install_src(vmesa, scale_rgba, bias_rgba, src_fmt,
			 1.0f, 1.0f, width, height,
			 src_stride, 0, GL_FALSE, viaImage->buf);

    dst_xoff = (dst_offset & 0x0f) / cpp;
    dst_offset &= ~0x0f;

    clip.x1 = dst_xoff;
    clip.x2 = width + dst_xoff;
    clip.y1 = 0;
    clip.y2 = height;

    vmesa->clearTexCache = 1;
    via_meta_install_dst_bufobj(vmesa, dst_fmt, stride, height, dst_offset,
				dst_xoff, &clip, via_bufferobj_buffer(dst));

    /*
     * FIXME: Do we need to intersect with the teximage rectangle?
     */

    clip.x1 = 0;
    clip.x2 = width;
    clip.y1 = 0;
    clip.y2 = height;

    via_meta_emit_src_clip(vmesa, x, y, 0, 0, &clip, 0.5f);

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;

    return GL_TRUE;
}

GLboolean
via_try_3d_copy(GLcontext * ctx,
		GLint xoffset, GLint yoffset,
		GLint x, GLint y, GLsizei width, GLsizei height,
		struct via_texture_image * viaImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    uint32_t cpp;
    uint32_t src_cpp;
    uint32_t dst_fmt;
    uint32_t src_fmt;
    uint32_t stride;
    uint32_t xReadable = 0;
    uint32_t src_offset = 0;
    uint32_t readableWidth;
    uint32_t scale_rgba;
    uint32_t bias_rgba;
    struct drm_via_clip_rect clip;
    struct drm_via_clip_rect dst_clip;
    struct drm_clip_rect tmp_clip;
    struct drm_clip_rect *cur_clip;
    int i;
    struct via_framebuffer *vfb = viaReadFrameBuffer(vmesa);
    struct via_renderbuffer *read_buf = viaReadRenderBuffer(vmesa);

    if (!read_buf)
	return GL_FALSE;

    if (!via_compute_image_transfer(ctx, &scale_rgba, &bias_rgba))
	return GL_FALSE;

    src_fmt = read_buf->texFormat;
    if (src_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    src_cpp = (read_buf->bpp + 7) >> 3;

    dst_fmt = viaImage->dstHwFormat;
    if (dst_fmt == VIA_FMT_ERROR)
	return GL_FALSE;

    cpp = viaImage->image.TexFormat->TexelBytes;

    stride = via_teximage_stride(viaImage);
    if (stride & 0x0F)
	return GL_FALSE;

    readableWidth = vfb->Base.Width;
    if (read_buf->isSharedFrontBuffer) {
	volatile struct drm_via_scanout *front = &vmesa->sarea->scanouts[0];

	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);
	src_offset = read_buf->pitch * vfb->drawY;
	readableWidth = front->width;
	xReadable = vfb->drawX;
    }

    via_meta_install_src(vmesa, scale_rgba, bias_rgba, src_fmt,
			 1.0f, 1.0f,
			 readableWidth, vfb->Base.Height,
			 read_buf->pitch, src_offset,
			 (vfb->Base.Name == 0), read_buf->buf);

    clip.x1 = 0;
    clip.x2 = width;
    clip.y1 = 0;
    clip.y2 = height;

    vmesa->clearTexCache = 1;
    via_meta_install_dst_bufobj(vmesa, dst_fmt, stride, height, 0,
				0, &clip, viaImage->buf);

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

	if (via_intersect_drm_rect(&dst_clip, &clip, &tmp_clip)) {

	    dst_clip.x1 -= x;
	    dst_clip.x2 -= x;
	    dst_clip.y1 -= y;
	    dst_clip.y2 -= y;

	    via_meta_emit_src_clip(vmesa, x + xReadable, y,
				   xoffset, yoffset, &dst_clip, 0.5);
	}

	cur_clip++;
    }

    VIA_FLUSH_DMA(vmesa);

    if (read_buf->isSharedFrontBuffer) {
	UNLOCK_HARDWARE(vmesa);
    }

    via_meta_uninstall(vmesa);
    vmesa->clearTexCache = 1;

    return GL_TRUE;
}
