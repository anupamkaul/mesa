/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009  VMware, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/**
 * \file texmem.c
 * Texture memory-related functions.
 */


#include "mtypes.h"
#include "formats.h"
#include "imports.h"
#include "texmem.h"


/**
 * Allocate space for the given texture image.
 * This is a fallback called via ctx->Driver.AllocTexImageData().
 */
GLboolean
_mesa_alloc_texture_image_data(GLcontext *ctx, struct gl_texture_image *tImage)
{
   GLint bytes = _mesa_format_image_size(tImage->TexFormat, tImage->Width,
                                         tImage->Height, tImage->Depth);
   /* XXX store data on tImgae->DriverData */
   tImage->Map.Data = _mesa_align_malloc(bytes, 512);
   return tImage->Map.Data != NULL;
}


/**
 * Free texture image data.
 * This function is a fallback called via ctx->Driver.FreeTexImageData().
 *
 * \param texImage texture image.
 *
 * Free the texture image data if it's not marked as client data.
 */
void
_mesa_free_texture_image_data(GLcontext *ctx,
                              struct gl_texture_image *texImage)
{
   (void) ctx;

   if (texImage->Map.Data && !texImage->IsClientData) {
      /* free the old texture data */
      /* XXX store data on tImgae->DriverData */
      _mesa_align_free(texImage->Map.Data);
   }

   texImage->Map.Data = NULL;
}


/**
 * Default/fallback for ctx->Driver.MapTexture()
 * Map all images in a texture.
 */
void
_mesa_map_texture(GLcontext *ctx, struct gl_texture_object *tObj, GLenum mode)
{
   const GLuint numFaces = tObj->Target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
   GLuint face, level;

   for (face = 0; face < numFaces; face++) {
      for (level = tObj->BaseLevel; level <= tObj->MaxLevel; level++) {
         ctx->Driver.MapTextureImage(ctx, tObj, level, face, mode);
      }
   }
}


/**
 * Default/fallback for ctx->Driver.UnmapTexture()
 * Unmap all images in a texture.
 */
void
_mesa_unmap_texture(GLcontext *ctx, struct gl_texture_object *tObj)
{
   const GLuint numFaces = tObj->Target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
   GLuint face, level;

   for (face = 0; face < numFaces; face++) {
      for (level = tObj->BaseLevel; level <= tObj->MaxLevel; level++) {
         ctx->Driver.UnmapTextureImage(ctx, tObj, level, face);
      }
   }
}


/**
 * Default/fallback for ctx->Driver.MapTextureImage()
 * Map a single image in a texture.
 * This function should only be used by software drivers that store
 * the actual texture image data off the DriverData pointer.
 */
void
_mesa_map_texture_image(GLcontext *ctx, struct gl_texture_object *tObj,
                        GLuint level, GLuint face, GLenum mode)
{
   struct gl_texture_image *texImage = tObj->Image[face][level];
   texImage->Map.Data = texImage->DriverData;
   ASSERT(texImage->Map.RowStride > 0);
}


/**
 * Default/fallback for ctx->Driver.UnmapTextureImage()
 * Unmap a single image in a texture.
 */
void
_mesa_unmap_texture_image(GLcontext *ctx, struct gl_texture_object *tObj,
                          GLuint level, GLuint face)
{
   struct gl_texture_image *texImage = tObj->Image[face][level];
   texImage->Map.Data = NULL;
}



/**
 * Map all current texture object images.
 * Typically called prior to software rendering.
 */
void
_mesa_map_current_textures(GLcontext *ctx)
{
   GLuint u;

   if (!ctx->Texture._EnabledUnits) {
      /* no textures enabled, or no way to validate images! */
      return;
   }

   if (!ctx->Driver.MapTexture)
      return;

   for (u = 0; u < ctx->Const.MaxTextureImageUnits; u++) {
      if (ctx->Texture.Unit[u]._ReallyEnabled) {
         struct gl_texture_object *texObj = ctx->Texture.Unit[u]._Current;
         ASSERT(texObj);
         if (texObj) {
            /* Map read/write in case of render to texture */
            ctx->Driver.MapTexture(ctx, texObj, GL_READ_WRITE);
         }
      }
   }
}


/**
 * Unmap all current texture object images.
 * Typically called after software rendering is finished.
 */
void
_mesa_unmap_current_textures(GLcontext *ctx)
{
   GLuint u;

   if (!ctx->Texture._EnabledUnits) {
      /* no textures enabled */
      return;
   }

   if (!ctx->Driver.UnmapTexture)
      return;

   for (u = 0; u < ctx->Const.MaxTextureImageUnits; u++) {
      if (ctx->Texture.Unit[u]._ReallyEnabled) {
         struct gl_texture_object *texObj = ctx->Texture.Unit[u]._Current;
         ASSERT(texObj);
         if (texObj) {
            ctx->Driver.UnmapTexture(ctx, texObj);
         }
      }
   }
}

