/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.
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
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "state_tracker/xm_winsys.h"

#include "util/u_memory.h"
#include "trace/tr_xm.h"
#include "trace/tr_screen.h"
#include "trace/tr_context.h"
#include "trace/tr_buffer.h"
#include "trace/tr_texture.h"

struct trace_xm_driver
{
   struct xm_driver base;

   struct xm_driver *driver;
};

static INLINE struct trace_xm_driver *
trace_xm_driver(struct xm_driver *_driver)
{
   return (struct trace_xm_driver *)_driver;
}

static struct pipe_screen *
trace_xm_create_screen(struct xm_driver *_driver)
{
   struct trace_xm_driver *tr_driver = trace_xm_driver(_driver);
   struct xm_driver *driver = tr_driver->driver;
   struct pipe_screen *screen;

   /* TODO trace call */

   screen = driver->create_screen(driver);

   return trace_screen_create(screen);
}


static void
trace_xm_display_surface(struct xm_driver *_driver,
			 struct xmesa_buffer *buffer,
			 struct pipe_surface *_surface)
{
   struct trace_xm_driver *tr_driver = trace_xm_driver(_driver);
   struct trace_surface *tr_surface = trace_surface(_surface);
   struct xm_driver *driver = tr_driver->driver;
   struct pipe_surface *surface = tr_surface->surface;

   /* TODO trace call */
   
   driver->display_surface(driver, buffer, surface);
}


static void
trace_xm_destroy(struct xm_driver *_driver)
{
   struct trace_xm_driver *tr_driver = trace_xm_driver(_driver);
   struct xm_driver *driver = tr_driver->driver;

   if (driver->destroy)
      driver->destroy(driver);

   free(tr_driver);
}

struct xm_driver *
trace_xm_create(struct xm_driver *driver)
{
   struct trace_xm_driver *tr_driver;

   if (!driver)
      goto error;

   if (!trace_enabled())
      goto error;

   tr_driver = CALLOC_STRUCT(trace_xm_driver);

   if (!tr_driver)
      goto error;

   tr_driver->base.create_screen = trace_xm_create_screen;
   tr_driver->base.display_surface = trace_xm_display_surface;
   tr_driver->base.destroy = trace_xm_destroy;
   tr_driver->driver = driver;

   return &tr_driver->base;

error:
   return driver;
}
