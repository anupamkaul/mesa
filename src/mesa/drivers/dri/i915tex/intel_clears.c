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

#include "intel_context.h"
#include "intel_frame_tracker.h"
#include "intel_state.h"
#include "intel_metaops.h"
#include "intel_fbo.h"
#include "draw/intel_draw.h"



static void draw_quad( struct intel_context *intel,
		       GLuint x1, GLuint y1,
		       GLuint x2, GLuint y2,
		       GLfloat depth,
		       GLuint color )
{
   struct intel_metaops_color_vertex vert[4];

   if (0) _mesa_printf("%s %d..%d, %d..%d\n", __FUNCTION__, x1, x2, y1, y2);

   vert[0].xyz[0] = x1;
   vert[0].xyz[1] = y1;
   vert[0].xyz[2] = depth;
   vert[0].color.ui = color;

   vert[1].xyz[0] = x2;
   vert[1].xyz[1] = y1;
   vert[1].xyz[2] = depth;
   vert[1].color.ui = color;

   vert[2].xyz[0] = x2;
   vert[2].xyz[1] = y2;
   vert[2].xyz[2] = depth;
   vert[2].color.ui = color;

   vert[3].xyz[0] = x1;
   vert[3].xyz[1] = y2;
   vert[3].xyz[2] = depth;
   vert[3].color.ui = color;

   intel_meta_draw_color_quad( intel, vert );
}


static void
intelClearWithClearRects(struct intel_context *intel, GLbitfield mask)
{
   GLcontext *ctx = &intel->ctx;
   struct gl_framebuffer *fb = ctx->DrawBuffer;


   {
      GLcontext *ctx = &intel->ctx;
      GLuint clearparams = mask & (ctx->DrawBuffer->_ColorDrawBufferMask[0] | 
				   BUFFER_BIT_STENCIL |
				   BUFFER_BIT_DEPTH);

      if (intel->state.clearparams != clearparams) {
	 intel->state.dirty.intel |= INTEL_NEW_CLEAR_PARAMS;
	 intel->state.clearparams = clearparams;
      }
   }
   
   intel_update_software_state( intel );

   intel->render->clear_rect( intel->render, 
			      mask,
			      fb->_Xmin, fb->_Ymin,
			      fb->_Xmax, fb->_Ymax );
}

/* A true meta version of this would be very simple and additionally
 * machine independent.  Maybe we'll get there one day.
 */
static void
intelClearWithTris(struct intel_context *intel, GLbitfield mask)
{
   GLuint clearColor = intel->ClearColor8888;
   GLcontext *ctx = &intel->ctx;
   GLuint buf;
   struct gl_framebuffer *fb = ctx->DrawBuffer;


/*    if (INTEL_DEBUG & DEBUG_BLIT) */
      _mesa_printf("%s 0x%x\n", __FUNCTION__, mask);

   intel_install_meta_state(intel);


   /* Back and stencil cliprects are the same.  Try and do both
    * buffers at once:
    */
   if (mask & (BUFFER_BIT_BACK_LEFT | 
	       BUFFER_BIT_STENCIL | 
	       BUFFER_BIT_DEPTH)) 
   {
      struct intel_region *backRegion =
	 intel_get_rb_region(fb, BUFFER_BACK_LEFT);
      struct intel_region *depthRegion =
	 intel_get_rb_region(fb, BUFFER_DEPTH);

      intel_meta_draw_region(intel, backRegion, depthRegion);

      if (mask & BUFFER_BIT_BACK_LEFT)
	 intel_meta_color_mask(intel, GL_TRUE);
      else
	 intel_meta_color_mask(intel, GL_FALSE);

      if (mask & BUFFER_BIT_STENCIL)
	 intel_meta_stencil_replace(intel,
				    intel->ctx.Stencil.WriteMask[0],
				    intel->ctx.Stencil.Clear);
      else
	 intel_meta_no_stencil_write(intel);

      if (mask & BUFFER_BIT_DEPTH)
	 intel_meta_depth_replace(intel);
      else
	 intel_meta_no_depth_write(intel);

      draw_quad( intel, 
		 fb->_Xmin, fb->_Ymin,
		 fb->_Xmax, fb->_Ymax,
		 intel->ctx.Depth.Clear, 
		 clearColor);

      mask &= ~(BUFFER_BIT_BACK_LEFT | 
		BUFFER_BIT_STENCIL | 
		BUFFER_BIT_DEPTH);
   }

   /* clear the remaining (color) renderbuffers */
   for (buf = 0; buf < BUFFER_COUNT && mask; buf++) {
      const GLuint bufBit = 1 << buf;
      if (mask & bufBit) {
	 struct intel_renderbuffer *irbColor =
	    intel_renderbuffer(fb->Attachment[buf].Renderbuffer);

	 ASSERT(irbColor);

	 intel_meta_no_depth_write(intel);
	 intel_meta_no_stencil_write(intel);
	 intel_meta_color_mask(intel, GL_TRUE);
	 intel_meta_draw_region(intel, irbColor->region, NULL);
	 
	 draw_quad( intel, 
		    fb->_Xmin, fb->_Ymin,
		    fb->_Xmax, fb->_Ymax,
		    intel->ctx.Depth.Clear, 
		    clearColor);

	 mask &= ~bufBit;
      }
   }

   intel_leave_meta_state(intel);
}

/**
 * Called by ctx->Driver.Clear.
 */
void intelClear(GLcontext *ctx, GLbitfield mask)
{
   struct intel_context *intel = intel_context(ctx);
   const GLuint colorMask = *((GLuint *) & ctx->Color.ColorMask);
   GLbitfield rect_mask = 0;
   GLbitfield tri_mask = 0;
   GLbitfield swrast_mask = 0;
   struct gl_framebuffer *fb = ctx->DrawBuffer;
   GLuint i;

   _mesa_printf("CLEAR %x\n", mask);

   intel_frame_note_clear( intel->ft, mask );

   /* Fallback to tris if colormask or stencil mask are active.
    */
   if ((mask & BUFFER_BITS_COLOR) && 
       (colorMask != ~0 ||
	(mask & BUFFER_BITS_COLOR) != ctx->DrawBuffer->_ColorDrawBufferMask[0])) 
   {
      tri_mask = mask & (BUFFER_BITS_COLOR|BUFFER_BIT_DEPTH|BUFFER_BIT_STENCIL);
      mask &= ~tri_mask;
   }
   else if ((mask & BUFFER_BIT_STENCIL) &&
	    (intel->ctx.Stencil.WriteMask[0] & 0xff) != 0xff) 
   {
      tri_mask = mask & (BUFFER_BITS_COLOR|BUFFER_BIT_DEPTH|BUFFER_BIT_STENCIL);
      mask &= ~tri_mask;
   }
   else 
   {
      rect_mask = mask & (BUFFER_BITS_COLOR|BUFFER_BIT_DEPTH|BUFFER_BIT_STENCIL);
      mask &= ~rect_mask;
   }
   

   /* What's this all about?  
    */
   for (i = 0; i < BUFFER_COUNT; i++) {
      GLuint bufBit = 1 << i;
      if ((rect_mask | tri_mask) & bufBit) {
         if (!fb->Attachment[i].Renderbuffer->ClassID) {
	    _mesa_printf("trying to clear attachment %d, bit 0x%x, ClassID %d\n",
			 i, bufBit, fb->Attachment[i].Renderbuffer->ClassID);
            rect_mask &= ~bufBit;
            tri_mask &= ~bufBit;
            swrast_mask |= bufBit;
         }
      }
   }

   /* I really don't care.
    */
   assert(swrast_mask == 0);
   assert(mask == 0);

   if (tri_mask) {
      assert(rect_mask == 0);
      intelClearWithTris(intel, tri_mask);
   }
   else if (rect_mask) {
      assert(tri_mask == 0);
      intelClearWithClearRects(intel, rect_mask);
   }
}


void
intelClearColor(GLcontext * ctx, const GLfloat color[4])
{
   struct intel_context *intel = intel_context(ctx);
   GLubyte clear[4];
   GLuint cc565, cc8888;

   CLAMPED_FLOAT_TO_UBYTE(clear[0], color[0]);
   CLAMPED_FLOAT_TO_UBYTE(clear[1], color[1]);
   CLAMPED_FLOAT_TO_UBYTE(clear[2], color[2]);
   CLAMPED_FLOAT_TO_UBYTE(clear[3], color[3]);

   /* compute both 32 and 16-bit clear values */
   cc8888 = INTEL_PACKCOLOR8888(clear[0], clear[1], clear[2], clear[3]);
   cc565 = INTEL_PACKCOLOR565(clear[0], clear[1], clear[2]);

   if (cc8888 != intel->ClearColor8888) {
      intel->ClearColor8888 = cc8888;
      intel->ClearColor565 = cc565;
      intel->state.dirty.intel |= INTEL_NEW_CLEAR_PARAMS;
   }
}


