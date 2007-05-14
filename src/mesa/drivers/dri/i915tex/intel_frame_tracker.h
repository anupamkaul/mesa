/*
 Copyright (C) Aristocrat Technologies.  2007.  All Rights Reserved.
 Copyright (C) Intel Corp.  2007.  All Rights Reserved.

 Aristocrat and Intel funded Tungsten Graphics
 (http://www.tungstengraphics.com) to support zone rendering in this
 3D driver.
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 **********************************************************************/

#ifndef INTEL_FRAME_TRACKER_H
#define INTEL_FRAME_TRACKER_H

#include "imports.h"

struct intel_context;
struct intel_frame_tracker;

struct intel_frame_tracker *intel_frame_tracker_create( struct intel_context * );
void intel_frame_tracker_destroy( struct intel_frame_tracker * );


GLboolean intel_frame_is_in_frame( struct intel_frame_tracker *ft );
GLboolean intel_frame_predict_flush( struct intel_frame_tracker *ft );
GLboolean intel_frame_predict_finish( struct intel_frame_tracker *ft );
GLboolean intel_frame_predict_window_rebind( struct intel_frame_tracker *ft );
GLboolean intel_frame_predict_resize( struct intel_frame_tracker *ft );


void intel_frame_note_resize( struct intel_frame_tracker *ft );
void intel_frame_note_window_rebind( struct intel_frame_tracker *ft );
void intel_frame_note_clear( struct intel_frame_tracker *ft, GLbitfield mask );


GLboolean intel_frame_can_clear_stencil( struct intel_frame_tracker *ft );


enum {
   INTEL_FT_FLUSHED = 0,
   INTEL_FT_SWAP_BUFFERS,
   INTEL_FT_GL_FLUSH,		/* like swapbuffers, frontbuffer rendering */
   INTEL_FT_CLASSIC,
   INTEL_FT_SWRAST,
   INTEL_FT_SWZ,
   INTEL_FT_HWZ,
   INTEL_FT_BLITTER
};

void intel_frame_set_mode( struct intel_frame_tracker *ft,
			   GLuint new_mode );

GLuint intel_frame_mode( const struct intel_frame_tracker *ft );

void intel_frame_flush_and_restart( struct intel_frame_tracker *ft );

#endif
