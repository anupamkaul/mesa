/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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

#include "glheader.h"
#include "macros.h"
#include "mtypes.h"
#include "simple_list.h"
#include "enums.h"
#include "convolve.h"
#include "context.h"
#include "texcompress.h"
#include "texformat.h"
#include "texobj.h"
#include "texstore.h"

#include "mm.h"
#include "via_context.h"
#include "via_fb.h"
#include "via_tex.h"
#include "via_state.h"
#include "via_ioctl.h"
#include "via_3d_reg.h"

static const struct gl_texture_format *
viaChooseTexFormat( GLcontext *ctx, GLint internalFormat,
		    GLenum format, GLenum type )
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);
   const GLboolean do32bpt = ( vmesa->viaScreen->bitsPerPixel == 32
/* 			       && vmesa->viaScreen->textureSize > 4*1024*1024 */
      );

/*    fprintf(stderr, "do32bpt: %d bpp %d textureSize %d\n", do32bpt, */
/* 	   vmesa->viaScreen->bitsPerPixel, */
/* 	   vmesa->viaScreen->textureSize); */

   switch ( internalFormat ) {
   case 4:
   case GL_RGBA:
   case GL_COMPRESSED_RGBA:
      if ( format == GL_BGRA ) {
	 if ( type == GL_UNSIGNED_INT_8_8_8_8_REV ) {
	    return &_mesa_texformat_argb8888;
	 }
         else if ( type == GL_UNSIGNED_SHORT_4_4_4_4_REV ) {
            return &_mesa_texformat_argb4444;
	 }
         else if ( type == GL_UNSIGNED_SHORT_1_5_5_5_REV ) {
	    return &_mesa_texformat_argb1555;
	 }
      }
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_argb4444;

   case 3:
   case GL_RGB:
   case GL_COMPRESSED_RGB:
      if ( format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 ) {
	 return &_mesa_texformat_rgb565;
      }
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_rgb565;

   case GL_RGBA8:
   case GL_RGB10_A2:
   case GL_RGBA12:
   case GL_RGBA16:
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_argb4444;

   case GL_RGBA4:
   case GL_RGBA2:
      return &_mesa_texformat_argb4444;

   case GL_RGB5_A1:
      return &_mesa_texformat_argb1555;

   case GL_RGB8:
   case GL_RGB10:
   case GL_RGB12:
   case GL_RGB16:
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_rgb565;

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
      if (type == GL_UNSIGNED_SHORT_8_8_MESA ||
	  type == GL_UNSIGNED_BYTE)
         return &_mesa_texformat_ycbcr;
      else
         return &_mesa_texformat_ycbcr_rev;

   case GL_COMPRESSED_RGB_FXT1_3DFX:
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

   case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
      return &_mesa_texformat_rgba_dxt5;

   case GL_DEPTH_COMPONENT:
   case GL_DEPTH_COMPONENT16:
   case GL_DEPTH_COMPONENT24:
   case GL_DEPTH_COMPONENT32:
      return &_mesa_texformat_depth_component16;
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
	      _mesa_lookup_enum_by_nr(internalFormat),
	      __FUNCTION__);
      return NULL;
   }

   return NULL; /* never get here */
}

static int logbase2(int n)
{
   GLint i = 1;
   GLint log2 = 0;

   if (n < 0) {
      return -1;
   }

   while (n > i) {
      i *= 2;
      log2++;
   }

   if (i != n) {
      return -1;
   }
   else {
      return log2;
   }
}


/* Basically, just collect the image dimensions and addresses for each
 * image and update the texture object state accordingly.
 */
static GLboolean viaSetTexImages(viaContextPtr vmesa,
				 struct gl_texture_object *tObj)
{
   struct via_texture_object *t = tObj->DriverData;
   const struct gl_texture_image *baseImage = tObj->Image[0][tObj->BaseLevel];
   GLint firstLevel, lastLevel, numLevels;
   GLuint texFormat;
   GLint w, h, p;
   GLint i, j = 0, k = 0, l = 0, m = 0;
   GLuint texBase;
   GLuint basH = 0;
   GLuint widthExp = 0;
   GLuint heightExp = 0;    

   switch (baseImage->TexFormat->MesaFormat) {
   case MESA_FORMAT_ARGB8888:
      texFormat = HC_HTXnFM_ARGB8888;
      break;
   case MESA_FORMAT_ARGB4444:
      texFormat = HC_HTXnFM_ARGB4444; 
      break;
   case MESA_FORMAT_RGB565:
      texFormat = HC_HTXnFM_RGB565;   
      break;
   case MESA_FORMAT_ARGB1555:
      texFormat = HC_HTXnFM_ARGB1555;   
      break;
   case MESA_FORMAT_RGB888:
      texFormat = HC_HTXnFM_ARGB0888;
      break;
   case MESA_FORMAT_L8:
      texFormat = HC_HTXnFM_L8;       
      break;
   case MESA_FORMAT_I8:
      texFormat = HC_HTXnFM_T8;       
      break;
   case MESA_FORMAT_CI8:
      texFormat = HC_HTXnFM_Index8;   
      break;
   case MESA_FORMAT_AL88:
      texFormat = HC_HTXnFM_AL88;     
      break;
   case MESA_FORMAT_A8:
      texFormat = HC_HTXnFM_A8;     
      break;
   default:
      _mesa_problem(vmesa->glCtx, "Bad texture format in viaSetTexImages");
      return GL_FALSE;
   }

   /* Compute which mipmap levels we really want to send to the hardware.
    * This depends on the base image size, GL_TEXTURE_MIN_LOD,
    * GL_TEXTURE_MAX_LOD, GL_TEXTURE_BASE_LEVEL, and GL_TEXTURE_MAX_LEVEL.
    * Yes, this looks overly complicated, but it's all needed.
    */
   if (tObj->MinFilter == GL_LINEAR || tObj->MinFilter == GL_NEAREST) {
      firstLevel = lastLevel = tObj->BaseLevel;
   }
   else {
      firstLevel = tObj->BaseLevel + (GLint)(tObj->MinLod + 0.5);
      firstLevel = MAX2(firstLevel, tObj->BaseLevel);
      lastLevel = tObj->BaseLevel + (GLint)(tObj->MaxLod + 0.5);
      lastLevel = MAX2(lastLevel, tObj->BaseLevel);
      lastLevel = MIN2(lastLevel, tObj->BaseLevel + baseImage->MaxLog2);
      lastLevel = MIN2(lastLevel, tObj->MaxLevel);
      lastLevel = MAX2(firstLevel, lastLevel);        /* need at least one level */
   }

   /* save these values */
   t->firstLevel = firstLevel;
   t->lastLevel = lastLevel;
   
   numLevels = lastLevel - firstLevel + 1;
    
   if (t->memType == VIA_MEM_AGP)
      t->regTexFM = (HC_SubA_HTXnFM << 24) | HC_HTXnLoc_AGP | texFormat;
   else
      t->regTexFM = (HC_SubA_HTXnFM << 24) | HC_HTXnLoc_Local | texFormat;
        

   for (i = 0; i < numLevels; i++) {    
      if (!t->image[i]) {
	 if (VIA_DEBUG & DEBUG_TEXTURE)
	    fprintf(stderr, "%s: no image[%d]\n", __FUNCTION__, i);	 
	 return GL_FALSE;
      }
	    
      w = t->image[i]->image->WidthLog2;
      h = t->image[i]->image->HeightLog2;
      p = t->image[i]->pitchLog2;

      texBase = t->image[i]->texMem.texBase;
      if (!texBase) {
	 if (VIA_DEBUG & DEBUG_TEXTURE)
	    fprintf(stderr, "%s: no texBase[%d]\n", __FUNCTION__, i);	 
	 return GL_FALSE;
      }

      t->regTexBaseAndPitch[i].baseL = (((HC_SubA_HTXnL0BasL + i) << 24) | 
					(texBase & 0xFFFFFF));

      t->regTexBaseAndPitch[i].pitchLog2 = (((HC_SubA_HTXnL0Pit + i) << 24) |
					    (p << 20));
					      
					      
      /* The base high bytes for each 3 levels are packed
       * together into one register:
       */
      j = i / 3;
      k = 3 - (i % 3);                                              
      basH |= ((texBase & 0xFF000000) >> (k << 3));
      if (k == 1) {
	 t->regTexBaseH[j] = ((j + HC_SubA_HTXnL012BasH) << 24) | basH;
	 basH = 0;
      }
            
      /* Likewise, sets of 6 log2width and log2height values are
       * packed into individual registers:
       */
      l = i / 6;
      m = i % 6;
      widthExp |= (((GLuint)w & 0xF) << (m << 2));
      heightExp |= (((GLuint)h & 0xF) << (m << 2));
      if (m == 5) {
	 t->regTexWidthLog2[l] = ((l + HC_SubA_HTXnL0_5WE) << 24 | widthExp);
	 t->regTexHeightLog2[l] = ((l + HC_SubA_HTXnL0_5HE) << 24 | heightExp);  
	 widthExp = 0;
	 heightExp = 0;
      }
      if (w) w--;
      if (h) h--;
      if (p) p--;                                           
   }
        
   if (k != 1) {
      t->regTexBaseH[j] = ((j + HC_SubA_HTXnL012BasH) << 24) | basH;      
   }
   if (m != 5) {
      t->regTexWidthLog2[l] = ((l + HC_SubA_HTXnL0_5WE) << 24 | widthExp);
      t->regTexHeightLog2[l] = ((l + HC_SubA_HTXnL0_5HE) << 24 | heightExp);
   }

   return GL_TRUE;
}


static GLboolean viaUpdateTexUnit(GLcontext *ctx, GLuint unit)
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

   if (texUnit->_ReallyEnabled == TEXTURE_2D_BIT || 
       texUnit->_ReallyEnabled == TEXTURE_1D_BIT) {

      struct gl_texture_object *tObj = texUnit->_Current;
      struct via_texture_object *t = tObj->DriverData;

      /* Upload teximages (not pipelined)
       */
      if (t->dirtyImages) {
	 if (!viaSetTexImages(vmesa, tObj)) {
	    if (VIA_DEBUG & DEBUG_TEXTURE)
	       fprintf(stderr, "viaSetTexImages failed for unit %d\n", unit);
	    return GL_FALSE;
	 }
      }

      /* Update state if this is a different texture object to last
       * time.
       */
      if (vmesa->CurrentTexObj[unit] != t) {
	 VIA_FLUSH_DMA(vmesa);
	 vmesa->CurrentTexObj[unit] = t;
      }

      return GL_TRUE;
   }
   else if (texUnit->_ReallyEnabled) {
      fprintf(stderr, "bad _ReallyEnabled\n");
      return GL_FALSE;
   } 
   else {
      vmesa->CurrentTexObj[unit] = 0;
      VIA_FLUSH_DMA(vmesa);
      return GL_TRUE;
   }
}
 
GLboolean viaUpdateTextureState(GLcontext *ctx)
{
   return (viaUpdateTexUnit(ctx, 0) &&
	   viaUpdateTexUnit(ctx, 1));
}




/* TODO: Add code to read a texture back from fb or agp memory (should
 * just be a memcpy) thus freeing up texture spce.
 *
 * TODO: Do this with hardware.
 *
 * TODO: Do this in the background.
 */



static struct via_texture_object *viaAllocTextureObject(struct gl_texture_object *texObj)
{
   struct via_texture_object *t = CALLOC_STRUCT(via_texture_object);
   if (!t)
      return NULL;

   t->dirtyImages = ~0;
   t->memType = VIA_MEM_UNKNOWN;

   make_empty_list(t);

   texObj->DriverData = (void *)t;
   return t;
}


static struct via_texture_image *viaAllocTextureImage(struct gl_texture_image *texImage)
{
   struct via_texture_image *t = CALLOC_STRUCT(via_texture_image);

   if (!t)
      return NULL;

   t->image = texImage;
   texImage->DriverData = (void *)t;

   return t;
}

static void choose_texture_heap( GLcontext *ctx,
				 struct via_texture_object *t,
				 GLint level,
				 GLint width,
				 GLint height )
{
   t->memType = VIA_MEM_AGP; 

   /* Problems with VIA_MEM_VIDEO at the moment.  Don't know if we
    * ever properly tested it in the old setup:
    */
/*    t->memType = VIA_MEM_VIDEO;  */
}
				 


static void viaTexImage(GLcontext *ctx, 
			GLint dims,
			GLenum target, GLint level,
			GLint internalFormat,
			GLint width, GLint height, GLint border,
			GLenum format, GLenum type, const void *pixels,
			const struct gl_pixelstore_attrib *packing,
			struct gl_texture_object *texObj,
			struct gl_texture_image *texImage)
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);
   GLint postConvWidth = width;
   GLint postConvHeight = height;
   GLint texelBytes, sizeInBytes;
   struct via_texture_object *t = texObj->DriverData;
   struct via_texture_image *image = texImage->DriverData;

   /* Retreive or allocate driver private data for this
    * gl_texture_image:
    */    
   if (!t) 
      t = viaAllocTextureObject( texObj );  

   if (!image) 
      image = viaAllocTextureImage( texImage );

   t->image[level] = image;

   if (t->memType == VIA_MEM_UNKNOWN) {
      choose_texture_heap(ctx, t, level, width, height);
   }

   if (ctx->_ImageTransferState & IMAGE_CONVOLUTION_BIT) {
      _mesa_adjust_image_for_convolution(ctx, dims, &postConvWidth,
                                         &postConvHeight);
   }

   /* choose the texture format */
   texImage->TexFormat = viaChooseTexFormat(ctx, internalFormat, 
					    format, type);

   assert(texImage->TexFormat);

   if (dims == 1) {
      texImage->FetchTexelc = texImage->TexFormat->FetchTexel1D;
      texImage->FetchTexelf = texImage->TexFormat->FetchTexel1Df;
   }
   else {
      texImage->FetchTexelc = texImage->TexFormat->FetchTexel2D;
      texImage->FetchTexelf = texImage->TexFormat->FetchTexel2Df;
   }
   texelBytes = texImage->TexFormat->TexelBytes;


   /* Minimum pitch of 32 bytes */
   if (postConvWidth * texelBytes < 32) {
      postConvWidth = 32 / texelBytes;
      texImage->RowStride = postConvWidth;
   }

   assert(texImage->RowStride == postConvWidth);
   image->pitchLog2 = logbase2(postConvWidth * texelBytes);

   /* allocate memory */
   if (texImage->IsCompressed)
      sizeInBytes = texImage->CompressedSize;
   else
      sizeInBytes = postConvWidth * postConvHeight * texelBytes;


   /* Attempt to allocate texture memory directly, otherwise use main
    * memory and this texture will always be a fallback.   FIXME!
    *
    * TODO: make room in agp if this fails.
    * TODO: use fb ram for textures as well.
    */
   image->texMem.size = sizeInBytes;
   image->texMem.memType = VIA_MEM_AGP;

   if (via_alloc_texture(vmesa, &image->texMem)) {
      texImage->Data = image->texMem.bufAddr;
      texImage->IsClientData = GL_TRUE;	/* not really, but it'll work for now */
   }
   else {
      fprintf(stderr, "use system memory\n");
      /* No room, use system memory: */
      texImage->Data = MESA_PBUFFER_ALLOC(sizeInBytes);
      if (!texImage->Data) {
	 _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
	 return;
      }
   }

   vmesa->clearTexCache = 1;
   t->dirtyImages |= (1<<level);

   pixels = _mesa_validate_pbo_teximage(ctx, dims, width, height, 1, format, type,
					pixels, packing, "glTexImage");
   if (!pixels) {
      /* Note: we check for a NULL image pointer here, _after_ we allocated
       * memory for the texture.  That's what the GL spec calls for.
       */
      return;
   }
   else {
      GLint dstRowStride, dstImageStride = 0;
      GLboolean success;
      if (texImage->IsCompressed) {
         dstRowStride = _mesa_compressed_row_stride(texImage->IntFormat,width);
      }
      else {
         dstRowStride = postConvWidth * texImage->TexFormat->TexelBytes;
      }
      ASSERT(texImage->TexFormat->StoreImage);
      success = texImage->TexFormat->StoreImage(ctx, dims, texImage->Format,
                                                texImage->TexFormat,
                                                texImage->Data,
                                                0, 0, 0,  /* dstX/Y/Zoffset */
                                                dstRowStride, dstImageStride,
                                                width, height, 1,
                                                format, type, pixels, packing);
      if (!success) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
      }
   }

   /* GL_SGIS_generate_mipmap */
   if (level == texObj->BaseLevel && texObj->GenerateMipmap) {
      _mesa_generate_mipmap(ctx, target,
                            &ctx->Texture.Unit[ctx->Texture.CurrentUnit],
                            texObj);
   }

   _mesa_unmap_teximage_pbo(ctx, packing);
}

static void viaTexImage2D(GLcontext *ctx, 
			  GLenum target, GLint level,
			  GLint internalFormat,
			  GLint width, GLint height, GLint border,
			  GLenum format, GLenum type, const void *pixels,
			  const struct gl_pixelstore_attrib *packing,
			  struct gl_texture_object *texObj,
			  struct gl_texture_image *texImage)
{
   viaTexImage( ctx, 2, target, level, 
		internalFormat, width, height, border,
		format, type, pixels,
		packing, texObj, texImage );
}

static void viaTexSubImage2D(GLcontext *ctx,
                             GLenum target,
                             GLint level,
                             GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             const GLvoid *pixels,
                             const struct gl_pixelstore_attrib *packing,
                             struct gl_texture_object *texObj,
                             struct gl_texture_image *texImage)
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);
  
   VIA_FLUSH_DMA(vmesa);
   vmesa->clearTexCache = 1;

   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width,
			     height, format, type, pixels, packing, texObj,
			     texImage);
}

static void viaTexImage1D(GLcontext *ctx, 
			  GLenum target, GLint level,
			  GLint internalFormat,
			  GLint width, GLint border,
			  GLenum format, GLenum type, const void *pixels,
			  const struct gl_pixelstore_attrib *packing,
			  struct gl_texture_object *texObj,
			  struct gl_texture_image *texImage)
{
   viaTexImage( ctx, 1, target, level, 
		internalFormat, width, 1, border,
		format, type, pixels,
		packing, texObj, texImage );
}

static void viaTexSubImage1D(GLcontext *ctx,
                             GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLsizei width,
                             GLenum format, GLenum type,
                             const GLvoid *pixels,
                             const struct gl_pixelstore_attrib *packing,
                             struct gl_texture_object *texObj,
                             struct gl_texture_image *texImage)
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);

   VIA_FLUSH_DMA(vmesa);
   vmesa->clearTexCache = 1;

   _mesa_store_texsubimage1d(ctx, target, level, xoffset, width,
			     format, type, pixels, packing, texObj,
			     texImage);
}


static void viaBindTexture(GLcontext *ctx, GLenum target,
                           struct gl_texture_object *texObj)
{
   if (target == GL_TEXTURE_2D) {
      struct via_texture_object *t = texObj->DriverData;

      if (!t) {

	 t = viaAllocTextureObject(texObj);
	 if (!t) {
	    _mesa_error(ctx, GL_OUT_OF_MEMORY, "viaBindTexture");
	    return;
	 }
	 texObj->DriverData = t;
      }
   }
}

static void viaDeleteTexture(GLcontext *ctx, struct gl_texture_object *texObj)
{
   viaContextPtr vmesa = VIA_CONTEXT(ctx);
   struct via_texture_object *t = texObj->DriverData;
   GLint i;

   if (t) {
      if (vmesa) {
	 if (vmesa->dma) {
	    VIA_FLUSH_DMA(vmesa);
	 }
	 
	 for (i = 0; i < VIA_MAX_TEXLEVELS; i++) {
	    if (t->image[i] && t->image[i]->texMem.index) {
	       via_free_texture(vmesa, &t->image[i]->texMem);
	    }
	 }

	 if (vmesa->CurrentTexObj[0] == t) 
	    vmesa->CurrentTexObj[0] = 0;

	 if (vmesa->CurrentTexObj[1] == t) 
	    vmesa->CurrentTexObj[1] = 0;

	 remove_from_list(t);
	 free(t);
      }

      texObj->DriverData = 0;
   }

   /* Free mipmap images and the texture object itself */
   _mesa_delete_texture_object(ctx, texObj);
}

static GLboolean viaIsTextureResident(GLcontext *ctx,
                                      struct gl_texture_object *texObj)
{
   struct via_texture_object *t = texObj->DriverData;
   GLuint i;
   
   if (!t) return GL_FALSE;

   for (i = 0; i < VIA_MAX_TEXLEVELS; i++) {
      if (t->image[i] && !t->image[i]->texMem.index) {
	 return GL_FALSE;
      }
   }

   return GL_TRUE;
}


void viaInitTextureFuncs(struct dd_function_table * functions)
{
   functions->ChooseTextureFormat = viaChooseTexFormat;
   functions->TexImage1D = viaTexImage1D;
   functions->TexImage2D = viaTexImage2D;
   functions->TexSubImage1D = viaTexSubImage1D;
   functions->TexSubImage2D = viaTexSubImage2D;

   functions->NewTextureObject = _mesa_new_texture_object;
   functions->BindTexture = viaBindTexture;
   functions->DeleteTexture = viaDeleteTexture;
   functions->UpdateTexturePalette = 0;
   functions->IsTextureResident = viaIsTextureResident;
}

void viaInitTextures(GLcontext *ctx)
{
   GLuint tmp = ctx->Texture.CurrentUnit;
   ctx->Texture.CurrentUnit = 0;
   viaBindTexture(ctx, GL_TEXTURE_1D, ctx->Texture.Unit[0].Current1D);
   viaBindTexture(ctx, GL_TEXTURE_2D, ctx->Texture.Unit[0].Current2D);
   ctx->Texture.CurrentUnit = 1;
   viaBindTexture(ctx, GL_TEXTURE_1D, ctx->Texture.Unit[1].Current1D);
   viaBindTexture(ctx, GL_TEXTURE_2D, ctx->Texture.Unit[1].Current2D);
   ctx->Texture.CurrentUnit = tmp;
}
