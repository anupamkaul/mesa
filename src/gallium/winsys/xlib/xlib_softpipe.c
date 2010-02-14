/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Bismarck, ND., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/

/*
 * Authors:
 *   Keith Whitwell
 *   Brian Paul
 */


#include "xm_api.h"
#include "util/u_memory.h"
#include "softpipe/sp_winsys.h"




static struct pipe_buffer *
xm_surface_buffer_create(struct pipe_winsys *winsys,
                         unsigned width, unsigned height,
                         enum pipe_format format,
                         unsigned usage,
                         unsigned tex_usage,
                         unsigned *stride)
{
   const unsigned alignment = 64;
   unsigned nblocksy, size;

   nblocksy = util_format_get_nblocksy(format, height);
   *stride = align(util_format_get_stride(format, width), alignment);
   size = *stride * nblocksy;

#ifdef USE_XSHM
   if (!debug_get_bool_option("XLIB_NO_SHM", FALSE))
   {
      struct xm_buffer *buffer = CALLOC_STRUCT(xm_buffer);

      pipe_reference_init(&buffer->base.reference, 1);
      buffer->base.alignment = alignment;
      buffer->base.usage = usage;
      buffer->base.size = size;
      buffer->userBuffer = FALSE;
      buffer->shminfo.shmid = -1;
      buffer->shminfo.shmaddr = (char *) -1;
      buffer->shm = TRUE;
         
      buffer->data = alloc_shm(buffer, size);
      if (!buffer->data)
         goto out;

      return &buffer->base;
         
   out:
      if (buffer)
         FREE(buffer);
   }
#endif
   

   return winsys->buffer_create(winsys, alignment,
                                usage,
                                size);
}




static struct pipe_screen *
xlib_create_softpipe_screen( struct xm_driver *driver )
{
   struct pipe_winsys *winsys;
   struct pipe_screen *screen;

   winsys = xlib_create_softpipe_winsys();
   if (winsys == NULL)
      return NULL;

   screen = softpipe_create_screen(winsys);
   if (screen == NULL)
      goto fail;

   return screen;

fail:
   if (winsys)
      winsys->destroy( winsys );

   return NULL;
}


struct xm_driver xlib_softpipe_driver = 
{
   .create_screen = xlib_create_softpipe_screen,
   .texture_from_sw_target = NULL,
   .display_surface = xlib_softpipe_display_surface
};



