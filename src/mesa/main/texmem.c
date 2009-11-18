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
