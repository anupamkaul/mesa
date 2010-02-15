
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

#ifndef SW_WINSYS_H
#define SW_WINSYS_H

struct pipe_context;
struct pipe_screen;
struct pipe_surface;


/* The drm driver stack have two different users st/dri and st/xorg.
 *
 * st/xorg produces both scanouts (primary) and shared (displaytarget)
 * textures via the normal pipe_screen::create_texture() callback,
 * this follows how the lp_winsys works. However the lp_winsys
 * does not deal with the difference between scanouts and displaytargets
 * something that needs to be added. st/xorg then uses the drm_api to
 * access the handles to textures.
 *
 * st/dri creates texture via the drm api.
 *
 *
 * With all of the above requirements the drm API needs to be aware that
 * a software rasterizer has been layered ontop of it. However this code
 * can be completely generic and reused for all drm winsys.
 */

/*
 * In the software drivers, we would also like the co-state tracker to
 * be involved in creating the backing for scanout textures.  This
 * allows the knowledge of eg. XShm to be collapsed down to a single
 * location, and permits a null or at least tiny software rasterizer
 * winsys.
 *
 * The main question is whether to follow the approach of the st/dri
 * state tracker and ask the driver to turn some pre-existing storage
 * into a texture, or to stay closer to the lp_winsys approach and
 * have the provide the driver with a set of callbacks allowing it to
 * request such storage be created - effectively lifting the provision
 * of an lp_winsys into the co-state tracker's responsibilities.
 *
 * In either case, we still want to have an "sw_driver" abstraction,
 * permitting multiple software rasterizers to be handled by generic
 * code in the state tracker, and removing the need for separate
 * handling of softpipe, llvmpipe and perhaps also cell.
 */

/**
 * Opaque pointer.
 */
struct sw_displaytarget;


/**
 * This is the interface that sw expects any window system
 * hosting it to implement.
 * 
 * sw is for the most part a self sufficient driver. The only thing it
 * does not know is how to display a surface.
 */
struct sw_callbacks
{
   void 
   (*destroy)( struct sw_callbacks *ws );

   /* No need to query what formats are supported.  As the
    * implementation of this interface is provided by the state
    * tracker, it will already know what formats it supports and will
    * not try to created scanout textures in other formats.  The
    * co-state tracker may still need to query what formats the driver
    * can render to, but that does not require a callback in this
    * struct.
    */
   
   /**
    * Allocate storage for a render target.
    * 
    * Often surfaces which are meant to be blitted to the front screen (i.e.,
    * display targets) must be allocated with special characteristics, memory 
    * pools, or obtained directly from the windowing system.
    *  
    * This callback is invoked by the pipe_screen when creating a texture marked
    * with the PIPE_TEXTURE_USAGE_DISPLAY_TARGET flag to get the underlying 
    * storage.
    */
   struct sw_displaytarget *
   (*displaytarget_create)( struct sw_callbacks *ws,
                            enum pipe_format format,
                            unsigned width, unsigned height,
                            unsigned alignment,
                            unsigned *stride );

   void *
   (*displaytarget_map)( struct sw_callbacks *ws, 
                         struct sw_displaytarget *dt,
                         unsigned flags );

   void
   (*displaytarget_unmap)( struct sw_callbacks *ws,
                           struct sw_displaytarget *dt );

   void 
   (*displaytarget_destroy)( struct sw_callbacks *ws, 
                             struct sw_displaytarget *dt );


   /* No displaytarget_display callback -- the co state trackers now
    * universally override pipe_screen::flush_frontbuffer() with their
    * own code, and that call will at some point be turned to a direct
    * state-tracker to co-state-tracker interface.
    */
};


struct sw_driver {

   struct pipe_screen *(*create_screen)( struct sw_driver *driver,
					 struct sw_callbacks *callbacks );

   struct pipe_texture *(*wrap_displaytarget)( struct sw_driver *driver,
                                               struct pipe_screen *screen,
                                               struct pipe_texture *templ,
                                               struct sw_displaytarget *dt );

   struct sw_displaytarget *(*get_displaytarget)( struct sw_driver *driver,
                                                  struct pipe_texture *tex );

   /* No call to wrap a display target and create a texture.  Hope
    * that the callback mechanism is sufficient for now.
    */

   void (*destroy)( struct sw_driver *driver );

};


/* Currently called by the driver/winsys to register an sw_driver.
 * Note that this is the opposite usage to drm_api.h/drm_api_create(),
 * which is called by the state tracker.
 */
extern void
st_register_sw_driver( struct sw_driver *driver );


#endif
