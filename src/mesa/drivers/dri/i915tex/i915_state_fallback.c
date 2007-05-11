/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */
 


#include "intel_batchbuffer.h"
#include "intel_regions.h"
#include "intel_context.h"
#include "intel_tex.h"
#include "intel_fbo.h"
#include "i915_context.h"

/* A place to check fallbacks that aren't calculated on the fly or in
 * callback functions.  
 */
static GLboolean do_check_fallback(struct intel_context *intel)
{
   GLuint i;
   
   /* INTEL_NEW_METAOPS
    */
   if (intel->metaops.active)
      return GL_FALSE;

   /* _NEW_BUFFERS
    */
   if (intel->state._ColorDrawBufferMask0 != BUFFER_BIT_FRONT_LEFT &&
       intel->state._ColorDrawBufferMask0 != BUFFER_BIT_BACK_LEFT)
      return GL_TRUE;

   /* _NEW_RENDERMODE
    *
    * XXX: need to save/restore RenderMode in metaops state, or
    * somehow move to a new attribs pointer:
    */
   if (intel->state.RenderMode != GL_RENDER)
      return GL_TRUE;

   /* _NEW_TEXTURE:
    */
   for (i = 0; i < I915_TEX_UNITS; i++) {
      struct gl_texture_unit *texUnit = &intel->state.Texture->Unit[i];
      if (texUnit->_ReallyEnabled) {
	 struct intel_texture_object *intelObj = intel_texture_object(texUnit->_Current);
	 struct gl_texture_image *texImage = intelObj->base.Image[0][intelObj->firstLevel];
	 if (texImage->Border)
	    return GL_TRUE;

	 if (!intel_finalize_mipmap_tree(intel, i)) 
	    return GL_TRUE;
      }
   }
   
   /* _NEW_STENCIL, _NEW_BUFFERS 
    */
   if (intel->state.Stencil->Enabled) {
      struct intel_renderbuffer *irbStencil = 
	 (intel->state.DrawBuffer 
	  ? intel_get_renderbuffer(intel->state.DrawBuffer, BUFFER_STENCIL)
	  : NULL);

      intel->hw_stencil = irbStencil && irbStencil->region;

      if (!intel->hw_stencil)
	 return GL_TRUE;
   }


   return GL_FALSE;
}

static void check_fallback( struct intel_context *intel )
{
   {
      GLboolean flag = do_check_fallback( intel );

      /* May raise INTEL_NEW_FALLBACK
       */
      FALLBACK(intel, INTEL_FALLBACK_OTHER, flag );
   }

   
   {
      struct i915_context *i915 = i915_context( &intel->ctx );
      GLuint fallback_prims = 0;

      /* _NEW_POINT 
       */
      if (intel->state.Point->_Attenuated)
	 fallback_prims |= (1 << GL_POINTS);
      
      /* _NEW_LINE 
       */
      if (intel->state.Line->StippleFlag)
	 fallback_prims |= ((1 << GL_LINES) |
			    (1 << GL_LINE_STRIP) |
			    (1 << GL_LINE_LOOP));

      /* I915_NEW_POLY_STIPPLE_FALLBACK 
       */
      if (i915->fallback_on_poly_stipple)
	 fallback_prims |= ((1 << GL_TRIANGLES) |
			    (1 << GL_TRIANGLE_FAN) |
			    (1 << GL_TRIANGLE_STRIP) |
			    (1 << GL_QUADS) |
			    (1 << GL_QUAD_STRIP) |
			    (1 << GL_POLYGON));

      if (fallback_prims != intel->fallback_prims) {
	 intel->state.dirty.intel |= INTEL_NEW_FALLBACK_PRIMS;
	 intel->fallback_prims = fallback_prims;
      }
   }
}

const struct intel_tracked_state i915_check_fallback = {
   .dirty = {
      .mesa = (_NEW_BUFFERS | 
	       _NEW_RENDERMODE | 
	       _NEW_TEXTURE | 
	       _NEW_STENCIL ),

      .intel  = (INTEL_NEW_METAOPS |
		 I915_NEW_POLY_STIPPLE_FALLBACK),

      .extra = 0
   },
   .update = check_fallback
};

