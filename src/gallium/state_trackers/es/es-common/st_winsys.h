/**************************************************************************
 *
 * Copyright 2007-2008, Tungsten Graphics, Inc. All rights reserved.
 *
 **************************************************************************/

/**
 * @file
 * Public interface for the EGL/ES state tracker.
 *  
 * @author Jonathan White <jwhite@tungstengraphics.com>
 */

#ifndef ST_WINSYS_H
#define ST_WINSYS_H

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_winsys;
struct pipe_context;
struct pipe_winsys;
struct iegd_gdi_window;
struct ial_dev_priv;

/**
 * EGL/ES state tracker interface.
 * 
 * Gallium3D's EGL/ES state tracker interface
 */
struct st_winsys
{
   /** Create a pipe context */
   struct pipe_context * (*create_context)(struct pipe_screen *screen);

   /** Alloc iegd_gdi_window hold EGL NativeWindowType return EGLRect in window **/
   struct iegd_gdi_surface * (*alloc_window_surface)(struct win32_egl_surface *window);

   /** Free iegd_gdi_surface and related GDI Windows surfaces */
   void (*free_window_surface)(struct iegd_gdi_surface *gwnd);
};

#ifdef __cplusplus
}
#endif

#endif /* ST_WINSYS_H */
