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

#include "r300_mem.h"

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
	mt->totalsize = (curOffset + RADEON_OFFSET_MASK) & ~RADEON_OFFSET_MASK;
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

	mt->bo = dri_bo_alloc(&rmesa->bufmgr->base, "texture", mt->totalsize, 1024, 0);

	return mt;
}

/**
 * Destroy the given mipmap tree.
 */
void r300_miptree_destroy(r300_mipmap_tree *mt)
{
	dri_bo_unreference(mt->bo);
	free(mt);
}

/*
 * XXX Move this into core Mesa?
 */
static void
_mesa_copy_rect(GLubyte * dst,
                GLuint cpp,
                GLuint dst_pitch,
                GLuint dst_x,
                GLuint dst_y,
                GLuint width,
                GLuint height,
                const GLubyte * src,
                GLuint src_pitch, GLuint src_x, GLuint src_y)
{
   GLuint i;

   dst_pitch *= cpp;
   src_pitch *= cpp;
   dst += dst_x * cpp;
   src += src_x * cpp;
   dst += dst_y * dst_pitch;
   src += src_y * dst_pitch;
   width *= cpp;

   if (width == dst_pitch && width == src_pitch)
      memcpy(dst, src, height * width);
   else {
      for (i = 0; i < height; i++) {
         memcpy(dst, src, width);
         dst += dst_pitch;
         src += src_pitch;
      }
   }
}

/**
 * Upload the given texture image to the given face/level of the mipmap tree.
 * \param level of the texture, i.e. \c level==mt->firstLevel is the first hw level
 */
void r300_miptree_upload_image(r300_mipmap_tree *mt, GLuint face, GLuint level,
			       struct gl_texture_image *texImage)
{
	GLuint hwlevel = level - mt->firstLevel;
	r300_mipmap_level *lvl = &mt->levels[hwlevel];
	void *dest;

	assert(face < mt->faces);
	assert(level >= mt->firstLevel && level <= mt->lastLevel);
	assert(texImage && texImage->Data);
	assert(texImage->Width == lvl->width);
	assert(texImage->Height == lvl->height);
	assert(texImage->Depth == lvl->depth);

	dri_bo_map(mt->bo, GL_TRUE);

	dest = mt->bo->virtual + lvl->faces[face].offset;

	if (mt->tilebits)
		WARN_ONCE("%s: tiling not supported yet", __FUNCTION__);

	if (!mt->compressed) {
		GLuint dst_align;
		GLuint dst_pitch = lvl->width;
		GLuint src_pitch = lvl->width;

		if (mt->target == GL_TEXTURE_RECTANGLE_NV)
			dst_align = 64 / mt->bpp;
		else
			dst_align = 32 / mt->bpp;
		dst_pitch = (dst_pitch + dst_align - 1) & ~(dst_align - 1);

		_mesa_copy_rect(dest, mt->bpp, dst_pitch, 0, 0, lvl->width, lvl->height,
				texImage->Data, src_pitch, 0, 0);
	} else {
		memcpy(dest, texImage->Data, lvl->size);
	}

	dri_bo_unmap(mt->bo);
}
