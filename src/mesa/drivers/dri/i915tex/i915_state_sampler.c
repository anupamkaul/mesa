/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "mtypes.h"
#include "enums.h"
#include "texformat.h"
#include "dri_bufmgr.h"

#include "intel_mipmap_tree.h"
#include "intel_tex.h"

#include "i915_context.h"
#include "i915_reg.h"


static GLuint
translate_texture_format(GLuint mesa_format)
{
   switch (mesa_format) {
   case MESA_FORMAT_L8:
      return MAPSURF_8BIT | MT_8BIT_L8;
   case MESA_FORMAT_I8:
      return MAPSURF_8BIT | MT_8BIT_I8;
   case MESA_FORMAT_A8:
      return MAPSURF_8BIT | MT_8BIT_A8;
   case MESA_FORMAT_AL88:
      return MAPSURF_16BIT | MT_16BIT_AY88;
   case MESA_FORMAT_RGB565:
      return MAPSURF_16BIT | MT_16BIT_RGB565;
   case MESA_FORMAT_ARGB1555:
      return MAPSURF_16BIT | MT_16BIT_ARGB1555;
   case MESA_FORMAT_ARGB4444:
      return MAPSURF_16BIT | MT_16BIT_ARGB4444;
   case MESA_FORMAT_ARGB8888:
      return MAPSURF_32BIT | MT_32BIT_ARGB8888;
   case MESA_FORMAT_YCBCR_REV:
      return (MAPSURF_422 | MT_422_YCRCB_NORMAL);
   case MESA_FORMAT_YCBCR:
      return (MAPSURF_422 | MT_422_YCRCB_SWAPY);
   case MESA_FORMAT_RGB_FXT1:
   case MESA_FORMAT_RGBA_FXT1:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_FXT1);
   case MESA_FORMAT_Z16:
      return (MAPSURF_16BIT | MT_16BIT_L16);
   case MESA_FORMAT_RGBA_DXT1:
   case MESA_FORMAT_RGB_DXT1:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT1);
   case MESA_FORMAT_RGBA_DXT3:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT2_3);
   case MESA_FORMAT_RGBA_DXT5:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT4_5);
   case MESA_FORMAT_Z24_S8:
      return (MAPSURF_32BIT | MT_32BIT_xL824);
   default:
      fprintf(stderr, "%s: bad image format %x\n", __FUNCTION__, mesa_format);
      abort();
      return 0;
   }
}




/* The i915 (and related graphics cores) do not support GL_CLAMP.  The
 * Intel drivers for "other operating systems" implement GL_CLAMP as
 * GL_CLAMP_TO_EDGE, so the same is done here.
 */
static GLuint
translate_wrap_mode(GLenum wrap)
{
   switch (wrap) {
   case GL_REPEAT:
      return TEXCOORDMODE_WRAP;
   case GL_CLAMP:
      return TEXCOORDMODE_CLAMP_EDGE;   /* not quite correct */
   case GL_CLAMP_TO_EDGE:
      return TEXCOORDMODE_CLAMP_EDGE;
   case GL_CLAMP_TO_BORDER:
      return TEXCOORDMODE_CLAMP_BORDER;
   case GL_MIRRORED_REPEAT:
      return TEXCOORDMODE_MIRROR;
   default:
      return TEXCOORDMODE_WRAP;
   }
}



/* Recalculate all state from scratch.  Perhaps not the most
 * efficient, but this has gotten complex enough that we need
 * something which is understandable and reliable.
 */
static GLboolean
i915_update_sampler(struct intel_context *intel, GLuint unit)
{
   GLcontext *ctx = &intel->ctx;
   struct i915_context *i915 = i915_context(ctx);
   struct gl_texture_unit *tUnit = &ctx->Texture.Unit[unit];
   struct gl_texture_object *tObj = tUnit->_Current;
   struct intel_texture_object *intelObj = intel_texture_object(tObj);
   struct gl_texture_image *firstImage;
   GLuint *state = i915->state.Tex[unit];

   memset(state, 0, sizeof(state));
   

   /* Get first image here, since intelObj->firstLevel will get set in
    * the intel_finalize_mipmap_tree() call above.
    */
   firstImage = tObj->Image[0][intelObj->firstLevel];


   {
      GLuint minFilt, mipFilt, magFilt;

      switch (tObj->MinFilter) {
      case GL_NEAREST:
         minFilt = FILTER_NEAREST;
         mipFilt = MIPFILTER_NONE;
         break;
      case GL_LINEAR:
         minFilt = FILTER_LINEAR;
         mipFilt = MIPFILTER_NONE;
         break;
      case GL_NEAREST_MIPMAP_NEAREST:
         minFilt = FILTER_NEAREST;
         mipFilt = MIPFILTER_NEAREST;
         break;
      case GL_LINEAR_MIPMAP_NEAREST:
         minFilt = FILTER_LINEAR;
         mipFilt = MIPFILTER_NEAREST;
         break;
      case GL_NEAREST_MIPMAP_LINEAR:
         minFilt = FILTER_NEAREST;
         mipFilt = MIPFILTER_LINEAR;
         break;
      case GL_LINEAR_MIPMAP_LINEAR:
         minFilt = FILTER_LINEAR;
         mipFilt = MIPFILTER_LINEAR;
         break;
      default:
	 minFilt = FILTER_NEAREST;
	 mipFilt = MIPFILTER_NONE;
      }

      if (tObj->MaxAnisotropy > 1.0) {
         minFilt = FILTER_ANISOTROPIC;
         magFilt = FILTER_ANISOTROPIC;
      }
      else {
         switch (tObj->MagFilter) {
         case GL_NEAREST:
            magFilt = FILTER_NEAREST;
            break;
         case GL_LINEAR:
            magFilt = FILTER_LINEAR;
            break;
         default:
	    magFilt = FILTER_NEAREST;
	    break;
         }
      }

      {
         GLint b = (int) ((tObj->LodBias + tUnit->LodBias) * 16.0);
	 b = CLAMP(b, -256, 255);

	 state[I915_TEXREG_SS2] |= ((b << SS2_LOD_BIAS_SHIFT) & SS2_LOD_BIAS_MASK);
      }


      /* YUV conversion:
       */
      if (firstImage->TexFormat->MesaFormat == MESA_FORMAT_YCBCR ||
          firstImage->TexFormat->MesaFormat == MESA_FORMAT_YCBCR_REV)
         state[I915_TEXREG_SS2] |= SS2_COLORSPACE_CONVERSION;

      /* Shadow:
       */
      if (tObj->CompareMode == GL_COMPARE_R_TO_TEXTURE_ARB &&
          tObj->Target != GL_TEXTURE_3D) {

         state[I915_TEXREG_SS2] |=
            (SS2_SHADOW_ENABLE |
             intel_translate_compare_func(tObj->CompareFunc));

         minFilt = FILTER_4X4_FLAT;
         magFilt = FILTER_4X4_FLAT;
      }

      state[I915_TEXREG_SS2] |= ((minFilt << SS2_MIN_FILTER_SHIFT) |
                                 (mipFilt << SS2_MIP_FILTER_SHIFT) |
                                 (magFilt << SS2_MAG_FILTER_SHIFT));
   }

   {
      GLenum ws = tObj->WrapS;
      GLenum wt = tObj->WrapT;
      GLenum wr = tObj->WrapR;


      /* 3D textures don't seem to respect the border color.
       * Fallback if there's ever a danger that they might refer to
       * it.  
       * 
       * Effectively this means fallback on 3D clamp or
       * clamp_to_border.
       */
      if (tObj->Target == GL_TEXTURE_3D &&
          (tObj->MinFilter != GL_NEAREST ||
           tObj->MagFilter != GL_NEAREST) &&
          (ws == GL_CLAMP ||
           wt == GL_CLAMP ||
           wr == GL_CLAMP ||
           ws == GL_CLAMP_TO_BORDER ||
           wt == GL_CLAMP_TO_BORDER || 
	   wr == GL_CLAMP_TO_BORDER)) {
	 
/*          return GL_FALSE; */
	 sampler->fallback = true;
      }


      /* Or some field in tObj? */
      if (ctx->Texture.Unit[i]._ReallyEnabled == TEXTURE_RECT_BIT)
	 ss3 = SS3_NORMALIZED_COORDS;
      else
	 ss3 = 0;


      state[I915_TEXREG_SS3] = ss3;     /* SS3_NORMALIZED_COORDS */

      state[I915_TEXREG_SS3] |=
         ((translate_wrap_mode(ws) << SS3_TCX_ADDR_MODE_SHIFT) |
          (translate_wrap_mode(wt) << SS3_TCY_ADDR_MODE_SHIFT) |
          (translate_wrap_mode(wr) << SS3_TCZ_ADDR_MODE_SHIFT));

      state[I915_TEXREG_SS3] |= (unit << SS3_TEXTUREMAP_INDEX_SHIFT);
   }


   state[I915_TEXREG_SS4] = INTEL_PACKCOLOR8888(tObj->_BorderChan[0],
                                                tObj->_BorderChan[1],
                                                tObj->_BorderChan[2],
                                                tObj->_BorderChan[3]);


   
   I915_ACTIVESTATE(i915, I915_UPLOAD_TEX(unit), GL_TRUE);
   /* memcmp was already disabled, but definitely won't work as the
    * region might now change and that wouldn't be detected:
    */
   I915_STATECHANGE(i915, I915_UPLOAD_TEX(unit));


   DBG(TEXTURE, "state[I915_TEXREG_SS2] = 0x%x\n", state[I915_TEXREG_SS2]);
   DBG(TEXTURE, "state[I915_TEXREG_SS3] = 0x%x\n", state[I915_TEXREG_SS3]);
   DBG(TEXTURE, "state[I915_TEXREG_SS4] = 0x%x\n", state[I915_TEXREG_SS4]);

   return GL_TRUE;
}






void
i915_tex_state_emit()
{
      int nr = 0;

      for (i = 0; i < I915_TEX_UNITS; i++)
         if (dirty & I915_UPLOAD_TEX(i))
            nr++;

      for (i = 0; i < I915_TEX_UNITS; i++)
	 update_sampler( i915, i );


      BEGIN_BATCH(2 + nr * 3, 0);
      OUT_BATCH(_3DSTATE_SAMPLER_STATE | (3 * nr));
      OUT_BATCH((dirty & I915_UPLOAD_TEX_ALL) >> I915_UPLOAD_TEX_0_SHIFT);
      for (i = 0; i < I915_TEX_UNITS; i++)
         if (dirty & I915_UPLOAD_TEX(i)) {
            OUT_BATCH(state->Tex[i][I915_TEXREG_SS2]);
            OUT_BATCH(state->Tex[i][I915_TEXREG_SS3]);
            OUT_BATCH(state->Tex[i][I915_TEXREG_SS4]);
         }
      ADVANCE_BATCH();
}

const struct i915_tracked_state i915_upload_samplers = {
   .dirty = {
      .mesa = (_NEW_TEXTURE),
      .intel = 0,
      .indirect = 0
   },
   .update = upload_samplers
};
