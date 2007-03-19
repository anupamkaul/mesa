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

#ifndef INTEL_VB_H
#define INTEL_VB_H


struct intel_vb;

struct intel_vb *intel_vb_init( struct intel_context *intel );
void intel_vb_destroy( struct intel_vb *vb );

void intel_vb_flush( struct intel_vb *vb );

void *intel_vb_alloc( struct intel_vb *vb, GLuint space );

/* If successful, guarantees you can later allocate upto
 * min_free_space bytes without needing to flush.  
 */
GLboolean intel_vb_begin_dynamic_alloc( struct intel_vb *vb,
					GLuint min_free_space );

void *intel_vb_extend_dynamic_alloc( struct intel_vb *vb, GLuint space );
GLuint intel_vb_end_dynamic_alloc( struct intel_vb *vb );


#endif
