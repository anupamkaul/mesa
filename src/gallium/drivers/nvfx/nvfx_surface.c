
/**************************************************************************
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "nvfx_context.h"
#include "nvfx_resource.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_pack_color.h"

static void
nvfx_surface_copy(struct pipe_context *pipe,
		  struct pipe_resource *dest, unsigned dst_level,
		  unsigned destx, unsigned desty, unsigned destz,
		  struct pipe_resource *src, unsigned src_level,
		  const struct pipe_box *src_box)
{
	struct nvfx_context *nvfx = nvfx_context(pipe);
	struct nv04_surface_2d *eng2d = nvfx->screen->eng2d;
	struct pipe_surface *ps_dst, *ps_src, surf_tmpl;

	assert(src_box->depth == 1);

	memset(&surf_tmpl, 0, sizeof(surf_tmpl));
	surf_tmpl.format = src->format;
	surf_tmpl.usage = 0; /* no bind flag - not a surface */
	surf_tmpl.u.tex.level = src_level;
	surf_tmpl.u.tex.first_layer = src_box->z;
	surf_tmpl.u.tex.last_layer = src_box->z;
	/* XXX really need surfaces here? */
	ps_src = nvfx_miptree_surface_new(pipe, src, &surf_tmpl);
	surf_tmpl.format = dest->format;
	surf_tmpl.usage = 0; /* no bind flag - not a surface */
	surf_tmpl.u.tex.level = dst_level;
	surf_tmpl.u.tex.first_layer = destz;
	surf_tmpl.u.tex.last_layer = destz;
	ps_dst = nvfx_miptree_surface_new(pipe, dest, &surf_tmpl);

	eng2d->copy(eng2d, ps_dst, destx, desty, ps_src, src_box->x, src_box->y,
		    src_box->width, src_box->height);

	nvfx_miptree_surface_del(pipe, ps_src);
	nvfx_miptree_surface_del(pipe, ps_dst);
}

static void
nvfx_clear_render_target(struct pipe_context *pipe,
			 struct pipe_surface *dst,
			 const float *rgba,
			 unsigned dstx, unsigned dsty,
			 unsigned width, unsigned height)
{
	struct nvfx_context *nvfx = nvfx_context(pipe);
	struct nv04_surface_2d *eng2d = nvfx->screen->eng2d;
	union util_color uc;
	util_pack_color(rgba, dst->format, &uc);

	eng2d->fill(eng2d, dst, dstx, dsty, width, height, uc.ui);
}

static void
nvfx_clear_depth_stencil(struct pipe_context *pipe,
			 struct pipe_surface *dst,
			 unsigned clear_flags,
			 double depth,
			 unsigned stencil,
			 unsigned dstx, unsigned dsty,
			 unsigned width, unsigned height)
{
	struct nvfx_context *nvfx = nvfx_context(pipe);
	struct nv04_surface_2d *eng2d = nvfx->screen->eng2d;

	eng2d->fill(eng2d, dst, dstx, dsty, width, height,
		    util_pack_z_stencil(dst->format, depth, stencil));
}


void
nvfx_init_surface_functions(struct nvfx_context *nvfx)
{
	nvfx->pipe.resource_copy_region = nvfx_surface_copy;
	nvfx->pipe.clear_render_target = nvfx_clear_render_target;
	nvfx->pipe.clear_depth_stencil = nvfx_clear_depth_stencil;
}
