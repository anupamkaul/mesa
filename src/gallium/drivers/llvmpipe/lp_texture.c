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

#include <stdio.h>

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
 * Allocate storage for llvmpipe_texture::layout array.
 * The number of elements is width_in_tiles * height_in_tiles.
 */
static enum lp_texture_layout *
alloc_layout_array(unsigned width, unsigned height)
{
   const unsigned tx = align(width, TILE_SIZE) / TILE_SIZE;
   const unsigned ty = align(height, TILE_SIZE) / TILE_SIZE;

   assert(tx * ty > 0);
   assert(LP_TEX_LAYOUT_NONE == 0); /* calloc'ing LP_TEX_LAYOUT_NONE here */

   return (enum lp_texture_layout *)
      calloc(tx * ty, sizeof(enum lp_texture_layout));
}



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
   unsigned height = pt->height0;

   assert(LP_MAX_TEXTURE_2D_LEVELS <= LP_MAX_TEXTURE_LEVELS);
   assert(LP_MAX_TEXTURE_3D_LEVELS <= LP_MAX_TEXTURE_LEVELS);

   for (level = 0; level <= pt->last_level; level++) {
      const unsigned num_faces = lpt->base.target == PIPE_TEXTURE_CUBE ? 6 : 1;
      unsigned nblocksx, face;

      /* Allocate storage for whole quads. This is particularly important
       * for depth surfaces, which are currently stored in a swizzled format.
       */
      nblocksx = util_format_get_nblocksx(pt->format, align(width, TILE_SIZE));

      lpt->stride[level] =
         align(nblocksx * util_format_get_blocksize(pt->format), 16);

      lpt->tiles_per_row[level] = align(width, TILE_SIZE) / TILE_SIZE;

      for (face = 0; face < num_faces; face++) {
         lpt->layout[face][level] = alloc_layout_array(width, height);
      }

      width = u_minify(width, 1);
      height = u_minify(height, 1);
   }

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

   lpt->tiles_per_row[0] = align(width, TILE_SIZE) / TILE_SIZE;

   lpt->layout[0][0] = alloc_layout_array(width, height);

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
   static unsigned id_counter = 0;
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

   assert(lpt->layout[0][0][0] == LP_TEX_LAYOUT_NONE);

   lpt->id = id_counter++;

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

   assert(face < 6);
   assert(level < LP_MAX_TEXTURE_LEVELS);

   assert(tex_usage == LP_TEX_USAGE_READ ||
          tex_usage == LP_TEX_USAGE_READ_WRITE ||
          tex_usage == LP_TEX_USAGE_WRITE_ALL);

   assert(layout == LP_TEX_LAYOUT_NONE ||
          layout == LP_TEX_LAYOUT_TILED ||
          layout == LP_TEX_LAYOUT_LINEAR);

   if (lpt->dt) {
      /* display target */
      struct llvmpipe_screen *screen = llvmpipe_screen(texture->screen);
      struct sw_winsys *winsys = screen->winsys;
      unsigned dt_usage;

      if (tex_usage == LP_TEX_USAGE_READ) {
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
   assert(map);
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
                                        LP_TEX_USAGE_READ,
                                        LP_TEX_LAYOUT_LINEAR);

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
   const char *mode;

   assert(transfer->face < 6);
   assert(transfer->level < LP_MAX_TEXTURE_LEVELS);

   /*
   printf("tex_transfer_map(%d, %d  %d x %d of %d x %d,  usage %d )\n",
          transfer->x, transfer->y, transfer->width, transfer->height,
          transfer->texture->width0,
          transfer->texture->height0,
          transfer->usage);
   */

   if (transfer->usage == PIPE_TRANSFER_READ) {
      tex_usage = LP_TEX_USAGE_READ;
      mode = "read";
   }
   else {
      tex_usage = LP_TEX_USAGE_READ_WRITE;
      mode = "read/write";
   }

   if(0){
      struct llvmpipe_texture *lpt = llvmpipe_texture(transfer->texture);
      printf("transfer map tex %u  mode %s\n", lpt->id, mode);
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
                              tex_usage, LP_TEX_LAYOUT_LINEAR);

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

   assert(transfer->face < 6);
   assert(transfer->level < LP_MAX_TEXTURE_LEVELS);

   llvmpipe_texture_unmap(transfer->texture,
                          transfer->face, transfer->level, transfer->zslice);
}


/**
 * Compute size (in bytes) need to store a texture image / mipmap level.
 */
static unsigned
tex_image_size(const struct llvmpipe_texture *lpt, unsigned level,
               enum lp_texture_layout layout)
{
   enum pipe_format format = layout == LP_TEX_LAYOUT_LINEAR
      ? lpt->base.format : PIPE_FORMAT_A8R8G8B8_UNORM;
   const unsigned height = u_minify(lpt->base.height0, level);
   const unsigned depth = u_minify(lpt->base.depth0, level);
   const unsigned nblocksy =
      util_format_get_nblocksy(format, align(height, TILE_SIZE));
   const unsigned buffer_size =
      nblocksy * lpt->stride[level] *
      (lpt->base.target == PIPE_TEXTURE_3D ? depth : 1);

   return buffer_size;
}


/**
 * This function encapsulates some complicated logic for determining
 * how to convert a tile of image data from linear layout to tiled
 * layout, or vice versa.
 * \param cur_layout  the current tile layout
 * \param target_layout  the desired tile layout
 * \param usage  how the tile will be accessed (R/W vs. read-only, etc)
 * \param new_layout_return  returns the new layout mode
 * \param convert_return  returns TRUE if image conversion is needed
 */
static void
layout_logic(enum lp_texture_layout cur_layout,
             enum lp_texture_layout target_layout,
             enum lp_texture_usage usage,
             enum lp_texture_layout *new_layout_return,
             boolean *convert)
{
   enum lp_texture_layout other_layout, new_layout;

   *convert = FALSE;

   new_layout = 99; /* debug check */

   if (target_layout == LP_TEX_LAYOUT_LINEAR) {
      other_layout = LP_TEX_LAYOUT_TILED;
   }
   else {
      assert(target_layout == LP_TEX_LAYOUT_TILED);
      other_layout = LP_TEX_LAYOUT_LINEAR;
   }

   new_layout = target_layout;  /* may get changed below */

   if (cur_layout == LP_TEX_LAYOUT_BOTH) {
      if (usage == LP_TEX_USAGE_READ) {
         new_layout = LP_TEX_LAYOUT_BOTH;
      }
   }
   else if (cur_layout == other_layout) {
      if (usage != LP_TEX_USAGE_WRITE_ALL) {
         /* need to convert tiled data to linear or vice versa */
         *convert = TRUE;

         if (usage == LP_TEX_USAGE_READ)
            new_layout = LP_TEX_LAYOUT_BOTH;
      }
   }
   else {
      assert(cur_layout == LP_TEX_LAYOUT_NONE ||
             cur_layout == target_layout);
   }

   assert(new_layout == LP_TEX_LAYOUT_BOTH ||
          new_layout == target_layout);

   *new_layout_return = new_layout;
}



/**
 * Return pointer to texture image data (either linear or tiled layout).
 * \param usage  one of LP_TEX_USAGE_READ/WRITE_ALL/READ_WRITE
 * \param layout  either LP_TEX_LAYOUT_LINEAR or LP_TEX_LAYOUT_TILED
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
   const unsigned width = u_minify(lpt->base.width0, level);
   const unsigned height = u_minify(lpt->base.height0, level);
   const unsigned width_t = align(width, TILE_SIZE) / TILE_SIZE;
   const unsigned height_t = align(height, TILE_SIZE) / TILE_SIZE;

   assert(layout == LP_TEX_LAYOUT_NONE ||
          layout == LP_TEX_LAYOUT_TILED ||
          layout == LP_TEX_LAYOUT_LINEAR);

   assert(usage == LP_TEX_USAGE_READ ||
          usage == LP_TEX_USAGE_READ_WRITE ||
          usage == LP_TEX_USAGE_WRITE_ALL);

   if (lpt->dt) {
      assert(lpt->linear[face][level].data);
   }

   /* which is target?  which is other? */
   if (layout == LP_TEX_LAYOUT_LINEAR) {
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
      unsigned buffer_size = tex_image_size(lpt, level, layout);
      target_img->data = align_malloc(buffer_size, 16);
      target_data = target_img->data;
   }

   if (layout == LP_TEX_LAYOUT_NONE) {
      /* just allocating memory */
      return target_data;
   }

   if (other_data) {
      /* may need to convert other data to the requested layout */
      enum lp_texture_layout new_layout;
      unsigned x, y, i = 0;

      /* loop over all image tiles, doing layout conversion where needed */
      for (y = 0; y < height_t; y++) {
         for (x = 0; x < width_t; x++) {
            enum lp_texture_layout cur_layout = lpt->layout[face][level][i];
            boolean convert;

            layout_logic(cur_layout, layout, usage, &new_layout, &convert);

            if (convert) {
               if (layout == LP_TEX_LAYOUT_TILED) {
                  lp_linear_to_tiled(other_data, target_data,
                                     x * TILE_SIZE, y * TILE_SIZE,
                                     TILE_SIZE, TILE_SIZE,
                                     lpt->base.format,
                                     lpt->stride[level]);
               }
               else {
                  lp_tiled_to_linear(other_data, target_data,
                                     x * TILE_SIZE, y * TILE_SIZE,
                                     TILE_SIZE, TILE_SIZE,
                                     lpt->base.format,
                                     lpt->stride[level]);
               }
            }

            lpt->layout[face][level][i] = new_layout;
            i++;
         }
      }
   }
   else {
      /* no other data */
      unsigned i;
      for (i = 0; i < width_t * height_t; i++) {
         lpt->layout[face][level][i] = layout;
      }
   }

   assert(target_data);

   return target_data;
}


static INLINE enum lp_texture_layout
llvmpipe_get_texture_tile_layout(const struct llvmpipe_texture *lpt,
                                 unsigned face, unsigned level,
                                 unsigned x, unsigned y)
{
   uint i;
   assert(x < lpt->tiles_per_row[level]);
   i = y * lpt->tiles_per_row[level] + x;
   return lpt->layout[face][level][i];
}


static INLINE void
llvmpipe_set_texture_tile_layout(struct llvmpipe_texture *lpt,
                                 unsigned face, unsigned level,
                                 unsigned x, unsigned y,
                                 enum lp_texture_layout layout)
{
   uint i;
   assert(x < lpt->tiles_per_row[level]);
   i = y * lpt->tiles_per_row[level] + x;
   lpt->layout[face][level][i] = layout;
}


/**
 * Get pointer to a linear image where the tile at (x,y) is known to be
 * in linear layout.
 * \return pointer to start of image (not the tile)
 */
ubyte *
llvmpipe_get_texture_tile_linear(struct llvmpipe_texture *lpt,
                                 unsigned face, unsigned level,
                                 enum lp_texture_usage usage,
                                 unsigned x, unsigned y)
{
   struct llvmpipe_texture_image *tiled_img = &lpt->tiled[face][level];
   struct llvmpipe_texture_image *linear_img = &lpt->linear[face][level];
   enum lp_texture_layout cur_layout, new_layout;
   const unsigned tx = x / TILE_SIZE, ty = y / TILE_SIZE;
   boolean convert;

   assert(x % TILE_SIZE == 0);
   assert(y % TILE_SIZE == 0);

   if (!linear_img->data) {
      /* allocate memory for the tiled image now */
      unsigned buffer_size = tex_image_size(lpt, level, LP_TEX_LAYOUT_LINEAR);
      linear_img->data = align_malloc(buffer_size, 16);
   }

   cur_layout = llvmpipe_get_texture_tile_layout(lpt, face, level, tx, ty);

   layout_logic(cur_layout, LP_TEX_LAYOUT_LINEAR, usage,
                &new_layout, &convert);

   if (convert) {
      lp_tiled_to_linear(tiled_img->data, linear_img->data,
                         x, y, TILE_SIZE, TILE_SIZE, lpt->base.format,
                         lpt->stride[level]);
   }

   if (new_layout != cur_layout)
      llvmpipe_set_texture_tile_layout(lpt, face, level, tx, ty, new_layout);

   return linear_img->data;
}


/**
 * Get pointer to tiled data for rendering.
 * \return pointer to the tiled data at the given tile position
 */
ubyte *
llvmpipe_get_texture_tile(struct llvmpipe_texture *lpt,
                          unsigned face, unsigned level,
                          enum lp_texture_usage usage,
                          unsigned x, unsigned y)
{
   const unsigned width = u_minify(lpt->base.width0, level);
   struct llvmpipe_texture_image *tiled_img = &lpt->tiled[face][level];
   struct llvmpipe_texture_image *linear_img = &lpt->linear[face][level];
   enum lp_texture_layout cur_layout, new_layout;
   const unsigned tx = x / TILE_SIZE, ty = y / TILE_SIZE;
   boolean convert;

   assert(x % TILE_SIZE == 0);
   assert(y % TILE_SIZE == 0);

   if (!tiled_img->data) {
      /* allocate memory for the tiled image now */
      unsigned buffer_size = tex_image_size(lpt, level, LP_TEX_LAYOUT_TILED);
      tiled_img->data = align_malloc(buffer_size, 16);
   }

   cur_layout = llvmpipe_get_texture_tile_layout(lpt, face, level, tx, ty);

   layout_logic(cur_layout, LP_TEX_LAYOUT_TILED, usage, &new_layout, &convert);
   if (convert) {
      lp_linear_to_tiled(linear_img->data, tiled_img->data,
                         x, y, TILE_SIZE, TILE_SIZE, lpt->base.format,
                         lpt->stride[level]);
   }

   if (new_layout != cur_layout)
      llvmpipe_set_texture_tile_layout(lpt, face, level, tx, ty, new_layout);

   /* compute, return address of the 64x64 tile */
   {
      unsigned tiles_per_row, tile_offset;

      tiles_per_row = align(width, TILE_SIZE) / TILE_SIZE;

      assert(tiles_per_row == lpt->tiles_per_row[level]);

      tile_offset = ty * tiles_per_row + tx;
      tile_offset *= TILE_SIZE * TILE_SIZE * 4;

      assert(tiled_img->data);
      return (ubyte *) tiled_img->data + tile_offset;
   }
}


void
llvmpipe_init_screen_texture_funcs(struct pipe_screen *screen)
{
   screen->texture_create = llvmpipe_texture_create;
   screen->texture_destroy = llvmpipe_texture_destroy;
   screen->texture_from_handle = llvmpipe_texture_from_handle;
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
