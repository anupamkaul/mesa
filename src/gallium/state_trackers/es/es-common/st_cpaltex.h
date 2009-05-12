/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 **************************************************************************/


#ifndef ST_CPALTEX_H
#define ST_CPALTEX_H


extern void
cpal_compressed_teximage2d(GLenum target, GLint level,
                           GLenum internalFormat,
                           GLsizei width, GLsizei height,
                           const void *pixels);


#endif /* ST_CPALTEX_H */
