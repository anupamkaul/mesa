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

#include <stdlib.h>
#include <stdio.h>

#include "main/glheader.h"
#include "main/macros.h"
#include "main/mtypes.h"
#include "main/enums.h"
#include "main/colortab.h"
#include "main/convolve.h"
#include "main/context.h"
#include "main/mipmap.h"
#include "main/texcompress.h"
#include "main/texformat.h"
#include "main/texobj.h"
#include "main/texstore.h"
#include "main/teximage.h"
#include "swrast/swrast.h"

#include "via_context.h"
#include "via_tex.h"
#include "via_state.h"
#include "via_ioctl.h"
#include "via_3d_reg.h"

#include "wsbm_manager.h"

static const struct gl_texture_format *
viaChooseTexFormat(GLcontext * ctx, GLint internalFormat,
		   GLenum format, GLenum type)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    const GLboolean do32bpt = vmesa->viaScreen->bitsPerPixel == 32;

    switch (internalFormat) {
    case 4:
    case GL_RGBA:
	switch (format) {
	case GL_BGRA:
	    if (type == GL_UNSIGNED_INT_8_8_8_8_REV ||
		type == GL_UNSIGNED_BYTE) {
		return &_mesa_texformat_argb8888;
	    } else if (type == GL_UNSIGNED_INT_8_8_8_8) {
		return &_mesa_texformat_argb8888_rev;
	    } else if (type == GL_UNSIGNED_SHORT_4_4_4_4_REV) {
		return &_mesa_texformat_argb4444;
	    } else if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
		return &_mesa_texformat_argb1555;
	    }
	    return do32bpt ? &_mesa_texformat_argb8888 :
		&_mesa_texformat_argb4444;
	case GL_RGBA:
	    if (type == GL_UNSIGNED_INT_8_8_8_8) {
		return &_mesa_texformat_rgba8888;
	    } else if (type == GL_UNSIGNED_INT_8_8_8_8_REV ||
		       type == GL_UNSIGNED_BYTE) {
		return &_mesa_texformat_rgba8888_rev;
	    } else if (type == GL_UNSIGNED_SHORT_4_4_4_4) {
		return &_mesa_texformat_rgba4444;
	    } else if (type == GL_UNSIGNED_SHORT_5_5_5_1) {
		return &_mesa_texformat_rgba5551;
	    }
	    return do32bpt ? &_mesa_texformat_argb8888 :
		&_mesa_texformat_argb4444;
	default:
	    return do32bpt ? &_mesa_texformat_argb8888 :
		&_mesa_texformat_argb4444;
	}
    case 3:
    case GL_RGB:
	if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
	    return &_mesa_texformat_rgb565;
	} else if (type == GL_UNSIGNED_BYTE) {
	    return &_mesa_texformat_argb8888;
	}
	return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_rgb565;

    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
	return &_mesa_texformat_argb8888;

    case GL_RGBA4:
    case GL_RGBA2:
	return &_mesa_texformat_argb4444;

    case GL_RGB5_A1:
	return &_mesa_texformat_rgba5551;

    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
	return &_mesa_texformat_argb8888;

    case GL_RGB5:
    case GL_RGB4:
    case GL_R3_G3_B2:
	return &_mesa_texformat_rgb565;

    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
    case GL_COMPRESSED_ALPHA:
	return &_mesa_texformat_a8;

    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
    case GL_COMPRESSED_LUMINANCE:
	return &_mesa_texformat_l8;

    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
    case GL_COMPRESSED_LUMINANCE_ALPHA:
	return &_mesa_texformat_al88;

    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
    case GL_COMPRESSED_INTENSITY:
	return &_mesa_texformat_i8;

    case GL_YCBCR_MESA:
	if (type == GL_UNSIGNED_SHORT_8_8_MESA || type == GL_UNSIGNED_BYTE)
	    return &_mesa_texformat_ycbcr;
	else
	    return &_mesa_texformat_ycbcr_rev;

    case GL_COMPRESSED_RGB_FXT1_3DFX:
    case GL_COMPRESSED_RGB:
	return &_mesa_texformat_rgb_fxt1;
    case GL_COMPRESSED_RGBA_FXT1_3DFX:
	return &_mesa_texformat_rgba_fxt1;

    case GL_RGB_S3TC:
    case GL_RGB4_S3TC:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	return &_mesa_texformat_rgb_dxt1;

    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	return &_mesa_texformat_rgba_dxt1;

    case GL_RGBA_S3TC:
    case GL_RGBA4_S3TC:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	return &_mesa_texformat_rgba_dxt3;

    case GL_COMPRESSED_RGBA:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
	return &_mesa_texformat_rgba_dxt5;

    case GL_COLOR_INDEX:
    case GL_COLOR_INDEX1_EXT:
    case GL_COLOR_INDEX2_EXT:
    case GL_COLOR_INDEX4_EXT:
    case GL_COLOR_INDEX8_EXT:
    case GL_COLOR_INDEX12_EXT:
    case GL_COLOR_INDEX16_EXT:
	return &_mesa_texformat_ci8;

    default:
	fprintf(stderr, "unexpected texture format %s in %s\n",
		_mesa_lookup_enum_by_nr(internalFormat), __FUNCTION__);
	return NULL;
    }

    return NULL;		       /* never get here */
}

static int
logbase2(int n)
{
    GLint i = 1;
    GLint log2 = 0;

    while (n > i) {
	i *= 2;
	log2++;
    }

    return log2;
}

static GLboolean
viaTexFormat(GLuint mesaFormat, uint32_t * texFormat,
	     uint32_t * dstFormat, GLuint * actualFormat)
{
    GLboolean ret = GL_TRUE;

    *dstFormat = VIA_FMT_ERROR;
    *actualFormat = 0;

    switch (mesaFormat) {
    case MESA_FORMAT_ARGB8888:
	*texFormat = HC_HTXnFM_ARGB8888;
	*dstFormat = HC_HDBFM_ARGB8888;
	*actualFormat = GL_RGBA8;
	break;
    case MESA_FORMAT_ARGB8888_REV:
	*texFormat = HC_HTXnFM_BGRA8888;
	break;
    case MESA_FORMAT_RGBA8888:
	*texFormat = HC_HTXnFM_RGBA8888;
	break;
    case MESA_FORMAT_RGBA8888_REV:
	*texFormat = HC_HTXnFM_ABGR8888;
	*dstFormat = HC_HDBFM_ABGR8888;
	*actualFormat = GL_RGBA8;
	break;
    case MESA_FORMAT_ARGB4444:
	*texFormat = HC_HTXnFM_ARGB4444;
	*dstFormat = HC_HDBFM_ARGB4444;
	*actualFormat = GL_RGBA4;
	break;
    case MESA_FORMAT_RGBA4444:
	*texFormat = HC_HTXnFM_RGBA4444;
	break;
    case MESA_FORMAT_RGB565:
	*texFormat = HC_HTXnFM_RGB565;
	*dstFormat = HC_HDBFM_RGB565;
	*actualFormat = GL_RGB5;
	break;
    case MESA_FORMAT_RGBA5551:
	*texFormat = HC_HTXnFM_RGBA5551;
	break;
    case MESA_FORMAT_ARGB1555:
	*texFormat = HC_HTXnFM_ARGB1555;
	*dstFormat = HC_HDBFM_ARGB1555;
	*actualFormat = GL_RGB5_A1;
	break;
    case MESA_FORMAT_RGB888:
	*texFormat = HC_HTXnFM_ARGB0888;
	*dstFormat = HC_HDBFM_ARGB0888;
	*actualFormat = GL_RGB8;
	break;
    case MESA_FORMAT_L8:
	*texFormat = HC_HTXnFM_L8;
	break;
    case MESA_FORMAT_I8:
	*texFormat = HC_HTXnFM_T8;
	break;
    case MESA_FORMAT_CI8:
	*texFormat = HC_HTXnFM_Index8;
	break;
    case MESA_FORMAT_AL88:
	*texFormat = HC_HTXnFM_AL88;
	break;
    case MESA_FORMAT_A8:
	*texFormat = HC_HTXnFM_A8;
	break;
    case MESA_FORMAT_RGB_DXT1:
    case MESA_FORMAT_RGBA_DXT1:
	*texFormat = HC_HTXnFM_DX1;
	break;
    case MESA_FORMAT_RGBA_DXT3:
	*texFormat = HC_HTXnFM_DX23;
	break;
    case MESA_FORMAT_RGBA_DXT5:
	*texFormat = HC_HTXnFM_DX45;
	break;
    default:
	*texFormat = VIA_FMT_ERROR;
	ret = GL_FALSE;
    }

    return ret;
}

/* Basically, just collect the image dimensions and addresses for each
 * image and update the texture object state accordingly.
 */

static GLboolean
viaSetTexImages(GLcontext * ctx, struct gl_texture_object *texObj)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_object *viaObj = (struct via_texture_object *)texObj;
    const struct via_texture_image *baseImage =
	(struct via_texture_image *)texObj->Image[0][texObj->BaseLevel];
    GLint firstLevel, lastLevel, numLevels;
    GLuint texFormat;
    GLint w, h, p;
    GLint i, l = 0, m = 0;
    GLuint widthExp = 0;
    GLuint heightExp = 0;
    int ret = GL_TRUE;

    texFormat = baseImage->texHwFormat;

    /* Compute which mipmap levels we really want to send to the hardware.
     * This depends on the base image size, GL_TEXTURE_MIN_LOD,
     * GL_TEXTURE_MAX_LOD, GL_TEXTURE_BASE_LEVEL, and GL_TEXTURE_MAX_LEVEL.
     * Yes, this looks overly complicated, but it's all needed.
     */
    if (texObj->MinFilter == GL_LINEAR || texObj->MinFilter == GL_NEAREST) {
	firstLevel = lastLevel = texObj->BaseLevel;
    } else {
	firstLevel = texObj->BaseLevel + (GLint) (texObj->MinLod + 0.5);
	firstLevel = MAX2(firstLevel, texObj->BaseLevel);
	lastLevel = texObj->BaseLevel + (GLint) (texObj->MaxLod + 0.5);
	lastLevel = MAX2(lastLevel, texObj->BaseLevel);
	lastLevel =
	    MIN2(lastLevel, texObj->BaseLevel + baseImage->image.MaxLog2);
	lastLevel = MIN2(lastLevel, texObj->MaxLevel);
	lastLevel = MAX2(firstLevel, lastLevel);	/* need at least one level */
    }

    numLevels = lastLevel - firstLevel + 1;

    /*
     * FIXME: Add more devices here when we've checked how many
     * mipmap levels they support.
     */

    switch (vmesa->viaScreen->deviceID) {
    case VIA_CX700:
	break;
    case VIA_CLE266:
    case VIA_K8M800:
    default:

	/*
	 * Most Unichromes only have 10 mipmap levels. Ignore higher levels
	 * if the user has requested a large baseLevel.
	 */

	if ((numLevels > 10) && (ctx->Const.MaxTextureLevels > 10)) {
	    lastLevel -= numLevels - 10;
	    numLevels = 10;
	}
	break;
    }

    /* save these values, check if they effect the residency of the
     * texture:
     */
    if (viaObj->firstLevel != firstLevel || viaObj->lastLevel != lastLevel) {
	viaObj->firstLevel = firstLevel;
	viaObj->lastLevel = lastLevel;
    }

    if (ret == GL_FALSE)
	return ret;

    viaObj->regTexFM = (HC_SubA_HTXnFM << 24) | texFormat;

    for (i = 0; i < numLevels; i++) {
	struct via_reloc_texlist *addr = &viaObj->addr[i];
	struct via_texture_image *viaImage =
	    (struct via_texture_image *)texObj->Image[0][firstLevel + i];

	w = viaImage->image.WidthLog2;
	h = viaImage->image.HeightLog2;
	p = viaImage->pitchLog2;

	/* All images must be in the same memory space, so if any of them has
	 * been used as a render target, we need to migrate all of them to VRAM.
	 */
	assert(viaImage->buf != NULL);

	viaObj->pitchLog2[i] = ((HC_SubA_HTXnL0Pit + i) << 24) | (p << 20);

	addr->delta = 0;
	addr->buf = viaImage->buf;

	/* Sets of 6 log2width and log2height values are
	 * packed into individual registers:
	 */
	l = i / 6;
	m = i % 6;
	widthExp |= (((GLuint) w & 0xF) << (m << 2));
	heightExp |= (((GLuint) h & 0xF) << (m << 2));
	if (m == 5) {
	    viaObj->regTexWidthLog2[l] =
		(l + HC_SubA_HTXnL0_5WE) << 24 | widthExp;
	    viaObj->regTexHeightLog2[l] =
		(l + HC_SubA_HTXnL0_5HE) << 24 | heightExp;
	    widthExp = 0;
	    heightExp = 0;
	}
	if (w)
	    w--;
	if (h)
	    h--;
	if (p)
	    p--;
    }

    if (m != 5) {
	viaObj->regTexWidthLog2[l] =
	    (l + HC_SubA_HTXnL0_5WE) << 24 | widthExp;
	viaObj->regTexHeightLog2[l] =
	    (l + HC_SubA_HTXnL0_5HE) << 24 | heightExp;
    }

    return GL_TRUE;
}

GLboolean
viaUpdateTextureState(GLcontext * ctx)
{
    struct gl_texture_unit *texUnit = ctx->Texture.Unit;
    GLuint i;

    for (i = 0; i < 2; i++) {
	if (texUnit[i]._ReallyEnabled == TEXTURE_2D_BIT ||
	    texUnit[i]._ReallyEnabled == TEXTURE_1D_BIT) {

	    if (!viaSetTexImages(ctx, texUnit[i]._Current))
		return GL_FALSE;
	} else if (texUnit[i]._ReallyEnabled) {
	    return GL_FALSE;
	}
    }

    return GL_TRUE;
}

static void
viaTexImage(GLcontext * ctx,
	    GLint dims,
	    GLenum target, GLint level,
	    GLint internalFormat,
	    GLint width, GLint height, GLint border,
	    GLenum format, GLenum type, const void *pixels,
	    const struct gl_pixelstore_attrib *packing,
	    struct gl_texture_object *texObj,
	    struct gl_texture_image *texImage, int imageSize, int compressed)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    GLint postConvWidth = width;
    GLint postConvHeight = height;
    GLint texelBytes, sizeInBytes;
    GLint dstRowStride;
    GLboolean success;
    struct via_texture_object *viaObj = (struct via_texture_object *)texObj;
    struct via_texture_image *viaImage = (struct via_texture_image *)texImage;
    int ret;

    if (ctx->_ImageTransferState & IMAGE_CONVOLUTION_BIT) {
	_mesa_adjust_image_for_convolution(ctx, dims, &postConvWidth,
					   &postConvHeight);
    }

    /* choose the texture format */
    texImage->TexFormat = viaChooseTexFormat(ctx, internalFormat,
					     format, type);

    viaImage->mesaFormat = texImage->TexFormat->MesaFormat;
    viaTexFormat(viaImage->mesaFormat, &viaImage->texHwFormat,
		 &viaImage->dstHwFormat, &viaImage->actualFormat);

    assert(texImage->TexFormat);

    _mesa_set_fetch_functions(texImage, dims);

    texelBytes = texImage->TexFormat->TexelBytes;

    if (texelBytes == 0) {
	/* compressed format */
	texImage->IsCompressed = GL_TRUE;
	texImage->CompressedSize =
	    ctx->Driver.CompressedTextureSize(ctx, texImage->Width,
					      texImage->Height,
					      texImage->Depth,
					      texImage->TexFormat->
					      MesaFormat);
	if (texImage->TexFormat->MesaFormat == MESA_FORMAT_RGB_DXT1 ||
	    texImage->TexFormat->MesaFormat == MESA_FORMAT_RGBA_DXT1)
	    viaImage->pitchLog2 = logbase2(width * 2);
	else
	    viaImage->pitchLog2 = logbase2(width * 4);
    }

    /* Minimum pitch of 32 bytes */
    if (!texImage->IsCompressed && postConvWidth * texelBytes < 32) {
	postConvWidth = 32 / texelBytes;
	texImage->RowStride = postConvWidth;
    }

    if (!texImage->IsCompressed) {
	assert(texImage->RowStride == postConvWidth);
	viaImage->pitchLog2 = logbase2(postConvWidth * texelBytes);
    }

    /* allocate memory */
    if (texImage->IsCompressed)
	sizeInBytes = texImage->CompressedSize;
    else
	sizeInBytes = postConvWidth * postConvHeight * texelBytes;

    if (!viaImage->buf) {
	ret = wsbmGenBuffers(vmesa->viaScreen->bufferPool,
			     1, &viaImage->buf, 0,
			     (viaObj->imagesInVRAM ? WSBM_PL_FLAG_VRAM :
			      WSBM_PL_FLAG_TT));
	if (ret) {
	    _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
	    return;
	}
    }

    ret = wsbmBOData(viaImage->buf, sizeInBytes, NULL,
		     vmesa->viaScreen->bufferPool,
		     (viaObj->imagesInVRAM ? WSBM_PL_FLAG_VRAM :
		      WSBM_PL_FLAG_TT));

    if (ret) {
	_mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
	return;
    }

    if (via_try_3d_upload(ctx, 0, 0,
			  width, height, format,
			  type, pixels, packing, viaImage)) {
	struct via_texture_object *viaObj =
	    containerOf(texObj, struct via_texture_object, obj);
	viaObj->imagesInVRAM = GL_TRUE;
	return;
    }

    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_WRITE);
    vmesa->clearTexCache = 1;

    if (compressed) {
	pixels =
	    _mesa_validate_pbo_compressed_teximage(ctx, imageSize, pixels,
						   packing,
						   "glCompressedTexImage");
    } else {
	pixels = _mesa_validate_pbo_teximage(ctx, dims, width, height, 1,
					     format, type,
					     pixels, packing, "glTexImage");
    }

    if (!pixels) {
	/* Note: we check for a NULL image pointer here, _after_ we allocated
	 * memory for the texture.  That's what the GL spec calls for.
	 */
	goto out;
    }

    if (texImage->IsCompressed) {
	dstRowStride =
	    _mesa_compressed_row_stride(texImage->TexFormat->MesaFormat,
					width);
    } else {
	dstRowStride = postConvWidth * texImage->TexFormat->TexelBytes;
    }

    if (compressed) {
	ctx->Driver.TextureMemCpy(texImage->Data, pixels, imageSize);
    } else {
	ASSERT(texImage->TexFormat->StoreImage);
	success = texImage->TexFormat->StoreImage(ctx, dims, texImage->_BaseFormat, texImage->TexFormat, texImage->Data, 0, 0, 0,	/* dstX/Y/Zoffset */
						  dstRowStride,
						  texImage->ImageOffsets,
						  width, height, 1,
						  format, type, pixels,
						  packing);
	if (!success) {
	    _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
	}
    }

#if 0
    /* GL_SGIS_generate_mipmap */
    if (level == texObj->BaseLevel && texObj->GenerateMipmap) {
	_mesa_generate_mipmap(ctx, target,
			      &ctx->Texture.Unit[ctx->Texture.CurrentUnit],
			      texObj);
    }
#endif

    _mesa_unmap_teximage_pbo(ctx, packing);
  out:
    texImage->Data = NULL;
    wsbmBOUnmap(viaImage->buf);
}

static void
viaTexImage2D(GLcontext * ctx,
	      GLenum target, GLint level,
	      GLint internalFormat,
	      GLint width, GLint height, GLint border,
	      GLenum format, GLenum type, const void *pixels,
	      const struct gl_pixelstore_attrib *packing,
	      struct gl_texture_object *texObj,
	      struct gl_texture_image *texImage)
{
    viaTexImage(ctx, 2, target, level,
		internalFormat, width, height, border,
		format, type, pixels, packing, texObj, texImage, 0, 0);
}

static void
viaCompressedTexImage(GLcontext * ctx,
		      GLenum target,
		      GLint level,
		      GLint internalFormat,
		      GLint width,
		      GLint height,
		      GLint border,
		      GLsizei imageSize,
		      const GLvoid * pixels,
		      struct gl_texture_object *texObj,
		      struct gl_texture_image *texImage)
{
    viaTexImage(ctx, 2, target, level,
		internalFormat, width, height, border,
		0, 0, pixels, &ctx->Unpack, texObj, texImage, imageSize, 1);
}

static void
viaTexSubImage2D(GLcontext * ctx,
		 GLenum target,
		 GLint level,
		 GLint xoffset, GLint yoffset,
		 GLsizei width, GLsizei height,
		 GLenum format, GLenum type,
		 const GLvoid * pixels,
		 const struct gl_pixelstore_attrib *packing,
		 struct gl_texture_object *texObj,
		 struct gl_texture_image *texImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_image *viaImage =
	containerOf(texImage, struct via_texture_image, image);

    if (via_try_3d_upload(ctx, xoffset, yoffset,
			  width, height, format,
			  type, pixels, packing, viaImage)) {
	struct via_texture_object *viaObj =
	    containerOf(texObj, struct via_texture_object, obj);
	viaObj->imagesInVRAM = GL_TRUE;
	return;
    }

    if (wsbmBOOnList(viaImage->buf)) {
	VIA_FLUSH_DMA(vmesa);
    }

    wsbmBOWaitIdle(viaImage->buf, 0);

    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_WRITE);
    vmesa->clearTexCache = 1;

    _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width,
			      height, format, type, pixels, packing, texObj,
			      texImage);
    texImage->Data = NULL;
    wsbmBOUnmap(viaImage->buf);
}

static void
viaTexImage1D(GLcontext * ctx,
	      GLenum target, GLint level,
	      GLint internalFormat,
	      GLint width, GLint border,
	      GLenum format, GLenum type, const void *pixels,
	      const struct gl_pixelstore_attrib *packing,
	      struct gl_texture_object *texObj,
	      struct gl_texture_image *texImage)
{
    viaTexImage(ctx, 1, target, level,
		internalFormat, width, 1, border,
		format, type, pixels, packing, texObj, texImage, 0, 0);
}

static void
viaTexSubImage1D(GLcontext * ctx,
		 GLenum target,
		 GLint level,
		 GLint xoffset,
		 GLsizei width,
		 GLenum format, GLenum type,
		 const GLvoid * pixels,
		 const struct gl_pixelstore_attrib *packing,
		 struct gl_texture_object *texObj,
		 struct gl_texture_image *texImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_image *viaImage =
	containerOf(texImage, struct via_texture_image, image);

    if (wsbmBOOnList(viaImage->buf)) {
	VIA_FLUSH_DMA(vmesa);
    }

    wsbmBOWaitIdle(viaImage->buf, 0);
    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_WRITE);
    vmesa->clearTexCache = 1;

    _mesa_store_texsubimage1d(ctx, target, level, xoffset, width,
			      format, type, pixels, packing, texObj,
			      texImage);
    texImage->Data = NULL;
    wsbmBOUnmap(viaImage->buf);
}

static GLboolean
viaIsTextureResident(GLcontext * ctx, struct gl_texture_object *texObj)
{
    return GL_TRUE;
}

static struct gl_texture_image *
viaNewTextureImage(GLcontext * ctx)
{
    (void)ctx;
    struct via_texture_image *viaImage =
	(struct via_texture_image *)CALLOC_STRUCT(via_texture_image);

    if (!viaImage)
	return NULL;

    viaImage->magic = VIA_TEXIMAGE_MAGIC;
    viaImage->image.DriverData = &viaImage->magic;

    return &viaImage->image;
}

static struct gl_texture_object *
viaNewTextureObject(GLcontext * ctx, GLuint name, GLenum target)
{
    struct via_texture_object *obj = CALLOC_STRUCT(via_texture_object);

    _mesa_initialize_texture_object(&obj->obj, name, target);
    (void)ctx;

    return &obj->obj;
}

static void
viaFreeTextureImageData(GLcontext * ctx, struct gl_texture_image *texImage)
{
    struct via_texture_image *image =
	containerOf(texImage, struct via_texture_image, image);

    if (image->buf) {
	wsbmBOUnreference(&image->buf);
	image->buf = NULL;
    }

    texImage->Data = NULL;
}

static void
viaGetTexImage(GLcontext * ctx, GLenum target, GLint level,
	       GLenum format, GLenum type, GLvoid * pixels,
	       struct gl_texture_object *texObj,
	       struct gl_texture_image *texImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_image *viaImage =
	containerOf(texImage, struct via_texture_image, image);

    if (wsbmBOOnList(viaImage->buf)) {
	VIA_FLUSH_DMA(vmesa);
    }

    wsbmBOWaitIdle(viaImage->buf, 0);

    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_READ);
    _mesa_get_teximage(ctx, target, level, format, type, pixels,
		       texObj, texImage);

    texImage->Data = NULL;
    wsbmBOUnmap(viaImage->buf);
}

static void
viaGetCompressedTexImage(GLcontext * ctx, GLenum target, GLint level,
			 GLvoid * img,
			 struct gl_texture_object *texObj,
			 struct gl_texture_image *texImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_image *viaImage =
	containerOf(texImage, struct via_texture_image, image);

    if (wsbmBOOnList(viaImage->buf)) {
	VIA_FLUSH_DMA(vmesa);
    }
    wsbmBOWaitIdle(viaImage->buf, 0);
    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_READ);

    _mesa_get_compressed_teximage(ctx, target, level, img, texObj, texImage);

    wsbmBOUnmap(viaImage->buf);
}

static void
viaCompressedTexSubImage2D(GLcontext * ctx, GLenum target, GLint level,
			   GLint xoffset, GLint yoffset,
			   GLsizei width, GLint height,
			   GLenum format,
			   GLsizei imageSize, const GLvoid * data,
			   struct gl_texture_object *texObj,
			   struct gl_texture_image *texImage)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_texture_image *viaImage =
	containerOf(texImage, struct via_texture_image, image);

    if (wsbmBOOnList(viaImage->buf)) {
	VIA_FLUSH_DMA(vmesa);
    }
    wsbmBOWaitIdle(viaImage->buf, 0);
    texImage->Data = wsbmBOMap(viaImage->buf, WSBM_SYNCCPU_WRITE);

    _mesa_store_compressed_texsubimage2d(ctx, target, level, xoffset,
					 yoffset, width, height, format,
					 imageSize, data, texObj, texImage);

    texImage->Data = NULL;
    wsbmBOUnmap(viaImage->buf);
}

static void
viaCopyTexSubImage2D(GLcontext * ctx, GLenum target, GLint level,
		     GLint xoffset, GLint yoffset,
		     GLint x, GLint y, GLsizei width, GLsizei height)
{
    struct gl_texture_unit *texUnit =
	&ctx->Texture.Unit[ctx->Texture.CurrentUnit];
    struct gl_texture_object *texObj =
	_mesa_select_tex_object(ctx, texUnit, target);
    struct gl_texture_image *texImage =
	_mesa_select_tex_image(ctx, texObj, target, level);

    if (via_try_3d_copy(ctx, xoffset, yoffset,
			x, y, width, height,
			containerOf(texImage, struct via_texture_image,
				    image)))
    {
	struct via_texture_object *viaObj =
	    containerOf(texObj, struct via_texture_object, obj);
	viaObj->imagesInVRAM = GL_TRUE;
	return;
    }

    /*
     * Mapping of readbuffer and _bound_ texture images are done
     * in the span routines.
     */

    _swrast_copy_texsubimage2d(ctx, target, level,
			       xoffset, yoffset, x, y, width, height);
}

static void
viaCopyTexImage2D(GLcontext * ctx, GLenum target, GLint level,
		  GLenum internalFormat,
		  GLint x, GLint y, GLsizei width, GLsizei height,
		  GLint border)
{

    struct gl_texture_unit *texUnit =
	&ctx->Texture.Unit[ctx->Texture.CurrentUnit];
    struct gl_texture_object *texObj =
	_mesa_select_tex_object(ctx, texUnit, target);
    struct gl_texture_image *texImage =
	_mesa_select_tex_image(ctx, texObj, target, level);

    if (border)
	goto out_sw;

    ctx->Driver.TexImage2D(ctx, target, level, internalFormat,
			   width, height, border,
			   GL_RGBA, CHAN_TYPE, NULL,
			   &ctx->DefaultPacking, texObj, texImage);

    /*
     * FIXME: Check for errors?
     */

    viaCopyTexSubImage2D(ctx, target, level, 0, 0, x, y, width, height);
    return;
  out_sw:
    _swrast_copy_teximage2d(ctx, target, level, internalFormat, x, y,
			    width, height, border);
}

void
viaInitTextureFuncs(struct dd_function_table *functions)
{
    functions->ChooseTextureFormat = viaChooseTexFormat;
    functions->TexImage1D = viaTexImage1D;
    functions->TexImage2D = viaTexImage2D;
    functions->TexSubImage1D = viaTexSubImage1D;
    functions->TexSubImage2D = viaTexSubImage2D;
    functions->CompressedTexImage2D = viaCompressedTexImage;
    functions->CompressedTexSubImage2D = viaCompressedTexSubImage2D;
    functions->GetTexImage = viaGetTexImage;
    functions->GetCompressedTexImage = viaGetCompressedTexImage;
    functions->NewTextureObject = viaNewTextureObject;
    functions->NewTextureImage = viaNewTextureImage;
    functions->DeleteTexture = _mesa_delete_texture_object;
    functions->FreeTexImageData = viaFreeTextureImageData;

    functions->CopyTexImage2D = viaCopyTexImage2D;
    functions->CopyTexSubImage2D = viaCopyTexSubImage2D;

#if defined( USE_SSE_ASM )
    functions->TextureMemCpy = via_sse_memcpy;
#else
    functions->TextureMemCpy = _mesa_memcpy;
#endif

    functions->UpdateTexturePalette = 0;
    functions->IsTextureResident = viaIsTextureResident;
}

int
via_map_unmap_texunit(struct gl_texture_unit *tu, GLboolean map)
{
    struct gl_texture_object *tobj = tu->_Current;
    struct via_texture_object *vobj = (struct via_texture_object *)tobj;
    struct gl_texture_image *image;
    struct via_texture_image *vImage;
    int i;
    int j;
    int jMax;
    int ret;

    if (!tu->_ReallyEnabled)
	return 0;

    jMax = (tobj == tu->CurrentCubeMap) ? 6 : 1;

    for (j = 0; j < jMax; ++j) {
	for (i = vobj->firstLevel; i <= vobj->lastLevel; ++i) {
	    image = tobj->Image[j][i];

	    if (image == NULL)
		continue;

	    if (!image->DriverData)
		continue;

	    vImage = containerOf(image, struct via_texture_image, image);

	    if (vImage->buf) {
		if (map) {
		    image->Data = wsbmBOMap(vImage->buf,
					    WSBM_ACCESS_READ |
					    WSBM_ACCESS_WRITE);
		    if (!image->Data) {
			return -ENOMEM;
		    }

		    ret = wsbmBOSyncForCpu(vImage->buf,
					   WSBM_SYNCCPU_READ |
					   WSBM_SYNCCPU_WRITE);
		    if (ret) {
			wsbmBOUnmap(vImage->buf);
			image->Data = NULL;
			return -ENOMEM;
		    }

		} else {
		    if (image->Data) {
			wsbmBOReleaseFromCpu(vImage->buf,
					     WSBM_SYNCCPU_READ |
					     WSBM_SYNCCPU_WRITE);
			wsbmBOUnmap(vImage->buf);
			image->Data = NULL;
		    }
		}
	    }
	}
    }
    return 0;
}
