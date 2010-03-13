/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#include "svga_cmd.h"

#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "os/os_thread.h"
#include "util/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"

#include "svga_screen.h"
#include "svga_context.h"
#include "svga_screen_texture.h"
#include "svga_screen_buffer.h"
#include "svga_winsys.h"
#include "svga_debug.h"
#include "svga_screen_buffer.h"

#include <util/u_string.h>

/* Use the pipe resource_manager module to implemen the new-style
 * driver functions in terms of the old ones we have already.
 */
static struct pipe_resource *
svga_resource_create(struct pipe_screen *screen,
		     const struct pipe_resource *template)
{
   struct svga_screen *ss = svga_screen(screen);
   return rm_resource_create(ss->rm_screen,
			     template);
}

static struct pipe_resource *
svga_resource_from_handle(struct pipe_screen *screen,
			  const struct pipe_resource *template,
			  struct winsys_handle *handle)
{
   struct svga_screen *ss = svga_screen(screen);
   return rm_resource_from_handle(ss->rm_screen,
				  template,
				  handle);
}

boolean svga_resource_get_handle(struct pipe_screen *screen,
			       struct pipe_resource *resource,
			       struct winsys_handle *handle)
{
   struct svga_screen *ss = svga_screen(screen);
   return rm_resource_get_handle(ss->rm_screen,
				 resource,
				 handle);
}


void svga_resource_destroy(struct pipe_screen *screen,
			 struct pipe_resource *resource)
{
   struct svga_screen *ss = svga_screen(screen);
   rm_resource_destroy(ss->rm_screen,
		       resource);
   
}

struct pipe_transfer *svga_get_transfer(struct pipe_context *pipe,
					struct pipe_resource *resource,
					struct pipe_subresource sr,
					enum pipe_transfer_usage usage,
					const struct pipe_box *box)
{
   struct svga_context *sc = svga_context(pipe);
   return rm_get_transfer(sc->rm_context,
			  resource,
			  sr,
			  usage,
			  box);
}

void svga_transfer_destroy(struct pipe_context *pipe,
			   struct pipe_transfer *transfer)
{
   struct svga_context *sc = svga_context(pipe);
   rm_transfer_destroy(sc->rm_context,
		       transfer);
}

void *svga_transfer_map( struct pipe_context *pipe,
			 struct pipe_transfer *transfer )
{
   struct svga_context *sc = svga_context(pipe);
   return rm_transfer_map(sc->rm_context,
			  transfer);
}

void svga_transfer_flush_region( struct pipe_context *pipe,
				 struct pipe_transfer *transfer,
				 const struct pipe_box *box )
{
   struct svga_context *sc = svga_context(pipe);
   rm_transfer_flush_region(sc->rm_context,
			    transfer,
			    box);
}

void svga_transfer_unmap( struct pipe_context *pipe,
			  struct pipe_transfer *transfer )
{
   struct svga_context *sc = svga_context(pipe);
   rm_transfer_unmap(sc->rm_context,
		     transfer);
}


void svga_transfer_inline_write( struct pipe_context *pipe,
				 struct pipe_resource *resource,
				 struct pipe_subresource sr,
				 enum pipe_transfer_usage usage,
				 const struct pipe_box *box,
				 const void *data )
{
   struct svga_context *sc = svga_context(pipe);
   rm_transfer_inline_write(sc->rm_context,
			    resource,
			    sr,
			    usage,
			    box,
			    data);
}

void svga_transfer_inline_read( struct pipe_context *pipe,
				struct pipe_resource *resource,
				struct pipe_subresource sr,
				enum pipe_transfer_usage usage,
				const struct pipe_box *box,
				void *data )
{
   struct svga_context *sc = svga_context(pipe);
   rm_transfer_inline_read(sc->rm_context,
			   resource,
			   sr,
			   usage,
			   box,
			   data);
}



void
svga_init_resource_functions(struct pipe_context *pipe)
{
   pipe->get_transfer = svga_get_transfer;
   pipe->transfer_map = svga_transfer_map;
   pipe->transfer_unmap = svga_transfer_unmap;
   pipe->transfer_destroy = svga_transfer_destroy;
}


void
svga_screen_init_resource_functions(struct pipe_screen *screen)
{
   screen->texture_create = svga_texture_create;
   screen->texture_from_handle = svga_screen_texture_from_handle;
   screen->texture_get_handle = svga_screen_texture_get_handle;
   screen->texture_destroy = svga_texture_destroy;
   screen->get_tex_surface = svga_get_tex_surface;
   screen->tex_surface_destroy = svga_tex_surface_destroy;
}
