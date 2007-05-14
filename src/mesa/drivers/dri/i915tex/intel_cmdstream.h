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
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */

#ifndef INTEL_CMDSTREAM_H
#define INTEL_CMDSTREAM_H

#include "intel_context.h"

#define CMDSTREAM_ORDER 9
#define CMDSTREAM_SIZE (1<<CMDSTREAM_ORDER)

GLubyte *intel_cmdstream_alloc_block( struct intel_context *intel );

void intel_cmdstream_use_batch_range( struct intel_context *intel,
				      GLuint start_offset,
				      GLuint finish_offset );

void intel_cmdstream_reset( struct intel_context *intel );

static INLINE GLuint intel_cmdstream_space( GLubyte *ptr )
{
   return CMDSTREAM_SIZE - (((unsigned long)ptr) & (CMDSTREAM_SIZE-1));
}


#endif
