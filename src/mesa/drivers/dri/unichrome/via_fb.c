/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>

#include "via_context.h"
#include "via_ioctl.h"
#include "via_fb.h"
#include "xf86drm.h"
#include "imports.h"
#include <sys/ioctl.h>

GLboolean
via_alloc_draw_buffer(struct via_context *vmesa, struct via_buffer *buf)
{
   drm_via_mem_t mem;
   mem.context = vmesa->hHWContext;
   mem.size = buf->size;
   mem.type = VIA_MEM_VIDEO;

   if (ioctl(vmesa->driFd, DRM_IOCTL_VIA_ALLOCMEM, &mem)) 
      return GL_FALSE;
    
    
   buf->offset = mem.offset;
   buf->map = (char *)vmesa->driScreen->pFB + mem.offset;
   buf->index = mem.index;
   return GL_TRUE;
}

void
via_free_draw_buffer(struct via_context *vmesa, struct via_buffer *buf)
{
   drm_via_mem_t mem;

   if (!vmesa) return;

   mem.context = vmesa->hHWContext;
   mem.index = buf->index;
   mem.type = VIA_MEM_VIDEO;
   ioctl(vmesa->driFd, DRM_IOCTL_VIA_FREEMEM, &mem);
   buf->map = NULL;
}


GLboolean
via_alloc_dma_buffer(struct via_context *vmesa)
{
   drm_via_dma_init_t init;

   vmesa->dma = (GLubyte *) malloc(VIA_DMA_BUFSIZ);
    
   /*
    * Check whether AGP DMA has been initialized.
    */
   init.func = VIA_DMA_INITIALIZED;
   vmesa->useAgp = 
     ( 0 == drmCommandWrite(vmesa->driFd, DRM_VIA_DMA_INIT, 
			     &init, sizeof(init)));
   if (VIA_DEBUG & DEBUG_DMA) {
      if (vmesa->useAgp) 
         fprintf(stderr, "unichrome_dri.so: Using AGP.\n");
      else
         fprintf(stderr, "unichrome_dri.so: Using PCI.\n");
   }
      
   return ((vmesa->dma) ? GL_TRUE : GL_FALSE);
}

void
via_free_dma_buffer(struct via_context *vmesa)
{
    if (!vmesa) return;
    free(vmesa->dma);
    vmesa->dma = 0;
} 

GLboolean
via_alloc_texture(struct via_context *vmesa, struct via_tex_buffer *t)
{
   if (t->memType == VIA_MEM_AGP || 
       t->memType == VIA_MEM_VIDEO) {
      drm_via_mem_t fb;

      fb.context = vmesa->hHWContext;
      fb.size = t->size;
      fb.type = t->memType;

      if (ioctl(vmesa->driFd, DRM_IOCTL_VIA_ALLOCMEM, &fb) != 0 || 
	  fb.index == 0) {
	 if (VIA_DEBUG & (DEBUG_IOCTL|DEBUG_TEXTURE))
	    fprintf(stderr, "via_alloc_texture fail\n");
	 t->index = 0;
	 return GL_FALSE;
      }	

      if (0)
	 fprintf(stderr, "offset %x index %x\n", fb.offset, fb.index);

      t->offset = fb.offset;
      t->index = fb.index;

      if (t->memType == VIA_MEM_AGP) {
	 t->bufAddr = (unsigned char *)((GLuint)vmesa->viaScreen->agpLinearStart + fb.offset); 	
	 t->texBase = (GLuint)vmesa->agpBase + fb.offset;
      }
      else {
	 t->bufAddr = (unsigned char *)(fb.offset + (GLuint)vmesa->driScreen->pFB);
	 t->texBase = fb.offset;
      }

      return GL_TRUE;
   }
   else if (t->memType == VIA_MEM_SYSTEM) {
      
      t->bufAddr = MESA_PBUFFER_ALLOC(t->size);      
      t->texBase = 0;
      t->offset = 0;
      t->index = 0;
      
      return t->bufAddr != NULL;
   }
   else
      return GL_FALSE;
}


void
via_free_texture(struct via_context *vmesa, struct via_tex_buffer *t)
{
    if (t->memType == VIA_MEM_SYSTEM) {
       MESA_PBUFFER_FREE(t->bufAddr);
       t->bufAddr = 0;
    }
    else if (t->index) {
       drm_via_mem_t fb;

       fb.context = vmesa->hHWContext;
       fb.index = t->index;
       fb.type = t->memType;

       if (ioctl(vmesa->driFd, DRM_IOCTL_VIA_FREEMEM, &fb)) {
	  fb.context = vmesa->hHWContext;
	  if (ioctl(vmesa->driFd, DRM_IOCTL_VIA_FREEMEM, &fb)) {
	     fprintf(stderr, "via_free_texture fail\n");
	  }
       }

       t->bufAddr = NULL;
       t->index = 0;
    }
}
