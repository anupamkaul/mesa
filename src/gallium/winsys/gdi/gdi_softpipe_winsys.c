/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Bismarck, ND., USA
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

/**
 * @file
 * Softpipe support.
 *
 * @author Keith Whitwell
 * @author Brian Paul
 * @author Jose Fonseca
 */


#include <windows.h>

#include "pipe/internal/p_winsys_screen.h"
#include "pipe/p_format.h"
#include "pipe/p_context.h"
#include "pipe/p_inlines.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "softpipe/sp_winsys.h"
#include "softpipe/sp_buffer.h"
#include "softpipe/sp_texture.h"
#include "shared/stw_winsys.h"


struct gdi_softpipe_displaytarget
{
   struct softpipe_displaytarget base;

   BITMAPINFO bmi;
};


/** Cast wrapper */
static INLINE struct gdi_softpipe_displaytarget *
gdi_softpipe_displaytarget( struct softpipe_displaytarget *buf )
{
   return (struct gdi_softpipe_displaytarget *)buf;
}


static boolean
gdi_softpipe_is_displaytarget_format_supported( struct softpipe_winsys *ws,
                                                enum pipe_format format )
{
   switch(format) {
   case PIPE_FORMAT_X8R8G8B8_UNORM:
   case PIPE_FORMAT_A8R8G8B8_UNORM:
      return TRUE;

   /* TODO: Support other formats possible with BMPs, as described in 
    * http://msdn.microsoft.com/en-us/library/dd183376(VS.85).aspx */
      
   default:
      return FALSE;
   }
}


static void
gdi_softpipe_displaytarget_destroy(struct softpipe_winsys *winsys,
                                   struct softpipe_displaytarget *dt)
{
   struct gdi_softpipe_displaytarget *gdt = gdi_softpipe_displaytarget(dt);

   FREE(gdt->base.data);
   FREE(gdt);
}


/**
 * Round n up to next multiple.
 */
static INLINE unsigned
round_up(unsigned n, unsigned multiple)
{
   return (n + multiple - 1) & ~(multiple - 1);
}


static struct softpipe_displaytarget *
gdi_softpipe_displaytarget_create(struct softpipe_winsys *winsys,
                                  unsigned width, unsigned height,
                                  enum pipe_format format)
{
   const unsigned alignment = 16;
   struct gdi_softpipe_displaytarget *gdt;
   unsigned cpp;
   unsigned bpp;
   
   gdt = CALLOC_STRUCT(gdi_softpipe_displaytarget);
   if(!gdt)
      return NULL;

   gdt->base.format = format;
   gdt->base.width = width;
   gdt->base.height = height;

   bpp = pf_get_bits(format);
   cpp = pf_get_size(format);
   
   gdt->base.stride = round_up(width * cpp, alignment);
   gdt->base.size = gdt->base.stride * height;
   
   gdt->base.data = align_malloc(gdt->base.size, alignment);

   gdt->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   gdt->bmi.bmiHeader.biWidth = gdt->base.stride / cpp;
   gdt->bmi.bmiHeader.biHeight= -(long)height;
   gdt->bmi.bmiHeader.biPlanes = 1;
   gdt->bmi.bmiHeader.biBitCount = pf_get_bits(format);
   gdt->bmi.bmiHeader.biCompression = BI_RGB;
   gdt->bmi.bmiHeader.biSizeImage = 0;
   gdt->bmi.bmiHeader.biXPelsPerMeter = 0;
   gdt->bmi.bmiHeader.biYPelsPerMeter = 0;
   gdt->bmi.bmiHeader.biClrUsed = 0;
   gdt->bmi.bmiHeader.biClrImportant = 0;

   return &gdt->base;
}


static void
gdi_softpipe_displaytarget_display(struct softpipe_winsys *winsys, 
                                   struct softpipe_displaytarget *dt,
                                   void *context_private)
{
   assert(0);
}


static void
gdi_softpipe_destroy(struct softpipe_winsys *winsys)
{
   FREE(winsys);
}


static struct pipe_screen *
gdi_softpipe_screen_create(void)
{
   static struct softpipe_winsys *winsys;
   struct pipe_screen *screen;

   winsys = CALLOC_STRUCT(softpipe_winsys);
   if(!winsys)
      goto no_winsys;

   winsys->destroy = gdi_softpipe_destroy;
   winsys->is_displaytarget_format_supported = gdi_softpipe_is_displaytarget_format_supported;
   winsys->displaytarget_create = gdi_softpipe_displaytarget_create;
   winsys->displaytarget_display = gdi_softpipe_displaytarget_display;
   winsys->displaytarget_destroy = gdi_softpipe_displaytarget_destroy;

   screen = softpipe_create_screen(winsys);
   if(!screen)
      goto no_screen;

   return screen;
   
no_screen:
   FREE(winsys);
no_winsys:
   return NULL;
}


static struct pipe_context *
gdi_softpipe_context_create(struct pipe_screen *screen)
{
   return softpipe_create(screen);
}


static void
gdi_softpipe_flush_frontbuffer(struct pipe_screen *screen,
                               struct pipe_surface *surface,
                               HDC hDC)
{
    struct softpipe_texture *texture;
    struct softpipe_buffer *buffer;
    struct gdi_softpipe_displaytarget *gdt;

    texture = softpipe_texture(surface->texture);
    buffer = softpipe_buffer(texture->buffer);
    gdt = gdi_softpipe_displaytarget(buffer->dt);

    assert(gdt);
    
    StretchDIBits(hDC,
                  0, 0, gdt->base.width, gdt->base.height,
                  0, 0, gdt->base.width, gdt->base.height,
                  gdt->base.data, &gdt->bmi, 0, SRCCOPY);
}


static const struct stw_winsys stw_winsys = {
   &gdi_softpipe_screen_create,
   &gdi_softpipe_context_create,
   &gdi_softpipe_flush_frontbuffer
};


BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
   switch (fdwReason) {
   case DLL_PROCESS_ATTACH:
      if (!stw_init(&stw_winsys)) {
         return FALSE;
      }
      return stw_init_thread();

   case DLL_THREAD_ATTACH:
      return stw_init_thread();

   case DLL_THREAD_DETACH:
      stw_cleanup_thread();
      break;

   case DLL_PROCESS_DETACH:
      stw_cleanup_thread();
      stw_cleanup();
      break;
   }
   return TRUE;
}
