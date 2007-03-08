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
      assert(0);
      return 0;
   }
}





static void
upload_maps( struct i915_context *i915 )
{
   GLcontext *ctx = &i915->intel.ctx;

   for (unit = 0; unit < I915_TEX_UNITS; unit++) {
      switch (ctx.Texture.Unit[unit]._ReallyEnabled) {

	 struct gl_texture_object *tObj = ctx->Texture.Unit[unit]._Current;
	 struct intel_texture_object *intelObj = intel_texture_object(tObj);
	 struct gl_texture_image *firstImage;
	 GLuint *state = i915->state.Tex[unit];

	 memset(state, 0, sizeof(state));

	 /*We need to refcount these. */

	 if (i915->state.tex_buffer[unit] != NULL) {
	    driBOUnReference(i915->state.tex_buffer[unit]);
	    i915->state.tex_buffer[unit] = NULL;
	 }

	 if (!intel_finalize_mipmap_tree(intel, unit))
	    return GL_FALSE;

	 /* Get first image here, since intelObj->firstLevel will get set in
	  * the intel_finalize_mipmap_tree() call above.
	  */
	 firstImage = tObj->Image[0][intelObj->firstLevel];

	 i915->state.tex_buffer[unit] = driBOReference(intelObj->mt->region->buffer);
	 i915->state.tex_offset[unit] = intel_miptree_image_offset(intelObj->mt, 0,
								   intelObj->
								   firstLevel);

	 state[I915_TEXREG_MS3] =
	    (((firstImage->Height - 1) << MS3_HEIGHT_SHIFT) |
	     ((firstImage->Width - 1) << MS3_WIDTH_SHIFT) |
	     translate_texture_format(firstImage->TexFormat->MesaFormat) |
	     MS3_USE_FENCE_REGS);

	 state[I915_TEXREG_MS4] =
	    (((((intelObj->mt->pitch * intelObj->mt->cpp) / 4) - 1) << MS4_PITCH_SHIFT) |
	     MS4_CUBE_FACE_ENA_MASK |
	     ((((intelObj->lastLevel - intelObj->firstLevel) * 4)) << MS4_MAX_LOD_SHIFT) |
	     ((firstImage->Depth - 1) << MS4_VOLUME_DEPTH_SHIFT));

	 
	 DBG(TEXTURE, "state[I915_TEXREG_MS2] = 0x%x\n", state[I915_TEXREG_MS2]);
	 DBG(TEXTURE, "state[I915_TEXREG_MS3] = 0x%x\n", state[I915_TEXREG_MS3]);
	 DBG(TEXTURE, "state[I915_TEXREG_MS4] = 0x%x\n", state[I915_TEXREG_MS4]);

      }
   }
   else {
      if (i915->state.active & I915_UPLOAD_TEX(i))
	 I915_ACTIVESTATE(i915, I915_UPLOAD_TEX(i), GL_FALSE);

      if (i915->state.tex_buffer[i] != NULL) {
	 driBOUnReference(i915->state.tex_buffer[i]);
	 i915->state.tex_buffer[i] = NULL;
      }

      break;
   }


   {
      int nr = 0;

      for (i = 0; i < I915_TEX_UNITS; i++)
	 if (dirty & I915_UPLOAD_TEX(i))
	    nr++;

      BEGIN_BATCH(2 + nr * 3, 0);
      OUT_BATCH(_3DSTATE_MAP_STATE | (3 * nr));
      OUT_BATCH((dirty & I915_UPLOAD_TEX_ALL) >> I915_UPLOAD_TEX_0_SHIFT);
      for (i = 0; i < I915_TEX_UNITS; i++) {
	 if (dirty & I915_UPLOAD_TEX(i)) {

	    if (state->tex_buffer[i]) {
	       OUT_RELOC(state->tex_buffer[i],
			 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
			 DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
			 state->tex_offset[i]);
	    }
	    else {
	       assert(i == 0);
	       assert(state == &i915->meta);
	       OUT_BATCH(0);
	    }

	    OUT_BATCH(state->Tex[i][I915_TEXREG_MS3]);
	    OUT_BATCH(state->Tex[i][I915_TEXREG_MS4]);
	 }
      }
      ADVANCE_BATCH();
   }

   return GL_TRUE;
}





const struct i915_tracked_state i915_upload_maps = {
   .dirty = {
      .mesa = (_NEW_TEXTURE),
      .intel = 0,
      .indirect = 0
   },
   .update = upload_maps
};
