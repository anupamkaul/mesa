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

#ifndef _VIATEX_H
#define _VIATEX_H

#include "main/mtypes.h"
#include "via_ioctl.h"

#define VIA_MAX_TEXLEVELS	12
#define VIA_TEXIMAGE_MAGIC 0x12344321

struct via_context;

struct via_texture_image
{
    struct gl_texture_image image;
    struct _WsbmBufferObject *buf;
    GLint pitchLog2;
    GLuint magic;
    uint32_t texHwFormat;
    uint32_t dstHwFormat;
    GLuint mesaFormat;
    GLuint actualFormat;
};

struct via_texture_object
{
    struct gl_texture_object obj;      /* The "parent" object */

    GLuint texelBytes;

    GLuint regTexFM;
    GLuint regTexWidthLog2[2];
    GLuint regTexHeightLog2[2];
    GLuint pitchLog2[VIA_MAX_TEXLEVELS];
    struct via_reloc_texlist addr[12];

    GLint firstLevel, lastLevel;       /* upload tObj->Image[first .. lastLevel] */

    GLboolean imagesInVRAM;
};

GLboolean viaMoveTexImageToVRAM(GLcontext * ctx,
				struct via_texture_image *viaImage);
GLboolean viaUpdateTextureState(GLcontext * ctx);
void viaInitTextureFuncs(struct dd_function_table *functions);
int via_map_unmap_texunit(struct gl_texture_unit *tu, GLboolean map);

extern GLboolean
via_try_3d_upload(GLcontext * ctx,
		  GLint xoffset, GLint yoffset,
		  GLsizei width, GLsizei height,
		  GLenum format,
		  GLenum type,
		  const void *pixels,
		  const struct gl_pixelstore_attrib *packing,
		  struct via_texture_image *viaImage);

GLboolean
via_try_3d_download(GLcontext * ctx,
		    GLint x, GLint y,
		    GLsizei width, GLsizei height,
		    GLenum format,
		    GLenum type,
		    const void *pixels,
		    const struct gl_pixelstore_attrib *packing,
		    struct via_texture_image *viaImage);

extern GLboolean
via_try_3d_copy(GLcontext * ctx,
		GLint xoffset, GLint yoffset,
		GLint x, GLint y, GLsizei width, GLsizei height,
		struct via_texture_image *viaImage);

/*
 * (Re)generate one mipmap level to dst with src as start,
 * using texturing with linear interpolation.
 */

extern GLboolean
via_generate_one_mipmap(GLcontext * ctx,
			GLuint x,
			GLuint y,
			GLuint width,
			GLuint height,
			struct via_texture_image *src,
			struct via_texture_image *dst);

/*
 * Change when we support ARB_texture_rectangle.
 */

static inline int
via_teximage_stride(const struct via_texture_image *viaImage)
{
    return (1 << viaImage->pitchLog2);
}

#if defined( USE_SSE_ASM )
void *via_sse_memcpy(void *to, const void *from, size_t sz);
#endif /* defined( USE_SSE_ASM ) */

#endif
