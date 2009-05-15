/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
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
 * TUNGSTEN GRAPHICS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **************************************************************************/



#define GL_GLEXT_PROTOTYPES
#include <GLES/gl.h>
#include <GLES/glext.h>

#include "state_tracker/st_cb_drawtex.h"


#define FIXED_TO_FLOAT(X)  ((X) * (1.0f / 65535.0f))


extern void
_mesa_DrawTexf(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height);


GL_API void GL_APIENTRY
glDrawTexsOES (GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
   _mesa_DrawTexf((GLfloat) x, (GLfloat) y, (GLfloat) z,
                  (GLfloat) width, (GLfloat) height);
}


GL_API void GL_APIENTRY
glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height)
{
   _mesa_DrawTexf((GLfloat) x, (GLfloat) y, (GLfloat) z,
                  (GLfloat) width, (GLfloat) height);
}

GL_API void GL_APIENTRY
glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
   _mesa_DrawTexf(FIXED_TO_FLOAT(x), FIXED_TO_FLOAT(y), FIXED_TO_FLOAT(z),
                  FIXED_TO_FLOAT(width), FIXED_TO_FLOAT(height));
}

GL_API void GL_APIENTRY
glDrawTexsvOES(const GLshort *coords)
{
   _mesa_DrawTexf((GLfloat) coords[0], (GLfloat) coords[1],
                  (GLfloat) coords[2],
                  (GLfloat) coords[3], (GLfloat) coords[4]);
}

GL_API void GL_APIENTRY
glDrawTexivOES(const GLint *coords)
{
   _mesa_DrawTexf((GLfloat) coords[0], (GLfloat) coords[1],
                  (GLfloat) coords[2],
                  (GLfloat) coords[3], (GLfloat) coords[4]);
}

GL_API void GL_APIENTRY
glDrawTexxvOES(const GLfixed *coords)
{
   _mesa_DrawTexf(FIXED_TO_FLOAT(coords[0]),
                  FIXED_TO_FLOAT(coords[1]),
                  FIXED_TO_FLOAT(coords[2]),
                  FIXED_TO_FLOAT(coords[3]),
                  FIXED_TO_FLOAT(coords[4]));
}

GL_API void GL_APIENTRY
glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height)
{
   _mesa_DrawTexf(x, y, z, width, height);
}

GL_API void GL_APIENTRY
glDrawTexfvOES(const GLfloat *coords)
{
   _mesa_DrawTexf(coords[0], coords[1], coords[2], coords[3], coords[4]);
}
