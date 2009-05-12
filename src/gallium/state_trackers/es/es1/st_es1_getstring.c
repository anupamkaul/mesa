/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
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
