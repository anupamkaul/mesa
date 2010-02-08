
/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#ifndef XM_WINSYS_H
#define XM_WINSYS_H

struct pipe_context;
struct pipe_screen;
struct pipe_surface;


/* XXX: remove the xmesa_buffer concept from this interface
 */
struct xmesa_buffer;


struct xm_driver {

   struct pipe_screen *(*create_screen)( struct xm_driver *driver );

   void (*display_surface)( struct xm_driver *driver,
                            struct xmesa_buffer *, /* XXX: remove me! */
                            struct pipe_surface * );

   void (*destroy)( struct xm_driver *driver );

};


/* Currently called by the driver/winsys to register an xm_driver.
 * Note that this is the opposite usage to drm_api.h/drm_api_create(),
 * which is called by the state tracker.
 */
extern void
xmesa_set_driver( struct xm_driver *driver );


#endif
