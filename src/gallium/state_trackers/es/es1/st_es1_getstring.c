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


#include <stdlib.h>
#include "GLES/gl.h"
#include "st_platform.h"


ST_IMPORT const GLubyte * ST_APIENTRY
_mesa_GetString( GLenum name );


/**
 * Note: a more dynamic solution would be query the underlying GL
 * and translate extension names...
 */
static const char *
extension_string(void)
{
   return 
      /* Core additions */
      "GL_OES_byte_coordinates "
      "GL_OES_fixed_point "
      "GL_OES_single_precision "
      "GL_OES_matrix_get "

      /* 1.1 required extensions */
      "GL_OES_read_format "
      "GL_OES_compressed_paletted_texture "
      "GL_OES_point_size_array "
      "GL_OES_point_sprite "

      /* 1.1 optional extensions */
      "GL_OES_draw_texture "

      /* 1.1 deprecated extensions */
      "GL_OES_query_matrix "

      /* Newer extensions */
      "GL_OES_blend_equation_separate "
      "GL_OES_blend_func_separate "
      "GL_OES_blend_subtract "
      "GL_OES_depth24 "
      "GL_OES_depth32 "
      "GL_OES_element_index_uint "
      "GL_OES_fbo_render_mipmap "
      "GL_OES_framebuffer_object "
      "GL_OES_mapbuffer "
      "GL_OES_rgb8_rgba8 "
      "GL_OES_stencil1 "
      "GL_OES_stencil4 "
      "GL_OES_stencil8 "
      "GL_OES_texture_cube_map "
      "GL_OES_texture_env_crossbar "
      "GL_OES_texture_mirrored_repeat "
      "GL_EXT_texture_filter_anisotropic "
      ;
}



GL_API const GLubyte * GL_APIENTRY
glGetString(GLenum name)
{
   switch (name) {
   case GL_VERSION:
      return (GLubyte *) "OpenGL ES-CM 1.1";
   case GL_VENDOR:
      return (GLubyte *) "Tungsten Graphics, Inc.";
   case GL_EXTENSIONS:
      return (GLubyte *) extension_string();
   default:
      return _mesa_GetString(name);
   }
}
