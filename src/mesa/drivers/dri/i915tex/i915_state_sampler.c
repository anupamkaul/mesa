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
#include "macros.h"
#include "dri_bufmgr.h"

#include "intel_mipmap_tree.h"
#include "intel_tex.h"
#include "intel_batchbuffer.h"
#include "intel_state_inlines.h"

#include "i915_context.h"
#include "i915_reg.h"






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
static void update_sampler(struct intel_context *intel, 
			   GLuint unit,
			   GLuint *state )
{
   struct gl_texture_unit *tUnit = &intel->state.Texture->Unit[unit];
   struct gl_texture_object *tObj = tUnit->_Current;
   struct intel_texture_object *intelObj = intel_texture_object(tObj);

   /* Need to do this after updating the maps, which call the
    * intel_finalize_mipmap_tree and hence can update firstLevel:
    */
   struct gl_texture_image *firstImage = tObj->Image[0][intelObj->firstLevel];


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

	 state[0] |= ((b << SS2_LOD_BIAS_SHIFT) & SS2_LOD_BIAS_MASK);
      }


      /* YUV conversion:
       */
      if (firstImage->TexFormat->MesaFormat == MESA_FORMAT_YCBCR ||
          firstImage->TexFormat->MesaFormat == MESA_FORMAT_YCBCR_REV)
         state[0] |= SS2_COLORSPACE_CONVERSION;

      /* Shadow:
       */
      if (tObj->CompareMode == GL_COMPARE_R_TO_TEXTURE_ARB &&
          tObj->Target != GL_TEXTURE_3D) {

         state[0] |= (SS2_SHADOW_ENABLE |
		      intel_translate_compare_func(tObj->CompareFunc));

         minFilt = FILTER_4X4_FLAT;
         magFilt = FILTER_4X4_FLAT;
      }

      state[0] |= ((minFilt << SS2_MIN_FILTER_SHIFT) |
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
	 
	 if (intel->strict_conformance) {
	    assert(0);
/* 	    sampler->fallback = true; */
	    /* TODO */
	 }
      }

      state[1] =
         ((translate_wrap_mode(ws) << SS3_TCX_ADDR_MODE_SHIFT) |
          (translate_wrap_mode(wt) << SS3_TCY_ADDR_MODE_SHIFT) |
          (translate_wrap_mode(wr) << SS3_TCZ_ADDR_MODE_SHIFT) |
	  (unit << SS3_TEXTUREMAP_INDEX_SHIFT));

      /* Or some field in tObj? */
      if (intel->state.Texture->Unit[unit]._ReallyEnabled == TEXTURE_RECT_BIT)
	 state[1] |= SS3_NORMALIZED_COORDS;
   }

   state[2] = INTEL_PACKCOLOR8888(tObj->_BorderChan[0],
				  tObj->_BorderChan[1],
				  tObj->_BorderChan[2],
				  tObj->_BorderChan[3]);
}



static void upload_samplers( struct intel_context *intel )
{
   GLint i, dirty = 0, nr = 0;
   GLuint state[I915_TEX_UNITS][3];

   for (i = 0; i < I915_TEX_UNITS; i++) {
      if (intel->state.Texture->Unit[i]._ReallyEnabled) {
	 update_sampler( intel, i, state[i] );
	 nr++;
	 dirty |= (1<<i);
      }
   }

   if (nr) {
      BEGIN_BATCH(2 + nr * 3, 0);
      OUT_BATCH(_3DSTATE_SAMPLER_STATE | (3 * nr));
      OUT_BATCH(dirty);
      for (i = 0; i < I915_TEX_UNITS; i++) {
	 if (intel->state.Texture->Unit[i]._ReallyEnabled) {
	    OUT_BATCH(state[i][0]);
	    OUT_BATCH(state[i][1]);
	    OUT_BATCH(state[i][2]);
	 }
      }
      ADVANCE_BATCH();
   }
}

const struct intel_tracked_state i915_upload_samplers = {
   .dirty = {
      .mesa = _NEW_TEXTURE,
      .intel = 0,
      .extra = 0
   },
   .update = upload_samplers
};
