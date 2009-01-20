/**************************************************************************
 * Copyright Tungsten Graphics, Inc., Cedar Park, Tx. , USA
 * All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "main/context.h"
#include "main/fbobject.h"
#include "main/texformat.h"
#include "main/renderbuffer.h"

#include "via_fbo.h"
#include "via_span.h"

/**
 * Get row of values from the renderbuffer that wraps a texture image.
 */

static void
texture_get_row(GLcontext * ctx, struct gl_renderbuffer *rb, GLuint count,
		GLint x, GLint y, void *values)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    ASSERT(texImage->Width == rb->Width);
    ASSERT(texImage->Height == rb->Height);

    if (rb->DataType == CHAN_TYPE) {
	GLchan *rgbaOut = (GLchan *) values;

	for (i = 0; i < count; i++) {
	    texImage->FetchTexelc(texImage, x + i, y, z, rgbaOut + 4 * i);
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	GLuint *zValues = (GLuint *) values;

	/*
	 * const GLdouble scale = (GLdouble) 0xffffffff;
	 */
	for (i = 0; i < count; i++) {
	    GLfloat flt;

	    texImage->FetchTexelf(texImage, x + i, y, z, &flt);
	    zValues[i] = ((GLuint) (flt * 0xffffff)) << 8;
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	GLuint *zValues = (GLuint *) values;

	for (i = 0; i < count; i++) {
	    GLfloat flt;

	    texImage->FetchTexelf(texImage, x + i, y, z, &flt);
	    zValues[i] = ((GLuint) (flt * 0xffffff)) << 8;
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_get_row");
    }
}

static void
texture_get_values(GLcontext * ctx, struct gl_renderbuffer *rb, GLuint count,
		   const GLint x[], const GLint y[], void *values)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    if (rb->DataType == CHAN_TYPE) {
	GLchan *rgbaOut = (GLchan *) values;

	for (i = 0; i < count; i++) {
	    texImage->FetchTexelc(texImage, x[i], y[i], z, rgbaOut + 4 * i);
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	GLuint *zValues = (GLuint *) values;

	for (i = 0; i < count; i++) {
	    GLfloat flt;

	    texImage->FetchTexelf(texImage, x[i], y[i], z, &flt);
	    zValues[i] = ((GLuint) (flt * 0xffffff)) << 8;
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	GLuint *zValues = (GLuint *) values;

	for (i = 0; i < count; i++) {
	    GLfloat flt;

	    texImage->FetchTexelf(texImage, x[i], y[i], z, &flt);
	    zValues[i] = ((GLuint) (flt * 0xffffff)) << 8;
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_get_values");
    }
}

/**
 * Put row of values into a renderbuffer that wraps a texture image.
 */
static void
texture_put_row(GLcontext * ctx, struct gl_renderbuffer *rb, GLuint count,
		GLint x, GLint y, const void *values, const GLubyte * mask)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    if (rb->DataType == CHAN_TYPE) {
	const GLchan *rgba = (const GLchan *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x + i, y, z, rgba);
	    }
	    rgba += 4;
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	const GLuint *zValues = (const GLuint *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x + i, y, z, zValues + i);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	const GLuint *zValues = (const GLuint *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		GLfloat flt = (zValues[i] >> 8) * (1.0 / 0xffffff);

		viarb->Store(texImage, x + i, y, z, &flt);
	    }
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_put_row");
    }
}

static void
texture_put_mono_row(GLcontext * ctx, struct gl_renderbuffer *rb,
		     GLuint count, GLint x, GLint y, const void *value,
		     const GLubyte * mask)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    if (rb->DataType == CHAN_TYPE) {
	const GLchan *rgba = (const GLchan *)value;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x + i, y, z, rgba);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	const GLuint zValue = *((const GLuint *)value);

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x + i, y, z, &zValue);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	const GLuint zValue = *((const GLuint *)value);
	const GLfloat flt = (zValue >> 8) * (1.0 / 0xffffff);

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x + i, y, z, &flt);
	    }
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_put_mono_row");
    }
}

static void
texture_put_values(GLcontext * ctx, struct gl_renderbuffer *rb, GLuint count,
		   const GLint x[], const GLint y[], const void *values,
		   const GLubyte * mask)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    if (rb->DataType == CHAN_TYPE) {
	const GLchan *rgba = (const GLchan *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x[i], y[i], z, rgba);
	    }
	    rgba += 4;
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	const GLuint *zValues = (const GLuint *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x[i], y[i], z, zValues + i);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	const GLuint *zValues = (const GLuint *)values;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		GLfloat flt = (zValues[i] >> 8) * (1.0 / 0xffffff);

		viarb->Store(texImage, x[i], y[i], z, &flt);
	    }
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_put_values");
    }
}

static void
texture_put_mono_values(GLcontext * ctx, struct gl_renderbuffer *rb,
			GLuint count, const GLint x[], const GLint y[],
			const void *value, const GLubyte * mask)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    struct gl_texture_image *texImage = &viarb->viaImage.image;
    const GLint z = viarb->zOffset;
    GLuint i;

    if (rb->DataType == CHAN_TYPE) {
	const GLchan *rgba = (const GLchan *)value;

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x[i], y[i], z, rgba);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT) {
	const GLuint zValue = *((const GLuint *)value);

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x[i], y[i], z, &zValue);
	    }
	}
    } else if (rb->DataType == GL_UNSIGNED_INT_24_8_EXT) {
	const GLuint zValue = *((const GLuint *)value);
	const GLfloat flt = (zValue >> 8) * (1.0 / 0xffffff);

	for (i = 0; i < count; i++) {
	    if (!mask || mask[i]) {
		viarb->Store(texImage, x[i], y[i], z, &flt);
	    }
	}
    } else {
	_mesa_problem(ctx, "invalid rb->DataType in texture_put_mono_values");
    }
}

void
via_set_texture_span_functions(struct gl_renderbuffer *rb)
{
    rb->GetRow = texture_get_row;
    rb->GetValues = texture_get_values;
    rb->PutRow = texture_put_row;
    rb->PutMonoRow = texture_put_mono_row;
    rb->PutValues = texture_put_values;
    rb->PutMonoValues = texture_put_mono_values;
}
