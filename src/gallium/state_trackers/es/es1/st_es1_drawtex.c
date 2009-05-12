/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
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
