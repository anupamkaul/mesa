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

   /* Primitive-dependent fallbacks.  Could optimize.
    */
   switch (intel->draw.reduced_primitive) {
   case GL_POINTS:
      if (intel->state.Point->_Attenuated)
	 return GL_TRUE;
      break;
   case GL_LINES:
      if (intel->state.Line->StippleFlag)
	 return GL_TRUE;
      break;
   case GL_TRIANGLES:
      if (intel->state.Polygon->StippleFlag /* && !intel->hw_stipple */)
	 return GL_TRUE;
      break;
   }


   return GL_FALSE;
}

static void check_fallback( struct intel_context *intel )
{
   GLboolean flag = do_check_fallback( intel );

   /* May raise INTEL_NEW_FALLBACK
    */
   FALLBACK(intel, INTEL_FALLBACK_OTHER, flag );
}

const struct intel_tracked_state i915_check_fallback = {
   .dirty = {
      .mesa = (_NEW_BUFFERS | 
	       _NEW_RENDERMODE | 
	       _NEW_TEXTURE | 
	       _NEW_STENCIL |
	       _NEW_LINE |
	       _NEW_POINT |
	       _NEW_POLYGON),

      .intel  = (INTEL_NEW_METAOPS | 
		 INTEL_NEW_REDUCED_PRIMITIVE),

      .extra = 0
   },
   .update = check_fallback
};


/* Not sure if this is really a good thing to have as a state atom or
 * not...
 */
static void choose_render( struct intel_context *intel )
{
   struct intel_render *new_render = NULL;

#if 0
   if (check_swz( intel ))
      new_render = i915->swz;
#endif

   if (intel->Fallback == 0)
      new_render = intel->classic;
   else
      new_render = intel->swrender;

   if (new_render != intel->render) {

      /* Shut down the old renderer:
       */
      intel->render->flush(intel->render, !intel->draw.in_frame );
/*       intel->render->end_render(intel->render); */

      /* Install the new one: 
       */
      intel->render = new_render;
      intel->render->start_render( intel->render );

      /* Update any state:
       */
      intel->render->set_prim( intel->render, intel->draw.hw_prim );

      if (intel->draw.in_draw)
	 intel->render->new_vertices( intel->render );
   }
}




const struct intel_tracked_state i915_choose_render = {
   .dirty = {
      .mesa = 0,
      .intel  = INTEL_NEW_FALLBACK,
      .extra = 0
   },
   .update = choose_render
};




