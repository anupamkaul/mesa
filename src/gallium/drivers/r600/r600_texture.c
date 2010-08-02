/*
 * Copyright 2010 Jerome Glisse <glisse@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *      Jerome Glisse
 *      Corbin Simpson
 */
#include <pipe/p_screen.h>
#include <util/u_format.h>
#include <util/u_math.h>
#include <util/u_inlines.h>
#include <util/u_memory.h>
#include "state_tracker/drm_driver.h"
#include "r600_screen.h"
#include "r600_context.h"
#include "r600_resource.h"

extern struct u_resource_vtbl r600_texture_vtbl;

static unsigned long r600_texture_get_offset(struct r600_resource_texture *rtex,
					unsigned level, unsigned layer)
{
	unsigned long offset = rtex->offset[level];

	switch (rtex->resource.base.b.target) {
	case PIPE_TEXTURE_3D:
	case PIPE_TEXTURE_CUBE:
		return offset + layer * rtex->layer_size[level];
	default:
		assert(layer == 0);
		return offset;
	}
}

static void r600_setup_miptree(struct r600_screen *rscreen, struct r600_resource_texture *rtex)
{
	struct pipe_resource *ptex = &rtex->resource.base.b;
	unsigned long w, h, pitch, size, layer_size, i, offset;

	rtex->bpt = util_format_get_blocksize(ptex->format);
	for (i = 0, offset = 0; i <= ptex->last_level; i++) {
		w = u_minify(ptex->width0, i);
		h = u_minify(ptex->height0, i);
		pitch = util_format_get_stride(ptex->format, align(w, 64));
		layer_size = pitch * h;
		if (ptex->target == PIPE_TEXTURE_CUBE)
			size = layer_size * 6;
		else
			size = layer_size * u_minify(ptex->depth0, i);
		rtex->offset[i] = offset;
		rtex->layer_size[i] = layer_size;
		rtex->pitch[i] = pitch;
		offset += size;
	}
	rtex->size = offset;
}

struct pipe_resource *r600_texture_create(struct pipe_screen *screen,
						const struct pipe_resource *templ)
{
	struct r600_resource_texture *rtex;
	struct r600_resource *resource;
	struct r600_screen *rscreen = r600_screen(screen);

	rtex = CALLOC_STRUCT(r600_resource_texture);
	if (!rtex) {
		return NULL;
	}
	resource = &rtex->resource;
	resource->base.b = *templ;
	resource->base.vtbl = &r600_texture_vtbl;
	pipe_reference_init(&resource->base.b.reference, 1);
	resource->base.b.screen = screen;
	r600_setup_miptree(rscreen, rtex);

	/* FIXME alignment 4096 enought ? too much ? */
	resource->domain = r600_domain_from_usage(resource->base.b.bind);
	resource->bo = radeon_bo(rscreen->rw, 0, rtex->size, 4096, NULL);
	if (resource->bo == NULL) {
		FREE(rtex);
		return NULL;
	}

	return &resource->base.b;
}

static void r600_texture_destroy(struct pipe_screen *screen,
				 struct pipe_resource *ptex)
{
	struct r600_resource_texture *rtex = (struct r600_resource_texture*)ptex;
	struct r600_resource *resource = &rtex->resource;
	struct r600_screen *rscreen = r600_screen(screen);

	if (resource->bo) {
		radeon_bo_decref(rscreen->rw, resource->bo);
	}
	FREE(rtex);
}

static struct pipe_surface *r600_create_surface(struct pipe_context *pipe,
						struct pipe_resource *texture,
						const struct pipe_surface *surf_tmpl)
{
	struct r600_resource_texture *rtex = (struct r600_resource_texture*)texture;
	struct pipe_surface *surface = CALLOC_STRUCT(pipe_surface);
	unsigned long offset;
	unsigned level = surf_tmpl->u.tex.level;

	assert(surf_tmpl->u.tex.first_layer == surf_tmpl->u.tex.last_layer);
	if (surface == NULL)
		return NULL;
	/* offset not used. Either drop or subclass pipe_surface */
	offset = r600_texture_get_offset(rtex, level, surf_tmpl->u.tex.first_layer);
	pipe_reference_init(&surface->reference, 1);
	pipe_resource_reference(&surface->texture, texture);
	surface->context = pipe;
	surface->format = surf_tmpl->format;
	surface->width = u_minify(texture->width0, level);
	surface->height = u_minify(texture->height0, level);
	surface->usage = surf_tmpl->usage;
	surface->texture = texture;
	surface->u.tex.first_layer = surf_tmpl->u.tex.first_layer;
	surface->u.tex.last_layer = surf_tmpl->u.tex.last_layer;
	surface->u.tex.level = level;
	return surface;
}

static void r600_surface_destroy(struct pipe_context *pipe,
				 struct pipe_surface *surface)
{
	pipe_resource_reference(&surface->texture, NULL);
	FREE(surface);
}

struct pipe_resource *r600_texture_from_handle(struct pipe_screen *screen,
					       const struct pipe_resource *templ,
					       struct winsys_handle *whandle)
{
	struct radeon *rw = (struct radeon*)screen->winsys;
	struct r600_resource_texture *rtex;
	struct r600_resource *resource;
	struct radeon_bo *bo = NULL;

	bo = radeon_bo(rw, whandle->handle, 0, 0, NULL);
	if (bo == NULL) {
		return NULL;
	}

	/* Support only 2D textures without mipmaps */
	if (templ->target != PIPE_TEXTURE_2D || templ->depth0 != 1 || templ->last_level != 0)
		return NULL;

	rtex = CALLOC_STRUCT(r600_resource_texture);
	if (rtex == NULL)
		return NULL;

	resource = &rtex->resource;
	resource->base.b = *templ;
	resource->base.vtbl = &r600_texture_vtbl;
	pipe_reference_init(&resource->base.b.reference, 1);
	resource->base.b.screen = screen;
	resource->bo = bo;
	rtex->pitch_override = whandle->stride;
	rtex->bpt = util_format_get_blocksize(templ->format);
	rtex->pitch[0] = whandle->stride;
	rtex->offset[0] = 0;
	rtex->size = align(rtex->pitch[0] * templ->height0, 64);

	return &resource->base.b;
}

static unsigned int r600_texture_is_referenced(struct pipe_context *context,
						struct pipe_resource *texture,
						unsigned level, int layer)
{
	/* FIXME */
	return PIPE_REFERENCED_FOR_READ | PIPE_REFERENCED_FOR_WRITE;
}

struct pipe_transfer* r600_texture_get_transfer(struct pipe_context *ctx,
						struct pipe_resource *texture,
						unsigned level,
						unsigned usage,
						const struct pipe_box *box)
{
	struct r600_resource_texture *rtex = (struct r600_resource_texture*)texture;
	struct r600_transfer *trans;

	trans = CALLOC_STRUCT(r600_transfer);
	if (trans == NULL)
		return NULL;
	pipe_resource_reference(&trans->transfer.resource, texture);
	trans->transfer.level = level;
	trans->transfer.usage = usage;
	trans->transfer.box = *box;
	trans->transfer.stride = rtex->pitch[level];
	trans->offset = r600_texture_get_offset(rtex, level, box->z);
	return &trans->transfer;
}

void r600_texture_transfer_destroy(struct pipe_context *ctx,
				   struct pipe_transfer *trans)
{
	pipe_resource_reference(&trans->resource, NULL);
	FREE(trans);
}

void* r600_texture_transfer_map(struct pipe_context *ctx,
				struct pipe_transfer* transfer)
{
	struct r600_transfer *rtransfer = (struct r600_transfer*)transfer;
	struct r600_resource *resource;
	enum pipe_format format = transfer->resource->format;
	struct r600_screen *rscreen = r600_screen(ctx->screen);
	char *map;

	resource = (struct r600_resource *)transfer->resource;
	if (radeon_bo_map(rscreen->rw, resource->bo)) {
		return NULL;
	}
	map = resource->bo->data;

	return map + rtransfer->offset +
		transfer->box.y / util_format_get_blockheight(format) * transfer->stride +
		transfer->box.x / util_format_get_blockwidth(format) * util_format_get_blocksize(format);
}

void r600_texture_transfer_unmap(struct pipe_context *ctx,
				 struct pipe_transfer* transfer)
{
	struct r600_screen *rscreen = r600_screen(ctx->screen);
	struct r600_resource *resource;

	resource = (struct r600_resource *)transfer->resource;
	radeon_bo_unmap(rscreen->rw, resource->bo);
}

struct u_resource_vtbl r600_texture_vtbl =
{
	u_default_resource_get_handle,	/* get_handle */
	r600_texture_destroy,		/* resource_destroy */
	r600_texture_is_referenced,	/* is_resource_referenced */
	r600_texture_get_transfer,	/* get_transfer */
	r600_texture_transfer_destroy,	/* transfer_destroy */
	r600_texture_transfer_map,	/* transfer_map */
	u_default_transfer_flush_region,/* transfer_flush_region */
	r600_texture_transfer_unmap,	/* transfer_unmap */
	u_default_transfer_inline_write	/* transfer_inline_write */
};

void r600_init_surface_functions(struct r600_context *r600)
{
	r600->context.create_surface = r600_create_surface;
	r600->context.surface_destroy = r600_surface_destroy;
}
