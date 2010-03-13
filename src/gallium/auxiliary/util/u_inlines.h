/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#ifndef U_INLINES_H
#define U_INLINES_H

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_screen.h"
#include "util/u_debug.h"
#include "util/u_atomic.h"
#include "util/u_box.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Reference counting helper functions.
 */


static INLINE void
pipe_reference_init(struct pipe_reference *reference, unsigned count)
{
   p_atomic_set(&reference->count, count);
}

static INLINE boolean
pipe_is_referenced(struct pipe_reference *reference)
{
   return p_atomic_read(&reference->count) != 0;
}

/**
 * Update reference counting.
 * The old thing pointed to, if any, will be unreferenced.
 * Both 'ptr' and 'reference' may be NULL.
 * \return TRUE if the object's refcount hits zero and should be destroyed.
 */
static INLINE boolean
pipe_reference(struct pipe_reference *ptr, struct pipe_reference *reference)
{
   boolean destroy = FALSE;

   if(ptr != reference) {
      /* bump the reference.count first */
      if (reference) {
         assert(pipe_is_referenced(reference));
         p_atomic_inc(&reference->count);
      }

      if (ptr) {
         assert(pipe_is_referenced(ptr));
         if (p_atomic_dec_zero(&ptr->count)) {
            destroy = TRUE;
         }
      }
   }

   return destroy;
}

static INLINE void
pipe_buffer_reference(struct pipe_buffer **ptr, struct pipe_buffer *buf)
{
   struct pipe_buffer *old_buf;

   assert(ptr);
   old_buf = *ptr;

   if (pipe_reference(&(*ptr)->base.reference, &buf->base.reference))
      old_buf->base.screen->resource_destroy(&old_buf->base);
   *ptr = buf;
}

static INLINE void
pipe_surface_reference(struct pipe_surface **ptr, struct pipe_surface *surf)
{
   struct pipe_surface *old_surf = *ptr;

   if (pipe_reference(&(*ptr)->reference, &surf->reference))
      old_surf->resource->screen->tex_surface_destroy(old_surf);
   *ptr = surf;
}

static INLINE void
pipe_texture_reference(struct pipe_texture **ptr, struct pipe_texture *tex)
{
   struct pipe_texture *old_tex = *ptr;

   if (pipe_reference(&(*ptr)->base.reference, &tex->base.reference))
      old_tex->base.screen->resource_destroy(&old_tex->base);
   *ptr = tex;
}

static INLINE void
pipe_resource_reference(struct pipe_resource **ptr, struct pipe_resource *tex)
{
   struct pipe_resource *old_tex = *ptr;

   if (pipe_reference(&(*ptr)->reference, &tex->reference))
      old_tex->screen->resource_destroy(old_tex);
   *ptr = tex;
}

static INLINE void
pipe_sampler_view_reference(struct pipe_sampler_view **ptr, struct pipe_sampler_view *view)
{
   struct pipe_sampler_view *old_view = *ptr;

   if (pipe_reference(&(*ptr)->reference, &view->reference))
      old_view->context->sampler_view_destroy(old_view->context, old_view);
   *ptr = view;
}


/*
 * Convenience wrappers for screen buffer functions.
 */

static INLINE struct pipe_buffer *
pipe_buffer_create( struct pipe_screen *screen,
                    unsigned alignment, unsigned usage, unsigned size )
{
   struct pipe_buffer buffer;
   memset(&buffer, 0, sizeof buffer);
   buffer.base.target = PIPE_RESOURCE_BUFFER;
   buffer.base.format = PIPE_FORMAT_R8_UNORM; /* want TYPELESS or similar */
   buffer.base.usage = usage;
   buffer.base.width0 = size;
   buffer.base.height0 = 1;
   buffer.base.depth0 = 1;
   return (struct pipe_buffer *)screen->resource_create(screen, &buffer.base);
}

#if 0
static INLINE struct pipe_buffer *
pipe_user_buffer_create( struct pipe_screen *screen, void *ptr, unsigned size )
{
   return screen->user_buffer_create(screen, ptr, size);
}
#endif

static INLINE void *
pipe_buffer_map_range(struct pipe_context *pipe,
		      struct pipe_buffer *buffer,
		      unsigned offset,
		      unsigned length,
		      unsigned usage,
		      struct pipe_transfer **transfer)
{
   struct pipe_box box;

   assert(offset < buffer->base.width0);
   assert(offset + length <= buffer->base.width0);
   assert(length);
   
   u_box_1d(offset, length, &box);

   *transfer = pipe->get_transfer( pipe,
				   &buffer->base,
				   u_subresource(0, 0),
				   usage,
				   &box);
   
   if (*transfer == NULL)
      return NULL;

   return pipe->transfer_map( pipe, *transfer );
}


static INLINE void *
pipe_buffer_map(struct pipe_context *pipe,
                struct pipe_buffer *buffer,
                unsigned usage,
		struct pipe_transfer **transfer)
{
   return pipe_buffer_map_range(pipe, buffer, usage, 0, buffer->base.width0, transfer);
}


static INLINE void
pipe_buffer_unmap(struct pipe_context *pipe,
                  struct pipe_buffer *buf,
		  struct pipe_transfer *transfer)
{
   if (transfer) {
      pipe->transfer_unmap(pipe, transfer);
      pipe->transfer_destroy(pipe, transfer);
   }
}

static INLINE void
pipe_buffer_flush_mapped_range(struct pipe_context *pipe,
			       struct pipe_transfer *transfer,
                               unsigned offset,
                               unsigned length)
{
   struct pipe_box box;

   assert(length);

   u_box_1d(offset, length, &box);

   pipe->transfer_flush_region(pipe, transfer, &box);
}

static INLINE void
pipe_buffer_write(struct pipe_context *pipe,
                  struct pipe_buffer *buf,
                  unsigned offset,
		  unsigned size,
                  const void *data)
{
   struct pipe_box box;
   struct pipe_subresource subresource;

   subresource.face = 0;
   subresource.level = 0;

   box.x = offset;
   box.y = 0;
   box.z = 0;
   box.w = size;
   box.h = 1;
   box.d = 1;

   pipe->transfer_inline_write( pipe,
				&buf->base,
				subresource,
				PIPE_TRANSFER_WRITE,
				&box,
				data);
}

/**
 * Special case for writing non-overlapping ranges.
 *
 * We can avoid GPU/CPU synchronization when writing range that has never
 * been written before.
 */
static INLINE void
pipe_buffer_write_nooverlap(struct pipe_context *pipe,
                            struct pipe_buffer *buf,
                            unsigned offset, unsigned size,
                            const void *data)
{
   struct pipe_box box;
   struct pipe_subresource subresource;

   subresource.face = 0;
   subresource.level = 0;

   box.x = offset;
   box.y = 0;
   box.z = 0;
   box.w = size;
   box.h = 1;
   box.d = 1;

   pipe->transfer_inline_write(pipe, 
			       &buf->base,
			       subresource,
			       (PIPE_TRANSFER_WRITE |
				PIPE_TRANSFER_NOOVERWRITE),
			       &box,
			       data);
}

static INLINE void
pipe_buffer_read(struct pipe_context *pipe,
                 struct pipe_buffer *buf,
                 unsigned offset, unsigned size,
                 void *data)
{
   struct pipe_box box;
   struct pipe_subresource subresource;

   subresource.face = 0;
   subresource.level = 0;

   box.x = offset;
   box.y = 0;
   box.z = 0;
   box.w = size;
   box.h = 1;
   box.d = 1;

   pipe->transfer_inline_read( pipe,
			       &buf->base,
			       subresource,
			       PIPE_TRANSFER_READ,
			       &box,
			       data);
}

static INLINE void *
pipe_transfer_map( struct pipe_context *context,
                   struct pipe_transfer *transfer )
{
   return context->transfer_map( context, transfer );
}

static INLINE void
pipe_transfer_unmap( struct pipe_context *context,
                     struct pipe_transfer *transfer )
{
   context->transfer_unmap( context, transfer );
}


static INLINE void
pipe_transfer_destroy( struct pipe_context *context, 
		       struct pipe_transfer *transfer )
{
   context->transfer_destroy(context, transfer);
}


#ifdef __cplusplus
}
#endif

#endif /* U_INLINES_H */
