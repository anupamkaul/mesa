/* $XFree86: xc/lib/GL/mesa/src/drv/mga/mgatexcnv.c,v 1.3 2002/10/30 12:51:36 alanh Exp $ */
/*
 * Copyright 2000-2001 VA Linux Systems, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include <stdlib.h>
#include <stdio.h>

#include <GL/gl.h>

#include "mm.h"
#include "mgacontext.h"
#include "mgatex.h"


/*
 * mgaConvertTexture
 * Converts a mesa format texture to the apropriate hardware format
 * Note that sometimes width may be larger than the texture, like 64x1
 * for an 8x8 texture.  This happens when we have to crutch the pitch
 * limits of the mga by uploading a block of texels as a single line.
 */
void mgaConvertTexture( GLubyte *dst, int texelBytes,
			struct gl_texture_image *image,
			int x, int y, int width, int height )
{
   int      i;
   GLubyte *src;
   int      src_pitch;
   int      dst_pitch;

   if (0)
      fprintf(stderr, "texture image %p\n", image->Data);

   if (image->Data == 0)
      return;

   src = (GLubyte *)image->Data + (y * image->RowStride + x) * texelBytes;
   
   src_pitch = image->RowStride * texelBytes;
   dst_pitch = width * texelBytes;
   
   for (i = height; i; i--) {
      memcpy( dst, src, dst_pitch );
      dst += dst_pitch;
      src += src_pitch;
   }
}
