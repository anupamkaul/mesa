/*
 * mesa 3-D graphics library
 * Version:  6.5
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file st_vbo_context.h
 * \brief ST_VBO builder module datatypes and definitions.
 * \author Keith Whitwell
 */


#ifndef _ST_VBO_H
#define _ST_VBO_H

#include "main/mtypes.h"

struct st_mesa_prim {
   GLuint mode:8;
   GLuint indexed:1;
   GLuint begin:1;
   GLuint end:1;
   GLuint weak:1;
   GLuint pad:20;

   GLuint start;
   GLuint count;
};

/* Would like to call this a "st_vbo_index_buffer", but this would be
 * confusing as the indices are not neccessarily yet in a non-null
 * buffer object.
 */
struct st_mesa_index_buffer {
   GLuint count;
   GLenum type;
   struct gl_buffer_object *obj;
   const void *ptr;
};



GLboolean _st_vbo_CreateContext( GLcontext *ctx );
void _st_vbo_DestroyContext( GLcontext *ctx );
void _st_vbo_InvalidateState( GLcontext *ctx, GLuint new_state );

typedef void (*st_vbo_draw_func)( GLcontext *ctx,
				  const struct gl_client_array **arrays,
				  const struct st_mesa_prim *prims,
				  GLuint nr_prims,
				  const struct st_mesa_index_buffer *ib,
				  GLuint min_index,
				  GLuint max_index );

void st_vbo_use_buffer_objects(GLcontext *ctx);

void st_vbo_set_draw_func(GLcontext *ctx, st_vbo_draw_func func);

void GLAPIENTRY
_st_vbo_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

void GLAPIENTRY
_st_vbo_Normal3f(GLfloat x, GLfloat y, GLfloat z);

void GLAPIENTRY
_st_vbo_MultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);

void GLAPIENTRY
_st_vbo_Materialfv(GLenum face, GLenum pname, const GLfloat *params);

void GLAPIENTRY
_st_vbo_VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

#endif
