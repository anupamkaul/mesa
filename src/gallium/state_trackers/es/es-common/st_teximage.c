/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 **************************************************************************/


/**
 * Special ES texture image functions, for both ES1 and ES2.
 */


#include "GLES/glplatform.h"

//#include "main/teximage.h"  // Don't include these because one of them pulls in gl.h
//#include "st_cpaltex.h"



/*
 * typedefs/etc from GLES/gl.h here.  Can't include that header because
 * it generates tons of conflicts.
 */

#ifndef GL_OES_compressed_paletted_texture

#define GL_PALETTE4_RGB8_OES                                    0x8B90
#define GL_PALETTE4_RGBA8_OES                                   0x8B91
#define GL_PALETTE4_R5_G6_B5_OES                                0x8B92
#define GL_PALETTE4_RGBA4_OES                                   0x8B93
#define GL_PALETTE4_RGB5_A1_OES                                 0x8B94
#define GL_PALETTE8_RGB8_OES                                    0x8B95
#define GL_PALETTE8_RGBA8_OES                                   0x8B96
#define GL_PALETTE8_R5_G6_B5_OES                                0x8B97
#define GL_PALETTE8_RGBA4_OES                                   0x8B98
#define GL_PALETTE8_RGB5_A1_OES                                 0x8B99
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef void GLvoid;
typedef int GLintptr;
typedef int GLsizeiptr;
typedef int GLfixed;
typedef int GLclampx;
/* Internal convenience typedefs */
typedef void (*_GLfuncptr)();

void GL_APIENTRY
_mesa_CompressedTexImage2DARB(GLenum target, GLint level,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, GLint border, GLsizei imageSize,
                              const GLvoid *data);

GL_API void GL_APIENTRY
glCompressedTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                       GLsizei width, GLsizei height, GLint border,
                       GLsizei imageSize, const GLvoid *data)
{
   switch (internalFormat) {
   case GL_PALETTE4_RGB8_OES:
   case GL_PALETTE4_RGBA8_OES:
   case GL_PALETTE4_R5_G6_B5_OES:
   case GL_PALETTE4_RGBA4_OES:
   case GL_PALETTE4_RGB5_A1_OES:
   case GL_PALETTE8_RGB8_OES:
   case GL_PALETTE8_RGBA8_OES:
   case GL_PALETTE8_R5_G6_B5_OES:
   case GL_PALETTE8_RGBA4_OES:
   case GL_PALETTE8_RGB5_A1_OES:
      cpal_compressed_teximage2d(target, level, internalFormat,
                                 width, height, data);
      break;
   default:
      _mesa_CompressedTexImage2DARB(target, level, internalFormat,
                                    width, height, border, imageSize, data);
   }
}

