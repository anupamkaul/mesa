/*
 * Copyright (C) 2008 Nicolai Haehnle.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "r300_mipmap_tree.h"

#include <errno.h>
#include <unistd.h>

#include "simple_list.h"
#include "texcompress.h"
#include "texformat.h"

static GLuint r300_compressed_texture_size(GLcontext *ctx,
		GLsizei width, GLsizei height, GLsizei depth,
		GLuint mesaFormat)
{
	GLuint size = _mesa_compressed_texture_size(ctx, width, height, depth, mesaFormat);

	if (mesaFormat == MESA_FORMAT_RGB_DXT1 ||
	    mesaFormat == MESA_FORMAT_RGBA_DXT1) {
		if (width + 3 < 8)	/* width one block */
			size = size * 4;
		else if (width + 3 < 16)
			size = size * 2;
	} else {
		/* DXT3/5, 16 bytes per block */
		WARN_ONCE("DXT 3/5 suffers from multitexturing problems!\n");
		if (width + 3 < 8)
			size = size * 2;
	}

	return size;
}

/**
 * Compute sizes and fill in offset and blit information for the given
 * image (determined by \p face and \p level).
 *
 * \param curOffset points to the offset at which the image is to be stored
 * and is updated by this function according to the size of the image.
 */
static void compute_tex_image_offset(r300_mipmap_tree *mt,
	GLuint face, GLuint level, GLuint* curOffset)
{
	r300_mipmap_level *lvl = &mt->levels[level];

	/* Find image size in bytes */
	if (mt->compressed) {
		lvl->size = r300_compressed_texture_size(mt->r300->radeon.glCtx,
			lvl->width, lvl->height, lvl->depth, mt->compressed);
	} else if (mt->target == GL_TEXTURE_RECTANGLE_NV) {
		lvl->size = ((lvl->width * mt->bpp + 63) & ~63) * lvl->height;
	} else if (mt->tilebits & R300_TXO_MICRO_TILE) {
		/* tile pattern is 16 bytes x2. mipmaps stay 32 byte aligned,
		 * though the actual offset may be different (if texture is less than
		 * 32 bytes width) to the untiled case */
		int w = (lvl->width * mt->bpp * 2 + 31) & ~31;
		lvl->size = (w * ((lvl->height + 1) / 2)) * lvl->depth;
	} else {
		int w = (lvl->width * mt->bpp + 31) & ~31;
		lvl->size = w * lvl->height * lvl->depth;
	}
	assert(lvl->size > 0);

	/* All images are aligned to a 32-byte offset */
	*curOffset = (*curOffset + 0x1f) & ~0x1f;
	lvl->faces[face].offset = *curOffset;
	*curOffset += lvl->size;
}

static GLuint minify(GLuint size, GLuint levels)
{
	size = size >> levels;
	if (size < 1)
		size = 1;
	return size;
}

static void calculate_miptree_layout(r300_mipmap_tree *mt)
{
	GLuint curOffset;
	GLuint numLevels;
	GLuint i;

	numLevels = mt->lastLevel - mt->firstLevel + 1;
	assert(numLevels <= RADEON_MAX_TEXTURE_LEVELS);

	curOffset = 0;
	for(i = 0; i < numLevels; i++) {
		GLuint face;

		mt->levels[i].width = minify(mt->width0, mt->firstLevel + i);
		mt->levels[i].height = minify(mt->height0, mt->firstLevel + i);
		mt->levels[i].depth = minify(mt->depth0, mt->firstLevel + i);

		for(face = 0; face < mt->faces; face++)
			compute_tex_image_offset(mt, face, i, &curOffset);
	}

	/* Note the required size in memory */
	mt->base.totalSize = (curOffset + RADEON_OFFSET_MASK) & ~RADEON_OFFSET_MASK;
}


/**
 * Create a new mipmap tree, calculate its layout and allocate memory.
 */
r300_mipmap_tree* r300_miptree_create(r300ContextPtr rmesa, r300TexObj *t,
		GLenum target, GLuint firstLevel, GLuint lastLevel,
		GLuint width0, GLuint height0, GLuint depth0,
		GLuint bpp, GLuint tilebits, GLuint compressed)
{
	r300_mipmap_tree *mt = CALLOC_STRUCT(_r300_mipmap_tree);

	make_empty_list(&mt->base);
	mt->r300 = rmesa;
	mt->t = t;
	mt->target = target;
	mt->faces = (target == GL_TEXTURE_CUBE_MAP) ? 6 : 1;
	mt->firstLevel = firstLevel;
	mt->lastLevel = lastLevel;
	mt->width0 = width0;
	mt->height0 = height0;
	mt->depth0 = depth0;
	mt->bpp = bpp;
	mt->tilebits = tilebits;
	mt->compressed = compressed;

	calculate_miptree_layout(mt);

	return mt;
}

/**
 * Destroy the given mipmap tree.
 */
void r300_miptree_destroy(r300_mipmap_tree *mt)
{
	driDestroyTextureObject(&mt->base);
}

/** Callback function called by texmem.c */
void r300_miptree_destroy_callback(void* data, driTextureObject *t)
{
	r300_mipmap_tree *mt = (r300_mipmap_tree*)t;

	if (mt->t && mt->t->mt == mt)
		mt->t->mt = 0;
}

/**
 * Returns \c true if memory for this mipmap tree has been allocated.
 */
GLboolean r300_miptree_is_validated(r300_mipmap_tree *mt)
{
	if (!mt)
		return GL_FALSE;

	if (!mt->base.memBlock)
		return GL_FALSE;

	return GL_TRUE;
}

/**
 * Allocate memory for this mipmap tree.
 */
void r300_miptree_validate(r300_mipmap_tree *mt)
{
	if (mt->base.memBlock)
		return;

	driAllocateTexture(mt->r300->texture_heaps, mt->r300->nr_heaps, &mt->base);
}

GLuint r300_miptree_get_offset(r300_mipmap_tree *mt)
{
	if (!mt->base.memBlock)
		return 0;

	return mt->r300->radeon.radeonScreen->texOffset[0] + mt->base.memBlock->ofs;
}

/**
 * Upload the given texture image to the given face/level of the mipmap tree.
 * \param level of the texture, i.e. \c level==mt->firstLevel is the first hw level
 */
void r300_miptree_upload_image(r300_mipmap_tree *mt, GLuint face, GLuint level,
			       struct gl_texture_image *texImage)
{
	GLuint hwlevel = level - mt->firstLevel;
	r300_mipmap_level *lvl;
	drm_radeon_texture_t tex;
	drm_radeon_tex_image_t tmp;
	GLuint offset;
	int ret;

	assert(face < mt->faces);
	assert(level >= mt->firstLevel && level <= mt->lastLevel);
	assert(texImage && texImage->Data);

	if (!mt->base.memBlock)
		return;

	lvl = &mt->levels[hwlevel];
	assert(texImage->Width == lvl->width);
	assert(texImage->Height == lvl->height);
	assert(texImage->Depth == lvl->depth);

	offset = mt->r300->radeon.radeonScreen->texOffset[0] + mt->base.memBlock->ofs;

	tex.offset = offset;
	tex.image = &tmp;

	if (texImage->TexFormat->TexelBytes) {
		GLuint blitWidth;

		if (mt->target == GL_TEXTURE_RECTANGLE_NV) {
			blitWidth = 64 / texImage->TexFormat->TexelBytes;
		} else {
			blitWidth = MAX2(texImage->Width, 64 / texImage->TexFormat->TexelBytes);
		}

		tmp.x = lvl->faces[face].offset;
		tmp.y = 0;
		tmp.width = MIN2(lvl->size / texImage->TexFormat->TexelBytes, blitWidth);
		tmp.height = (lvl->size / texImage->TexFormat->TexelBytes) / tmp.width;
	} else {
		tmp.x = lvl->faces[face].offset % R300_BLIT_WIDTH_BYTES;
		tmp.y = lvl->faces[face].offset / R300_BLIT_WIDTH_BYTES;
		tmp.width = MIN2(lvl->size, R300_BLIT_WIDTH_BYTES);
		tmp.height = lvl->size / tmp.width;
	}
	tmp.data = texImage->Data;

	if (texImage->TexFormat->TexelBytes > 4) {
		const int log2TexelBytes =
		    (3 + (texImage->TexFormat->TexelBytes >> 4));
		tex.format = RADEON_TXFORMAT_I8;	/* any 1-byte texel format */
		tex.pitch =
		    MAX2((texImage->Width * texImage->TexFormat->TexelBytes) /
			 64, 1);
		tex.height = texImage->Height;
		tex.width = texImage->Width << log2TexelBytes;
		tex.offset += (tmp.x << log2TexelBytes) & ~1023;
		tmp.x = tmp.x % (1024 >> log2TexelBytes);
		tmp.width = tmp.width << log2TexelBytes;
	} else if (texImage->TexFormat->TexelBytes) {
		/* use multi-byte upload scheme */
		tex.height = texImage->Height;
		tex.width = texImage->Width;
		switch (texImage->TexFormat->TexelBytes) {
		case 1:
			tex.format = RADEON_TXFORMAT_I8;
			break;
		case 2:
			tex.format = RADEON_TXFORMAT_AI88;
			break;
		case 4:
			tex.format = RADEON_TXFORMAT_ARGB8888;
			break;
		}
		tex.pitch =
		    MAX2((texImage->Width * texImage->TexFormat->TexelBytes) /
			 64, 1);
		tex.offset += tmp.x & ~1023;
		tmp.x = tmp.x % 1024;

		if (mt->tilebits & R300_TXO_MICRO_TILE) {
			/* need something like "tiled coordinates" ? */
			tmp.y = tmp.x / (tex.pitch * 128) * 2;
			tmp.x =
			    tmp.x % (tex.pitch * 128) / 2 /
			    texImage->TexFormat->TexelBytes;
			tex.pitch |= RADEON_DST_TILE_MICRO >> 22;
		} else {
			tmp.x = tmp.x >> (texImage->TexFormat->TexelBytes >> 1);
		}

		if ((mt->tilebits & R300_TXO_MACRO_TILE) &&
		    (texImage->Width * texImage->TexFormat->TexelBytes >= 256)
		    && ((!(mt->tilebits & R300_TXO_MICRO_TILE)
			 && (texImage->Height >= 8))
			|| (texImage->Height >= 16))) {
			/* weird: R200 disables macro tiling if mip width is smaller than 256 bytes,
			   OR if height is smaller than 8 automatically, but if micro tiling is active
			   the limit is height 16 instead ? */
			tex.pitch |= RADEON_DST_TILE_MACRO >> 22;
		}
	} else {
		/* In case of for instance 8x8 texture (2x2 dxt blocks),
		   padding after the first two blocks is needed (only
		   with dxt1 since 2 dxt3/dxt5 blocks already use 32 Byte). */
		/* set tex.height to 1/4 since 1 "macropixel" (dxt-block)
		   has 4 real pixels. Needed so the kernel module reads
		   the right amount of data. */
		tex.format = RADEON_TXFORMAT_I8;	/* any 1-byte texel format */
		tex.pitch = (R300_BLIT_WIDTH_BYTES / 64);
		tex.height = (texImage->Height + 3) / 4;
		tex.width = (texImage->Width + 3) / 4;
		if (mt->compressed == MESA_FORMAT_RGB_DXT1 ||
		    mt->compressed == MESA_FORMAT_RGBA_DXT1) {
			tex.width *= 8;
		} else {
			tex.width *= 16;
		}
	}

	LOCK_HARDWARE(&mt->r300->radeon);
	do {
		ret =
		    drmCommandWriteRead(mt->r300->radeon.dri.fd,
					DRM_RADEON_TEXTURE, &tex,
					sizeof(drm_radeon_texture_t));
		if (ret) {
			if (RADEON_DEBUG & DEBUG_IOCTL)
				fprintf(stderr,
					"DRM_RADEON_TEXTURE:  again!\n");
			usleep(1);
		}
	} while (ret == -EAGAIN);
	UNLOCK_HARDWARE(&mt->r300->radeon);
}
