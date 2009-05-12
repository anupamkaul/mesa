/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 **************************************************************************/


/**
 * Temporary stubs for "missing" mesa functions referenced by
 * the veneer.c file.
 */

#include <assert.h>

#include "GLES/glplatform.h"

#include "vbo/vbo.h"
#include "main/blend.h"
#include "main/context.h"
#include "main/fog.h"
#include "main/get.h"
#include "main/light.h"
#include "main/multisample.h"
#include "main/state.h"
#include "main/texstate.h"
#include "main/texenv.h"
#include "main/teximage.h"
#include "main/texparam.h"
#include "main/varray.h"
#include "st_cpaltex.h"



/*
 * typedefs/etc from GLES/gl.h here.  Can't include that header because
 * it generates tons of conflicts.
 */
typedef int             GLclampx;

#if FEATURE_accum
/* This is a sanity check that to be sure we're using the correct mfeatures.h
 * header.  We don't want to accidentally use the one from mainline Mesa.
 */
#error "The wrong mfeatures.h file is being included!"
#endif

/** global symbol to identify the API to the EGL driver */
int st_api_OpenGL_ES1 = 1;

/* _vbo_Materialf() doesn't exist, though _vbo_Materialfv() does... */
void GLAPIENTRY
_vbo_Materialfv(GLenum face, GLenum pname, const GLfloat * params);

GL_API void GLAPIENTRY
_vbo_Materialf(GLenum face, GLenum pname, GLfloat param)
{
    _vbo_Materialfv(face, pname, &param);
}

/* There's insufficient support in Mesa to allow a direct
 * query of GL_POINT_SIZE_ARRAY_POINTER_OES, although
 * the data is there.  This hack grabs the data directly in
 * that case, and goes through Mesa for all other cases.
 */
GL_API void GL_APIENTRY
glGetPointerv(GLenum pname, GLvoid **params)
{
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END(ctx);

   switch(pname) {
      case GL_POINT_SIZE_ARRAY_POINTER_OES:
         *params = (GLvoid *) ctx->Array.ArrayObj->PointSize.Ptr;
         break;
      case GL_COLOR_ARRAY_POINTER:
      case GL_NORMAL_ARRAY_POINTER:
      case GL_TEXTURE_COORD_ARRAY_POINTER:
      case GL_VERTEX_ARRAY_POINTER:
         _mesa_GetPointerv(pname, params);
         break;
      default:
         _mesa_error(ctx, GL_INVALID_ENUM, "GetPointerv");
         return;
   } 
}
