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

#include "util/u_rect.h"
#include "lp_context.h"
#include "lp_flush.h"
#include "lp_surface.h"
#include "lp_texture.h"
#include "lp_tile_image.h"
#include "lp_tile_size.h"


/**
 * Adjust x, y, width, height to lie on tile bounds.
 */
static void
adjust_to_tile_bounds(unsigned x, unsigned y, unsigned width, unsigned height,
                      unsigned *x_tile, unsigned *y_tile,
                      unsigned *w_tile, unsigned *h_tile)
{
   *x_tile = x & ~(TILE_SIZE - 1);
   *y_tile = y & ~(TILE_SIZE - 1);
   *w_tile = ((x + width + TILE_SIZE - 1) & ~(TILE_SIZE - 1)) - *x_tile;
   *h_tile = ((y + height + TILE_SIZE - 1) & ~(TILE_SIZE - 1)) - *y_tile;
}



static void
lp_surface_copy(struct pipe_context *pipe,
                struct pipe_surface *dst, unsigned dstx, unsigned dsty,
                struct pipe_surface *src, unsigned srcx, unsigned srcy,
                unsigned width, unsigned height)
{
   enum lp_texture_layout src_layout, dst_layout;

   llvmpipe_flush_texture(pipe,
                          dst->texture, dst->face, dst->level,
                          0, /* flush_flags */
                          FALSE, /* read_only */
                          FALSE, /* cpu_access */
                          FALSE); /* do_not_flush */

   llvmpipe_flush_texture(pipe,
                          src->texture, src->face, src->level,
                          0, /* flush_flags */
                          TRUE, /* read_only */
                          FALSE, /* cpu_access */
                          FALSE); /* do_not_flush */

   /* Look for special case in which we're copying from a tiled image
    * to a linear image.
    */
   {
      struct llvmpipe_texture *src_tex = llvmpipe_texture(src->texture);
      struct llvmpipe_texture *dst_tex = llvmpipe_texture(dst->texture);
      enum pipe_format format = src_tex->base.format;

      src_layout = llvmpipe_get_texture_image_layout(src_tex,
                                                     src->face, src->level);

      dst_layout = llvmpipe_get_texture_image_layout(dst_tex,
                                                     dst->face, dst->level);

      if (src_layout == LP_TEXTURE_TILED &&
          dst_layout == LP_TEXTURE_LINEAR) {
         unsigned tx, ty, tw, th;
         ubyte *src_tiled_ptr = src_tex->tiled[src->face][src->level].data;
         ubyte *src_linear_ptr = src_tex->linear[src->face][src->level].data;
         ubyte *dst_linear_ptr = dst_tex->linear[dst->face][dst->level].data;

         if (src_linear_ptr && src_tiled_ptr && dst_linear_ptr) {
            adjust_to_tile_bounds(srcx, srcy, width, height,
                                  &tx, &ty, &tw, &th);

            /* convert tiled src data to linear */
            lp_tiled_to_linear(src_tiled_ptr,
                               src_linear_ptr,
                               tx, ty, tw, th,
                               format,
                               dst_tex->stride[dst->level]);

            util_copy_rect(dst_linear_ptr, format,
                           dst_tex->stride[dst->level],
                           dstx, dsty,
                           width, height,
                           src_linear_ptr, src_tex->stride[src->level],
                           srcx, srcy);

            if (width == src->width && height == src->height) {
               assert(srcx == 0);
               assert(srcy == 0);
               /* We converted the whole src image to linear.
                * Update the timestamp to indicate equality.
                */
               src_tex->linear[src->face][src->level].timestamp =
                  src_tex->linear[src->face][src->level].timestamp;
            }

            return;
         }
      }
   }

   util_surface_copy(pipe, FALSE,
                     dst, dstx, dsty,
                     src, srcx, srcy,
                     width, height);
}

void
lp_init_surface_functions(struct llvmpipe_context *lp)
{
   lp->pipe.surface_copy = lp_surface_copy;
   lp->pipe.surface_fill = util_surface_fill;
}
