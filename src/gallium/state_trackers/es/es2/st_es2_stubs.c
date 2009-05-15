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


/**
 * Temporary stubs for "missing" mesa functions referenced by
 * the ES 2.0 API veneer.
 */


#include "GLES/glplatform.h"
#include "st_platform.h"

#include "main/blend.h"
#include "main/bufferobj.h"
#include "main/fbobject.h"
#include "main/lines.h"
#include "main/shaders.h"
#include "main/texstate.h"
#include "main/varray.h"
#include "shader/arbprogram.h"


/** global symbol to identify the API to the EGL driver */
int st_api_OpenGL_ES2 = 1;


ST_IMPORT void ST_APIENTRY _vbo_VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);


/* These functions reflect critical functionality that is not
 * yet available in Mesa.  They're stubbed out here as Mesa
 * functions in order to build usable libraries that will work
 * if the function is not used.  As they're added to Mesa, they
 * should be removed from here.
 */
ST_IMPORT void ST_APIENTRY
_mesa_GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype,
                               GLint* range, GLint* precision);
ST_IMPORT void ST_APIENTRY
_mesa_GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype,
                               GLint* range, GLint* precision)
{
   assert(0);
}

ST_IMPORT void ST_APIENTRY
_mesa_ReleaseShaderCompiler(void);
ST_IMPORT void ST_APIENTRY
_mesa_ReleaseShaderCompiler(void)
{
   /* no-op */
}

ST_IMPORT void ST_APIENTRY
_mesa_ShaderBinary(GLint n, const GLuint* shaders, GLenum binaryformat,
                   const void* binary, GLint length);
ST_IMPORT void ST_APIENTRY
_mesa_ShaderBinary(GLint n, const GLuint* shaders, GLenum binaryformat,
                   const void* binary, GLint length)
{
   GET_CURRENT_CONTEXT(ctx);
   /* We'll always return 0 for GL_NUM_SHADER_BINARY_FORMATS so the
    * user should never call this functin.
    */
   _mesa_error(ctx, GL_INVALID_OPERATION, "glShaderBinary");
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib1f(GLuint indx, GLfloat x);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib1f(GLuint indx, GLfloat x)
{
   _vbo_VertexAttrib4f(indx, x, 0.0, 0.0, 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib1fv(GLuint indx, const GLfloat* values);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib1fv(GLuint indx, const GLfloat* values)
{
   _vbo_VertexAttrib4f(indx, values[0], 0.0, 0.0, 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y)
{
   _vbo_VertexAttrib4f(indx, x, y, 0.0, 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib2fv(GLuint indx, const GLfloat* values);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib2fv(GLuint indx, const GLfloat* values)
{
   _vbo_VertexAttrib4f(indx, values[0], values[1], 0.0, 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
   _vbo_VertexAttrib4f(indx, x, y, z, 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib3fv(GLuint indx, const GLfloat* values);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib3fv(GLuint indx, const GLfloat* values)
{
   _vbo_VertexAttrib4f(indx, values[0], values[1], values[2], 1.0f);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
   _vbo_VertexAttrib4f(indx, x, y, z, w);
}

ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib4fv(GLuint indx, const GLfloat* values);
ST_IMPORT void ST_APIENTRY
_mesa_VertexAttrib4fv(GLuint indx, const GLfloat* values)
{
   _vbo_VertexAttrib4f(indx, values[0], values[1], values[2], values[3]);
}
