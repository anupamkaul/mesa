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
#include "intel_batchbuffer.h"

#define FILE_DEBUG_FLAG DEBUG_FRAME

/* Track events over the last few frames and use these to predict
 * future behaviour.
 *
 * Track whether we are in a frame 
 */
struct intel_frame_tracker 
{
   struct intel_context *intel;

   GLuint mode;

   GLboolean in_frame;

   GLbitfield allclears;
   
   /* Bitflags:
    */
   GLuint flush_prediction;
   GLuint finish_prediction;
   GLuint resize_prediction;

   /* Counters: 
    */
   GLuint frame;
};


GLboolean intel_frame_is_in_frame( struct intel_frame_tracker *ft )
{
   return ft->in_frame;
}

GLboolean intel_frame_predict_flush( struct intel_frame_tracker *ft )
{
   return ft->flush_prediction != 0;
}


/* Resize prediction should be based on elapsed time, not a count of
 * swapbuffers.
 */
GLboolean intel_frame_predict_resize( struct intel_frame_tracker *ft )
{
   return ft->resize_prediction != 0;
}


/* These are worse than flushes.  If we think one is coming, we need
 * to try and aim for low latency over rendering performance, ie no
 * zone rendering.
 */
GLboolean intel_frame_predict_finish( struct intel_frame_tracker *ft )
{
   return ft->finish_prediction != 0;
}


GLboolean intel_frame_can_clear_stencil( struct intel_frame_tracker *ft )
{
   /* This isn't quite accurate, tries to check whether the stencil
    * buffer has ever been initialized, but it could be written to by
    * other methods, like 3d drawing with unconditional stencil
    * writes, etc.  Probably need to disable this or deal with trying
    * to catch all the cases. 
    *
    * Would be nicer to know if the app really requested a stencil
    * buffer...
    */
   if ((ft->allclears & BUFFER_BIT_STENCIL) == 0) 
      return GL_TRUE;

   /* Z/Stencil buffer contents undefined after swapbuffer?
    */
   if (ft->mode == INTEL_FT_SWAP_BUFFERS)
      return GL_TRUE;

   return GL_FALSE;
}





static void finish_frame( struct intel_frame_tracker *ft )
{
   ft->frame++;
   ft->in_frame = 0;
   ft->flush_prediction >>= 1;
   ft->finish_prediction >>= 1;
   ft->resize_prediction >>= 1;
   ft->intel->state.dirty.intel |= INTEL_NEW_FRAME;
}


static void start_frame( struct intel_frame_tracker *ft )
{
   ft->in_frame = 1;
}


void intel_frame_note_clear( struct intel_frame_tracker *ft,
			     GLbitfield mask )
{
   if (!ft->intel->ctx.Scissor.Enabled)
      ft->allclears |= mask;
}


void intel_frame_note_resize( struct intel_frame_tracker *ft )
{
   ft->resize_prediction |= (1<<16);
}


void intel_frame_note_window_rebind( struct intel_frame_tracker *ft )
{
   assert (!ft->in_frame);
}

GLuint  intel_frame_mode( const struct intel_frame_tracker *ft )
{
   return ft->mode; 
}


static const char * mode_name[] = 
{
   "FLUSHED",
   "SWAP_BUFFERS",
   "GL_FLUSH",
   "CLASSIC",
   "SWRAST",
   "SWZ",
   "HWZ",
   "BLITTER"
};


void intel_frame_set_mode( struct intel_frame_tracker *ft,
			   GLuint new_mode )
{
   struct intel_context *intel = ft->intel;
   GLboolean discard_z_buffer = (new_mode == INTEL_FT_SWAP_BUFFERS);
   GLboolean ignore_buffer_contents = GL_FALSE;

   if (ft->mode == new_mode) 
      return;

   if (INTEL_DEBUG & (DEBUG_RENDER|DEBUG_FALLBACKS|DEBUG_FRAME)) 
      _mesa_printf("transiton %s -> %s\n",
		   mode_name[ft->mode],
		   mode_name[new_mode]);

   switch (ft->mode) {
   case INTEL_FT_SWAP_BUFFERS:
      ignore_buffer_contents = GL_TRUE;
      start_frame( ft );
      break;

   case INTEL_FT_GL_FLUSH:
      start_frame( ft );
      break;
	 
   case INTEL_FT_FLUSHED:
      break;

   case INTEL_FT_SWRAST:
      intel->swrender->flush( intel->swrender, discard_z_buffer );
      break;

   case INTEL_FT_CLASSIC:
      intel->classic->flush( intel->classic, discard_z_buffer );
      break;

   case INTEL_FT_SWZ:
      intel->swz->flush( intel->swz, discard_z_buffer );
      break;

   case INTEL_FT_HWZ:
      intel->hwz->flush( intel->hwz, discard_z_buffer );
      break;

   case INTEL_FT_BLITTER:
      intel_batchbuffer_flush( intel->batch, GL_FALSE );
      break;
	 
   default:
      assert(0);
      break;
   }

   ft->mode = INTEL_FT_FLUSHED;
   
   switch (new_mode) {
   case INTEL_FT_GL_FLUSH:
   case INTEL_FT_SWAP_BUFFERS:
      finish_frame(ft);
      break;

   case INTEL_FT_FLUSHED:
      break;

   case INTEL_FT_SWRAST:
      intel->swrender->start_render( intel->swrender, ignore_buffer_contents );
      break;

   case INTEL_FT_CLASSIC:
      intel->classic->start_render( intel->classic, ignore_buffer_contents );
      break;

   case INTEL_FT_SWZ:
      intel->swz->start_render( intel->swz, ignore_buffer_contents );
      break;

   case INTEL_FT_HWZ:
      intel->hwz->start_render( intel->hwz, ignore_buffer_contents );
      break;

   case INTEL_FT_BLITTER:
      break;
	 
   default:
      assert(0);
      break;      
   }

   ft->mode = new_mode;
}


void intel_frame_flush_and_restart( struct intel_frame_tracker *ft )
{
   GLuint mode = ft->mode;
   intel_frame_set_mode( ft, INTEL_FT_FLUSHED );
   intel_frame_set_mode( ft, mode );
}
   

   
struct intel_frame_tracker *intel_frame_tracker_create( struct intel_context *intel )
{
   struct intel_frame_tracker *ft = CALLOC_STRUCT( intel_frame_tracker );
   ft->intel = intel;
   ft->mode = INTEL_FT_SWAP_BUFFERS;
   return ft;
}

void intel_frame_tracker_destroy( struct intel_frame_tracker *ft )
{
   FREE(ft);
}
