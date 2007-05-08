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

#include "macros.h"
#include "intel_context.h"
#include "intel_fbo.h"
#include "intel_lock.h"
#include "intel_frame_tracker.h"

#define FILE_DEBUG_FLAG DEBUG_FRAME

/* Track events over the last few frames and use these to predict
 * future behaviour.
 *
 * Track whether we are in a frame 
 */
struct intel_frame_tracker 
{
   struct intel_context *intel;

   GLboolean in_frame;
   GLboolean in_draw;

   GLboolean single_buffered;
   GLboolean resize_flag;

   GLbitfield allclears;
   
   /* Bitflags:
    */
   GLuint flush_prediction;
   GLuint draw_without_clears;
   GLuint map_depth_pixels;

   /* Counters: 
    */
   GLuint frame;
};


GLboolean intel_frame_is_in_frame( struct intel_frame_tracker *ft )
{
   return ft->in_frame;
}

GLboolean intel_frame_predict_forced_flush( struct intel_frame_tracker *ft )
{
   return ft->flush_prediction != 0;
}




/* Delay framebuffer size changes until after the end of the frame.
 * This is necessary for zone rendering but also clears up a whole
 * bunch of other code.
 */
static void emit_resize( struct intel_frame_tracker *ft )
{
   struct intel_context *intel = ft->intel;
   __DRIdrawablePrivate *dPriv = intel->driDrawable;
   struct gl_framebuffer *fb = (struct gl_framebuffer *) dPriv->driverPrivate;

   DBG("%s %dx%d\n", __FUNCTION__, dPriv->w, dPriv->h);

   intel_resize_framebuffer(&intel->ctx, fb, dPriv->w, dPriv->h);

//   intel->state.dirty.intel |= INTEL_NEW_WINDOW_DIMENSIONS;

   ft->resize_flag = GL_FALSE;
}

static void finish_frame( struct intel_frame_tracker *ft )
{
   ft->frame++;
   ft->in_frame = 0;
   ft->flush_prediction >>= 1;

   if (ft->resize_flag && !ft->in_draw) 
      emit_resize( ft );
}


/* Almost all bad events that we care about manifest themselves
 * firstly as a flush.  This includes stuff like fallbacks, blits
 * to/from the screen, readpixels, etc.  If there is a flush during
 * the frame, that's really the event that we care about.  
 * 
 * The only thing worst is a flush followed by a wait for idle, but
 * it's not clear what more we'd do in that case compared to what we
 * have to do for a regular flush.
 *
 * XXX: Want to distinguish between forced flushes due to client
 * events and internal flushes due to simply running out of
 * batchbuffer.  And then again, need to consider running out of batch
 * vs. running out of bin memory.
 */
void intel_frame_note_flush( struct intel_frame_tracker *ft,
			     GLboolean forced)
{
   DBG("%s forced %d in_frame %d\n", __FUNCTION__, forced, ft->in_frame);
   if (ft->in_frame && forced) {
      finish_frame( ft );
      ft->flush_prediction |= (1<<8);
   }
}

void intel_frame_note_clear( struct intel_frame_tracker *ft,
			     GLbitfield mask,
			     GLboolean clearrect )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);
   if (!ft->in_frame) {
      struct intel_context *intel = ft->intel;

      /* Update window dimensions for the coming frame???  This is
       * bogus, but will do for now.
       */
      LOCK_HARDWARE(intel);
      UPDATE_CLIPRECTS(intel);
      UNLOCK_HARDWARE(intel);

      ft->in_frame = GL_TRUE;

      if (!ft->intel->ctx.Scissor.Enabled)
	 ft->allclears |= mask;
   }
}

GLboolean intel_frame_can_clear_stencil( struct intel_frame_tracker *ft )
{
   if ((ft->allclears & BUFFER_BIT_STENCIL) == 0) 
      return GL_TRUE;

   return GL_FALSE;
}



void intel_frame_note_swapbuffers( struct intel_frame_tracker *ft )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);

   intel_draw_finish_frame( ft->intel->draw );
   
   if (ft->in_frame) {
      finish_frame( ft );
   }
   else
      _mesa_printf("unexpected swapbuffers\n");
}


void intel_frame_note_window_resize( struct intel_frame_tracker *ft )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);
   ft->resize_flag = GL_TRUE;
   if (!ft->in_frame) 
      emit_resize(ft);
}


void intel_frame_note_window_rebind( struct intel_frame_tracker *ft )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);
   assert (!ft->in_frame);
   emit_resize(ft);
}

void intel_frame_note_draw_start( struct intel_frame_tracker *ft )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);
   assert(!ft->in_draw);
   ft->in_draw = 1;
   if (!ft->in_frame) {
      ft->draw_without_clears |= (1<<31);
      ft->in_frame = 1;
   }
}

void intel_frame_note_draw_end( struct intel_frame_tracker *ft )
{
   DBG("%s in_frame %d\n", __FUNCTION__, ft->in_frame);
   assert(ft->in_draw);
   ft->in_draw = 0;

   if (ft->resize_flag && !ft->in_frame) 
      emit_resize( ft );
}



   
struct intel_frame_tracker *intel_frame_tracker_create( struct intel_context *intel )
{
   struct intel_frame_tracker *ft = CALLOC_STRUCT( intel_frame_tracker );
   ft->intel = intel;
   return ft;
}

void intel_frame_tracker_destroy( struct intel_frame_tracker *ft )
{
   FREE(ft);
}
