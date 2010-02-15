/**************************************************************************
 *
 * Copyright © 2010 Jakob Bornecrantz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#include "pipe/p_format.h"
#include "pipe/p_state.h"

#include "state_tracker/drm_api.h"
#include "state_tracker/xm_winsys.h"

#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "trace/tr_drm.h"

/*
 * XXX The sw_pipe_* code is generic and should be move out.
 *
 * This code wraps a pipe_screen and exposes a sw_callbacks interface
 * for use in software resterizers. This code is used by the DRM based
 * winsys to allow access to the drm driver.
 *
 * We must borrow the whole stack because only the pipe screen knows how
 * to decode the content of a buffer. Or how to create a buffer that
 * can still be used by drivers using real hardware (as the case is
 * with software st/xorg but hw st/dri).
 */

struct sw_pipe_callbacks
{
   struct sw_callbacks base;
   struct pipe_screen *screen;
};

struct sw_pipe_displatarget
{
   struct pipe_texture *tex;
   struct pipe_transfer *transfer;
};

static INLINE struct sw_pipe_callbacks *
sw_pipe_callbacks(struct sw_callbacks *swc)
{
   return (struct sw_pipe_callbacks *)swc;
}

static INLINE struct pipe_texture *
sw_pipe_dt_get_texture(struct sw_displaytarget *dt)
{
   struct sw_pipe_displatarget *swpdt = (struct sw_pipe_displatarget *)dt;
   return swpdt->tex;
}

static INLINE struct sw_displaytarget *
sw_pipe_dt_wrap_texture(struct pipe_texture *tex)
{
   struct sw_pipe_displatarget *swpdt = CALLOC_STRUCT(sw_pipe_displatarget);
   swpdt->tex = tex;
   return (struct sw_displaytarget *)swpdt;
}

static struct sw_displaytarget *
swpc_dt_create(struct sw_callbacks *swc,
               enum pipe_format format,
               unsigned width, unsigned height,
               unsigned alignment,
               unsigned *stride)
{
   struct sw_pipe_callbacks *swpc = sw_pipe_callbacks(swc);
   struct pipe_texture templ;
   struct pipe_texture *tex;
   memset(&templ, 0, sizeof(templ));

   templ->width0 = width;
   templ->height0 = height;
   templ->format = format;
   /*
    * XXX How do we tell the difference between displaytargets and primary (scanout)?
    * Aslo should all be rendertargets?
    * What about depthstencil?
    * XXX we pretty much need get usage passed down, maybe even the template.
    */
   templ->usage = 0;
   /* XXX alignment isn't needed for DRM platform */
   /* XXX stride isn't needed for DRM platform */

   tex = swpc->screen->texture_create(swpc->screen, &templ);

   return sw_pipe_dt_wrap_texture(tex);
}

static void *
swpc_dt_map(struct sw_callbacks *ws, 
            struct sw_displaytarget *dt,
            unsigned flags)
{
   /* XXX use transfer */
   return NULL;
}

static void
swpc_dt_unmap(struct sw_callbacks *ws,
              struct sw_displaytarget *dt)
{
   /* XXX unmap transfer */
}

static void 
swpc_dt_destroy(struct sw_callbacks *ws, 
                struct sw_displaytarget *dt)
{
   struct sw_pipe_displatarget *swpdt = (struct sw_pipe_displatarget *)dt;

   pipe_texture_reference(&swpdt->tex, NULL);

   FREE(swpdt);
}

static void
swpc_destroy(struct sw_callbacks *swc)
{
   struct sw_pipe_callbacks *swpc = sw_pipe_callbacks(swc);

   swpc->screen->destroy(swpc->screen);

   FREE(swpc);
}

static struct sw_callbacks *
sw_callbacks_warp_pipe_screen(struct pipe_screen *pipe)
{
   struct sw_pipe_callbacks *swpc = CALLOC_STRUCT(sw_pipe_callbacks);

   swpc->base.displaytarget_create = swpc_dt_create;
   swpc->base.displaytarget_map = swpc_dt_map;
   swpc->base.displaytarget_unmap = swpc_dt_unmap;
   swpc->base.displaytarget_destroy = swpc_dt_destroy;
   swpc->base.destroy = swpc_destroy;

   return &swpc->base;
}


/*
 * XXX Should be moved out into header file.
 */

struct drm_api * sw_drm_api_create(struct drm_api *api);


/*
 * Defines
 */

struct sw_drm_api
{
   struct drm_api base;
   struct drm_api *api;
   struct sw_driver *sw;
};

static INLINE struct sw_drm_api *
sw_drm_api(struct drm_api *api)
{
   return (struct sw_drm_api *)api;
}


/*
 * Exported functions
 */

static struct pipe_texture *
sw_drm_texture_from_shared_handle(struct drm_api *_api,
                                  struct pipe_screen *_screen,
                                  struct pipe_texture *templ,
                                  const char* name,
                                  unsigned pitch,
                                  unsigned handle)
{
   struct sw_drm_api *swapi = sw_drm_api(_api);
   struct drm_api *api = swapi->api;
   struct pipe_screen *screen = NULL/* XXX GET? */;
   struct sw_displaytarget *dt;
   struct pipe_texture *tex;

   tex = api->texture_from_shared_handle(api, screen, templ, name, pitch, handle);
   dt = sw_pipe_dt_wrap_texture(tex);

   return swapi->sw->wrap_displaytarget(swapi->sw, _screen, templ, dt);
}

static boolean
sw_drm_shared_handle_from_texture(struct drm_api *_api,
                                  struct pipe_screen *_screen,
                                  struct pipe_texture *_tex,
                                  unsigned *pitch,
                                  unsigned *handle)
{
   struct sw_drm_api *swapi = sw_drm_api(_api);
   struct drm_api *api = swapi->api;
   struct sw_displaytarget *dt = swapi->sw->get_displaytarget(swapi->sw, _tex);
   struct pipe_texture *tex = sw_pipe_dt_get_texture(dt);
   struct pipe_screen *screen = tex->screen;

   return api->shared_handle_from_texture(api, screen, tex, pitch, handle);
}

static boolean
sw_drm_local_handle_from_texture(struct drm_api *_api,
                                 struct pipe_screen *_screen,
                                 struct pipe_texture *_tex,
                                 unsigned *pitch,
                                 unsigned *handle)
{
   struct sw_drm_api *swapi = sw_drm_api(_api);
   struct drm_api *api = swapi->api;
   struct sw_displaytarget *dt = swapi->sw->get_displaytarget(swapi->sw, _tex);
   struct pipe_texture *tex = sw_pipe_dt_get_texture(dt);
   struct pipe_screen *screen = tex->screen;

   return api->local_handle_from_texture(api, screen, tex, pitch, handle);
}

static struct pipe_screen *
sw_drm_create_screen(struct drm_api *_api, int drmFD,
                     struct drm_create_screen_arg *arg)
{
   struct sw_drm_api *swapi = sw_drm_api(_api);
   struct drm_api *api = swapi->api;
   struct sw_callbacks *swc;
   struct pipe_screen *screen;

   screen = api->create_screen(api, drmFD, arg);

   swc = sw_callbacks_warp_pipe_screen(screen);

   return swapi->sw->create_screen(swapi->sw, swc);
}

static void
sw_drm_destroy(struct drm_api *api)
{
   struct sw_drm_api *swapi = sw_drm_api(api);
   if (swapi->api->destroy)
      swapi->api->destroy(swapi->api);

   FREE(swapi);
}

struct drm_api *
sw_drm_api_create(struct drm_api *api)
{
   struct sw_drm_api *swapi = CALLOC_STRUCT(sw_drm_api);

   swapi->base.name = "sw";
   swapi->base.driver_name = api->driver_name;
   swapi->base.create_screen = sw_drm_create_screen;
   swapi->base.texture_from_shared_handle = sw_drm_texture_from_shared_handle;
   swapi->base.shared_handle_from_texture = sw_drm_shared_handle_from_texture;
   swapi->base.local_handle_from_texture = sw_drm_local_handle_from_texture;
   swapi->base.destroy = sw_drm_destroy;

   return trace_drm_create(&swapi->base);
}
