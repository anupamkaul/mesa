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

#ifndef INTEL_METAOPS_H
#define INTEL_METAOPS_H

#include "glheader.h"

struct intel_context;
struct intel_region;
struct _DriBufferObject;

void intel_meta_draw_poly(struct intel_context *intel,
			  GLuint n,
			  GLfloat xy[][2],
			  GLfloat z, GLuint color, GLfloat tex[][2]);

void intel_meta_draw_quad(struct intel_context *intel,
			  GLfloat x0, GLfloat x1,
			  GLfloat y0, GLfloat y1,
			  GLfloat z,
			  GLuint color,
			  GLfloat s0, GLfloat s1, GLfloat t0, GLfloat t1);

void intel_meta_draw_region( struct intel_context *intel,
			     struct intel_region *draw_region,
			     struct intel_region *depth_region );

GLboolean intel_meta_tex_rect_source(struct intel_context *intel,
				     struct _DriBufferObject *buffer,
				     GLuint offset,
				     GLuint pitch, GLuint height, 
				     GLenum format, GLenum type);

void intel_meta_import_pixel_state(struct intel_context *intel);

void intel_meta_texture_blend_replace(struct intel_context *intel);

void intel_meta_no_texture( struct intel_context *intel );

void intel_meta_color_mask( struct intel_context *intel, GLboolean state );

void intel_meta_stencil_replace( struct intel_context *intel,
				 GLuint s_mask,
				 GLuint s_clear);

void intel_meta_depth_replace( struct intel_context *intel );

void intel_meta_no_depth_write( struct intel_context *intel );

void intel_meta_no_stencil_write( struct intel_context *intel );

void intel_meta_flat_shade( struct intel_context *intel );

void intel_install_meta_state( struct intel_context *intel );
void intel_leave_meta_state( struct intel_context *intel );

void intel_metaops_init( struct intel_context *intel );
void intel_metaops_destroy( struct intel_context *intel );


#endif
