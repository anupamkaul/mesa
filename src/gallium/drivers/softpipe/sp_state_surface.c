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

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */

#include "sp_context.h"
#include "sp_state.h"
#include "sp_tile_cache.h"

#include "draw/draw_context.h"

#include "util/u_format.h"
#include "util/u_inlines.h"


/**
 * XXX this might get moved someday
 * Set the framebuffer surface info: color buffers, zbuffer, stencil buffer.
 * Here, we flush the old surfaces and update the tile cache to point to the new
 * surfaces.
 */
void
softpipe_set_framebuffer_state(struct pipe_context *pipe,
                               const struct pipe_framebuffer_state *fb)
{
   struct softpipe_context *sp = softpipe_context(pipe);
   uint i;

   draw_flush(sp->draw);

   for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++) {
      /* check if changing cbuf */
      if (sp->framebuffer.cbufs[i] != fb->cbufs[i]) {
         /* flush old */
         sp_flush_tile_cache(sp->cbuf_cache[i]);

         /* assign new */
         pipe_surface_reference(&sp->framebuffer.cbufs[i], fb->cbufs[i]);

         /* update cache */
         sp_tile_cache_set_surface(sp->cbuf_cache[i], fb->cbufs[i]);
      }
   }

   sp->framebuffer.nr_cbufs = fb->nr_cbufs;

   /* zbuf changing? */
   if (sp->framebuffer.zsbuf != fb->zsbuf) {
      /* flush old */
      sp_flush_tile_cache(sp->zsbuf_cache);

      /* assign new */
      pipe_surface_reference(&sp->framebuffer.zsbuf, fb->zsbuf);

      /* update cache */
      sp_tile_cache_set_surface(sp->zsbuf_cache, fb->zsbuf);

      /* Tell draw module how deep the Z/depth buffer is */
      if (sp->framebuffer.zsbuf) {
         int depth_bits;
         double mrd;
         depth_bits = util_format_get_component_bits(sp->framebuffer.zsbuf->format,
                                                     UTIL_FORMAT_COLORSPACE_ZS,
                                                     0);
         if (depth_bits > 16) {
            mrd = 0.0000001;
         }
         else {
            mrd = 0.00002;
         }
         draw_set_mrd(sp->draw, mrd);
      }
   }

   sp->framebuffer.width = fb->width;
   sp->framebuffer.height = fb->height;

   /* find out how to do blending for this framebuffer, in case we want to do so */
   for (i = 0; i < fb->nr_cbufs; i++)
   {
      const struct util_format_description* desc = util_format_description(fb->cbufs[i]->format);
      unsigned chan;

      sp->cbuf_derived[i].has_dest_alpha = util_format_has_alpha(fb->cbufs[i]->format);
      sp->cbuf_derived[i].clamp_blend_source_factors_and_results = FALSE;
      sp->cbuf_derived[i].clamp_blend_dest = FALSE;
      sp->cbuf_derived[i].perform_logicop = TRUE;

      for(chan = 0; chan < desc->nr_channels; ++chan)
      {
         if(desc->channel[chan].type == UTIL_FORMAT_TYPE_FLOAT)
            sp->cbuf_derived[i].perform_logicop = FALSE;
         else
         {
            sp->cbuf_derived[i].clamp_blend_source_factors_and_results = TRUE;
            /* we can skip this for unsigned normalized, since they are
             * already in the [0, 1] range
             */
            if(desc->channel[chan].type != UTIL_FORMAT_TYPE_UNSIGNED
                  || !desc->channel[chan].normalized)
               sp->cbuf_derived[i].clamp_blend_dest = TRUE;
         }
      }
   }

   sp->dirty |= SP_NEW_FRAMEBUFFER;
}
