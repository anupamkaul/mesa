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
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  *   Michel Dänzer <michel@tungstengraphics.com>
  */

#include "pipe/p_context.h"
#include "pipe/p_defines.h"

#include "util/u_inlines.h"
#include "util/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"

#include "lp_context.h"
#include "lp_flush.h"
#include "lp_screen.h"
#include "lp_tile_image.h"
#include "lp_texture.h"
#include "lp_tile_size.h"

#include "state_tracker/sw_winsys.h"


/**
 * Conventional allocation path for non-display textures:
 * Just compute row strides here.  Storage is allocated on demand later.
 */
static boolean
llvmpipe_texture_layout(struct llvmpipe_screen *screen,
                        struct llvmpipe_texture *lpt)
{
   struct pipe_texture *pt = &lpt->base;
   unsigned level;
   unsigned width = pt->width0;

   assert(LP_MAX_TEXTURE_2D_LEVELS <= LP_MAX_TEXTURE_LEVELS);
   assert(LP_MAX_TEXTURE_3D_LEVELS <= LP_MAX_TEXTURE_LEVELS);

   for (level = 0; level <= pt->last_level; level++) {
      unsigned nblocksx;

      /* Allocate storage for whole quads. This is particularly important
       * for depth surfaces, which are currently stored in a swizzled format.
       */
      nblocksx = util_format_get_nblocksx(pt->format, align(width, TILE_SIZE));

      lpt->stride[level] =
         align(nblocksx * util_format_get_blocksize(pt->format), 16);

      width = u_minify(width, 1);
   }

   /* allocate storage later */
   return TRUE;
}



static boolean
llvmpipe_displaytarget_layout(struct llvmpipe_screen *screen,
                              struct llvmpipe_texture *lpt)
{
   struct sw_winsys *winsys = screen->winsys;

   /* Round up the surface size to a multiple of the tile size to
    * avoid tile clipping.
    */
   unsigned width = align(lpt->base.width0, TILE_SIZE);
   unsigned height = align(lpt->base.height0, TILE_SIZE);

   lpt->dt = winsys->displaytarget_create(winsys,
                                          lpt->base.tex_usage,
                                          lpt->base.format,
                                          width, height,
                                          16,
                                          &lpt->stride[0] );

   return lpt->dt != NULL;
}


static struct pipe_texture *
llvmpipe_texture_create(struct pipe_screen *_screen,
                        const struct pipe_texture *templat)
{
   struct llvmpipe_screen *screen = llvmpipe_screen(_screen);
   struct llvmpipe_texture *lpt = CALLOC_STRUCT(llvmpipe_texture);
   if (!lpt)
      return NULL;

   lpt->base = *templat;
   pipe_reference_init(&lpt->base.reference, 1);
   lpt->base.screen = &screen->base;

   if (lpt->base.tex_usage & (PIPE_TEXTURE_USAGE_DISPLAY_TARGET |
                              PIPE_TEXTURE_USAGE_SCANOUT |
                              PIPE_TEXTURE_USAGE_SHARED)) {
      if (!llvmpipe_displaytarget_layout(screen, lpt))
         goto fail;
   }
   else {
      if (!llvmpipe_texture_layout(screen, lpt))
         goto fail;
   }
    
   return &lpt->base;

 fail:
   FREE(lpt);
   return NULL;
}


static void
llvmpipe_texture_destroy(struct pipe_texture *pt)
{
   struct llvmpipe_screen *screen = llvmpipe_screen(pt->screen);
   struct llvmpipe_texture *lpt = llvmpipe_texture(pt);

   if (lpt->dt) {
      /* display target */
      struct sw_winsys *winsys = screen->winsys;
      winsys->displaytarget_destroy(winsys, lpt->dt);
   }
   else {
      /* regular texture */
      uint face, level;

      for (face = 0; face < Elements(lpt->linear); face++) {
         for (level = 0; level < Elements(lpt->linear[0]); level++) {
            if (lpt->linear[face][level].data) {
               align_free(lpt->linear[face][level].data);
               lpt->linear[face][level].data = NULL;
            }
         }
      }

      for (face = 0; face < Elements(lpt->tiled); face++) {
         for (level = 0; level < Elements(lpt->tiled[0]); level++) {
            if (lpt->tiled[face][level].data) {
               align_free(lpt->tiled[face][level].data);
               lpt->tiled[face][level].data = NULL;
            }
         }
      }
   }

   FREE(lpt);
}


/**
 * Map a texture for read/write (rendering).  Without any synchronization.
 */
void *
llvmpipe_texture_map(struct pipe_texture *texture,
                     unsigned face,
                     unsigned level,
                     unsigned zslice,
                     enum lp_texture_usage tex_usage,
                     enum lp_texture_layout layout)
{
   struct llvmpipe_texture *lpt = llvmpipe_texture(texture);
   uint8_t *map;
   unsigned offset = 0;

   assert(tex_usage == LP_TEXTURE_READ ||
          tex_usage == LP_TEXTURE_READ_WRITE ||
          tex_usage == LP_TEXTURE_WRITE_ALL);

   assert(layout == LP_TEXTURE_TILED ||
          layout == LP_TEXTURE_LINEAR);

   if (lpt->dt) {
      /* display target */
      struct llvmpipe_screen *screen = llvmpipe_screen(texture->screen);
      struct sw_winsys *winsys = screen->winsys;
      unsigned dt_usage;

      if (tex_usage == LP_TEXTURE_READ) {
         dt_usage = PIPE_BUFFER_USAGE_CPU_READ;
      }
      else {
         dt_usage = PIPE_BUFFER_USAGE_CPU_READ_WRITE;
      }

      assert(face == 0);
      assert(level == 0);
      assert(zslice == 0);

      /* FIXME: keep map count? */
      map = winsys->displaytarget_map(winsys, lpt->dt, dt_usage);

      /* install this linear image in texture data structure */
      lpt->linear[face][level].data = map;
      lpt->linear[face][level].timestamp = lpt->tiled[face][level].timestamp + 1;
   }
   else {
      /* regular texture */
      unsigned tex_height = u_minify(texture->height0, level);
      const unsigned nblocksy =
         util_format_get_nblocksy(texture->format, tex_height);
      const unsigned stride = lpt->stride[level];

      if (texture->target == PIPE_TEXTURE_CUBE) {
         /* XXX incorrect
         offset = face * nblocksy * stride;
         */
      }
      else if (texture->target == PIPE_TEXTURE_3D) {
         offset = zslice * nblocksy * stride;
      }
      else {
         assert(face == 0);
         assert(zslice == 0);
         offset = 0;
      }
   }

   map = llvmpipe_get_texture_image(lpt, face, level, tex_usage, layout);
   map += offset;

   return map;
}


/**
 * Unmap a texture. Without any synchronization.
 */
void
llvmpipe_texture_unmap(struct pipe_texture *texture,
                       unsigned face,
                       unsigned level,
                       unsigned zslice)
{
   struct llvmpipe_texture *lpt = llvmpipe_texture(texture);

   if (lpt->dt) {
      /* display target */
      struct llvmpipe_screen *lp_screen = llvmpipe_screen(texture->screen);
      struct sw_winsys *winsys = lp_screen->winsys;

      assert(face == 0);
      assert(level == 0);
      assert(zslice == 0);

      /* make sure linear image is up to date */
      (void) llvmpipe_get_texture_image(lpt, 0, 0,
                                        LP_TEXTURE_READ,
                                        LP_TEXTURE_LINEAR);

      winsys->displaytarget_unmap(winsys, lpt->dt);
   }
}


static struct pipe_texture *
llvmpipe_texture_from_handle(struct pipe_screen *screen,
                             const struct pipe_texture *template,
                             struct winsys_handle *whandle)
{
   struct sw_winsys *winsys = llvmpipe_screen(screen)->winsys;
   struct llvmpipe_texture *lpt = CALLOC_STRUCT(llvmpipe_texture);
   if (!lpt)
      return NULL;

   lpt->base = *template;
   pipe_reference_init(&lpt->base.reference, 1);
   lpt->base.screen = screen;

   lpt->dt = winsys->displaytarget_from_handle(winsys,
                                               template,
                                               whandle,
                                               &lpt->stride[0]);
   if (!lpt->dt)
      goto fail;

   return &lpt->base;

 fail:
   FREE(lpt);
   return NULL;
}


static boolean
llvmpipe_texture_get_handle(struct pipe_screen *screen,
                            struct pipe_texture *pt,
                            struct winsys_handle *whandle)
{
   struct sw_winsys *winsys = llvmpipe_screen(screen)->winsys;
   struct llvmpipe_texture *lpt = llvmpipe_texture(pt);

   assert(lpt->dt);
   if (!lpt->dt)
      return FALSE;

   return winsys->displaytarget_get_handle(winsys, lpt->dt, whandle);
}


static struct pipe_surface *
llvmpipe_get_tex_surface(struct pipe_screen *screen,
                         struct pipe_texture *pt,
                         unsigned face, unsigned level, unsigned zslice,
                         enum lp_texture_usage usage)
{
   struct llvmpipe_texture *lpt = llvmpipe_texture(pt);
   struct pipe_surface *ps;

   assert(level <= pt->last_level);

   ps = CALLOC_STRUCT(pipe_surface);
   if (ps) {
      pipe_reference_init(&ps->reference, 1);
      pipe_texture_reference(&ps->texture, pt);
      ps->format = pt->format;
      ps->width = u_minify(pt->width0, level);
      ps->height = u_minify(pt->height0, level);
      ps->usage = usage;

      /* Because we are llvmpipe, anything that the state tracker
       * thought was going to be done with the GPU will actually get
       * done with the CPU.  Let's adjust the flags to take that into
       * account.
       */
      if (ps->usage & PIPE_BUFFER_USAGE_GPU_WRITE) {
         /* GPU_WRITE means "render" and that can involve reads (blending) */
         ps->usage |= PIPE_BUFFER_USAGE_CPU_WRITE | PIPE_BUFFER_USAGE_CPU_READ;
      }

      if (ps->usage & PIPE_BUFFER_USAGE_GPU_READ)
         ps->usage |= PIPE_BUFFER_USAGE_CPU_READ;

      if (ps->usage & (PIPE_BUFFER_USAGE_CPU_WRITE |
                       PIPE_BUFFER_USAGE_GPU_WRITE)) {
         /* Mark the surface as dirty. */
         lpt->timestamp++;
         llvmpipe_screen(screen)->timestamp++;
      }

      ps->face = face;
      ps->level = level;
      ps->zslice = zslice;
   }
   return ps;
}


static void 
llvmpipe_tex_surface_destroy(struct pipe_surface *surf)
{
   /* Effectively do the texture_update work here - if texture images
    * needed post-processing to put them into hardware layout, this is
    * where it would happen.  For llvmpipe, nothing to do.
    */
   assert(surf->texture);
   pipe_texture_reference(&surf->texture, NULL);
   FREE(surf);
}


static struct pipe_transfer *
llvmpipe_get_tex_transfer(struct pipe_context *pipe,
                          struct pipe_texture *texture,
                          unsigned face, unsigned level, unsigned zslice,
                          enum pipe_transfer_usage usage,
                          unsigned x, unsigned y, unsigned w, unsigned h)
{
   struct llvmpipe_texture *lptex = llvmpipe_texture(texture);
   struct llvmpipe_transfer *lpt;

   assert(texture);
   assert(level <= texture->last_level);

   lpt = CALLOC_STRUCT(llvmpipe_transfer);
   if (lpt) {
      struct pipe_transfer *pt = &lpt->base;
      pipe_texture_reference(&pt->texture, texture);
      pt->x = x;
      pt->y = y;
      pt->width = align(w, TILE_SIZE);
      pt->height = align(h, TILE_SIZE);
      pt->stride = lptex->stride[level];
      pt->usage = usage;
      pt->face = face;
      pt->level = level;
      pt->zslice = zslice;

      return pt;
   }
   return NULL;
}


static void 
llvmpipe_tex_transfer_destroy(struct pipe_context *pipe,
                              struct pipe_transfer *transfer)
{
   /* Effectively do the texture_update work here - if texture images
    * needed post-processing to put them into hardware layout, this is
    * where it would happen.  For llvmpipe, nothing to do.
    */
   assert (transfer->texture);
   pipe_texture_reference(&transfer->texture, NULL);
   FREE(transfer);
}


static void *
llvmpipe_transfer_map( struct pipe_context *pipe,
                       struct pipe_transfer *transfer )
{
   struct llvmpipe_screen *screen = llvmpipe_screen(pipe->screen);
   ubyte *map;
   enum pipe_format format;
   enum lp_texture_usage tex_usage;

   if (transfer->usage == PIPE_TRANSFER_READ) {
      tex_usage = LP_TEXTURE_READ;
   }
   else {
      tex_usage = LP_TEXTURE_READ_WRITE;
   }

   assert(transfer->texture);
   format = transfer->texture->format;

   /*
    * Transfers, like other pipe operations, must happen in order, so flush the
    * context if necessary.
    */
   llvmpipe_flush_texture(pipe,
                          transfer->texture, transfer->face, transfer->level,
                          0, /* flush_flags */
                          !(transfer->usage & PIPE_TRANSFER_WRITE), /* read_only */
                          TRUE, /* cpu_access */
                          FALSE); /* do_not_flush */

   /* get pointer to linear texture image */
   map = llvmpipe_texture_map(transfer->texture, transfer->face,
                              transfer->level, transfer->zslice,
                              tex_usage, LP_TEXTURE_LINEAR);

   /* May want to do different things here depending on read/write nature
    * of the map:
    */
   if (transfer->usage & PIPE_TRANSFER_WRITE) {
      /* Do something to notify sharing contexts of a texture change.
       */
      screen->timestamp++;
   }
   
   map +=
      transfer->y / util_format_get_blockheight(format) * transfer->stride +
      transfer->x / util_format_get_blockwidth(format) * util_format_get_blocksize(format);

   return map;
}


static void
llvmpipe_transfer_unmap(struct pipe_context *pipe,
                        struct pipe_transfer *transfer)
{
   assert(transfer->texture);

   llvmpipe_texture_unmap(transfer->texture,
                          transfer->face, transfer->level, transfer->zslice);
}


/**
 * Compute size (in bytes) need to store a texture image / mipmap level.
 */
static unsigned
tex_image_size(const struct llvmpipe_texture *lpt, unsigned level)
{
   /*const unsigned width = u_minify(lpt->base.width0, level);*/
   const unsigned height = u_minify(lpt->base.height0, level);
   const unsigned depth = u_minify(lpt->base.depth0, level);
   const unsigned nblocksy =
      util_format_get_nblocksy(lpt->base.format, align(height, TILE_SIZE));
   const unsigned buffer_size =
      nblocksy * lpt->stride[level] *
      (lpt->base.target == PIPE_TEXTURE_3D ? depth : 1);

   return buffer_size;
}


/**
 * Return pointer to texture image data (either linear or tiled layout).
 * \param usage  one of LP_TEXTURE_READ/WRITE_ALL/READ_WRITE
 * \param layout  either LP_TEXTURE_LINEAR or LP_TEXTURE_TILED
 */
void *
llvmpipe_get_texture_image(struct llvmpipe_texture *lpt,
                           unsigned face, unsigned level,
                           enum lp_texture_usage usage,
                           enum lp_texture_layout layout)
{
   /*
    * 'target' refers to the image which we're retrieving (either in
    * tiled or linear layout).
    * 'other' refers to the same image but in the other layout. (it may
    *  or may not exist.
    */
   struct llvmpipe_texture_image *target_img;
   struct llvmpipe_texture_image *other_img;
   void *target_data;
   void *other_data;

   assert(layout == LP_TEXTURE_TILED ||
          layout == LP_TEXTURE_LINEAR);

   assert(usage == LP_TEXTURE_READ ||
          usage == LP_TEXTURE_READ_WRITE ||
          usage == LP_TEXTURE_WRITE_ALL);

   /* which is target?  which is other? */
   if (layout == LP_TEXTURE_LINEAR) {
      target_img = &lpt->linear[face][level];
      other_img = &lpt->tiled[face][level];
   }
   else {
      target_img = &lpt->tiled[face][level];
      other_img = &lpt->linear[face][level];
   }

   target_data = target_img->data;
   other_data = other_img->data;

   if (!target_data) {
      /* allocate memory for the target image now */
      unsigned buffer_size = tex_image_size(lpt, level);
      target_img->data = align_malloc(buffer_size, 16);
      target_img->timestamp = 0;
      target_data = target_img->data;
   }

   /* check if the other image is newer than target image */
   if (other_data &&
       other_img->timestamp > target_img->timestamp &&
       usage != LP_TEXTURE_WRITE_ALL) {
      /* The other image is newer than the target image -> need to
       * update target image.
       */
      const unsigned width = u_minify(lpt->base.width0, level);
      const unsigned height = u_minify(lpt->base.height0, level);

      if (layout == LP_TEXTURE_LINEAR)
         lp_tiled_to_linear(other_data, target_data,
                            width, height, lpt->base.format,
                            lpt->stride[level]);
      else
         lp_linear_to_tiled(other_data, target_data,
                            width, height, lpt->base.format,
                            lpt->stride[level]);

      /* target image is now equal to the other image */
      target_img->timestamp = other_img->timestamp;
   }

   if (usage != LP_TEXTURE_READ) {
      /* We're changing the target image so it'll be newer than the other
       * image.  Update its timestamp.
       */
      target_img->timestamp = other_img->timestamp + 1;
   }

   /* XXX if the 'other' image isn't used for a "long time", free it.
    */

   assert(target_data);

   return target_data;
}


void
llvmpipe_init_screen_texture_funcs(struct pipe_screen *screen)
{
   screen->texture_create = llvmpipe_texture_create;
   screen->texture_destroy = llvmpipe_texture_destroy;
   screen->texture_get_handle = llvmpipe_texture_get_handle;

   screen->get_tex_surface = llvmpipe_get_tex_surface;
   screen->tex_surface_destroy = llvmpipe_tex_surface_destroy;
}


void
llvmpipe_init_context_texture_funcs(struct pipe_context *pipe)
{
   pipe->get_tex_transfer = llvmpipe_get_tex_transfer;
   pipe->tex_transfer_destroy = llvmpipe_tex_transfer_destroy;
   pipe->transfer_map = llvmpipe_transfer_map;
   pipe->transfer_unmap = llvmpipe_transfer_unmap;
}
