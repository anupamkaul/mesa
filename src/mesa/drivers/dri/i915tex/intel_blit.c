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


#include <stdio.h>
#include <errno.h>

#include "mtypes.h"
#include "context.h"
#include "enums.h"

#include "intel_batchbuffer.h"
#include "intel_blit.h"
#include "intel_buffers.h"
#include "intel_context.h"
#include "intel_fbo.h"
#include "intel_frame_tracker.h"
#include "intel_reg.h"
#include "intel_regions.h"
#include "vblank.h"

#define FILE_DEBUG_FLAG DEBUG_BLIT




void
intelEmitFillBlit(struct intel_context *intel,
                  GLuint cpp,
                  GLshort dst_pitch,
                  struct _DriBufferObject *dst_buffer,
                  GLuint dst_offset,
                  GLshort x, GLshort y, GLshort w, GLshort h, GLuint color)
{
   GLuint BR13, CMD;
   BATCH_LOCALS;

   intel_frame_set_mode( intel->ft, INTEL_FT_BLITTER );

   dst_pitch *= cpp;

   switch (cpp) {
   case 1:
   case 2:
   case 3:
      BR13 = dst_pitch | (0xF0 << 16) | (1 << 24);
      CMD = XY_COLOR_BLT_CMD;
      break;
   case 4:
      BR13 = dst_pitch | (0xF0 << 16) | (1 << 24) | (1 << 25);
      CMD = (XY_COLOR_BLT_CMD | XY_COLOR_BLT_WRITE_ALPHA |
             XY_COLOR_BLT_WRITE_RGB);
      break;
   default:
      return;
   }

   DBG("%s dst:buf(%p)/%d+%d %d,%d sz:%dx%d\n",
       __FUNCTION__, dst_buffer, dst_pitch, dst_offset, x, y, w, h);


   BEGIN_BATCH(6, INTEL_BATCH_NO_CLIPRECTS);
   OUT_BATCH(CMD);
   OUT_BATCH(BR13);
   OUT_BATCH((y << 16) | x);
   OUT_BATCH(((y + h) << 16) | (x + w));
   OUT_RELOC(dst_buffer, DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
             DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE, dst_offset);
   OUT_BATCH(color);
   ADVANCE_BATCH();
}


static GLuint translate_raster_op(GLenum logicop)
{
   switch(logicop) {
   case GL_CLEAR: return 0x00;
   case GL_AND: return 0x88;
   case GL_AND_REVERSE: return 0x44;
   case GL_COPY: return 0xCC;
   case GL_AND_INVERTED: return 0x22;
   case GL_NOOP: return 0xAA;
   case GL_XOR: return 0x66;
   case GL_OR: return 0xEE;
   case GL_NOR: return 0x11;
   case GL_EQUIV: return 0x99;
   case GL_INVERT: return 0x55;
   case GL_OR_REVERSE: return 0xDD;
   case GL_COPY_INVERTED: return 0x33;
   case GL_OR_INVERTED: return 0xBB;
   case GL_NAND: return 0x77;
   case GL_SET: return 0xFF;
   default: return 0;
   }
}


/* Copy BitBlt
 */
void
intelEmitCopyBlit(struct intel_context *intel,
                  GLuint cpp,
                  GLshort src_pitch,
                  struct _DriBufferObject *src_buffer,
                  GLuint src_offset,
                  GLshort dst_pitch,
                  struct _DriBufferObject *dst_buffer,
                  GLuint dst_offset,
                  GLshort src_x, GLshort src_y,
                  GLshort dst_x, GLshort dst_y, 
		  GLshort w, GLshort h,
		  GLenum logic_op)
{
   GLuint CMD, BR13;
   int dst_y2 = dst_y + h;
   int dst_x2 = dst_x + w;
   BATCH_LOCALS;

   intel_frame_set_mode( intel->ft, INTEL_FT_BLITTER );

   DBG("%s src:buf(%p)/%d+%d %d,%d dst:buf(%p)/%d+%d %d,%d sz:%dx%d\n",
       __FUNCTION__,
       src_buffer, src_pitch, src_offset, src_x, src_y,
       dst_buffer, dst_pitch, dst_offset, dst_x, dst_y, w, h);

   src_pitch *= cpp;
   dst_pitch *= cpp;

   switch (cpp) {
   case 1:
   case 2:
   case 3:
      BR13 = (((GLint) dst_pitch) & 0xffff) | 
	 (translate_raster_op(logic_op) << 16) | (1 << 24);
      CMD = XY_SRC_COPY_BLT_CMD;
      break;
   case 4:
      BR13 =
         (((GLint) dst_pitch) & 0xffff) | 
	 (translate_raster_op(logic_op) << 16) | (1 << 24) | (1 << 25);
      CMD =
         (XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
          XY_SRC_COPY_BLT_WRITE_RGB);
      break;
   default:
      return;
   }

   if (dst_y2 < dst_y || dst_x2 < dst_x) {
      return;
   }

   /* Initial y values don't seem to work with negative pitches.  If
    * we adjust the offsets manually (below), it seems to work fine.
    *
    * On the other hand, if we always adjust, the hardware doesn't
    * know which blit directions to use, so overlapping copypixels get
    * the wrong result.
    */
   if (dst_pitch > 0 && src_pitch > 0) {
      BEGIN_BATCH(8, INTEL_BATCH_NO_CLIPRECTS);
      OUT_BATCH(CMD);
      OUT_BATCH(BR13);
      OUT_BATCH((dst_y << 16) | dst_x);
      OUT_BATCH((dst_y2 << 16) | dst_x2);
      OUT_RELOC(dst_buffer, DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE, dst_offset);
      OUT_BATCH((src_y << 16) | src_x);
      OUT_BATCH(((GLint) src_pitch & 0xffff));
      OUT_RELOC(src_buffer, DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_READ, src_offset);
      ADVANCE_BATCH();
   }
   else {
      BEGIN_BATCH(8, INTEL_BATCH_NO_CLIPRECTS);
      OUT_BATCH(CMD);
      OUT_BATCH(BR13);
      OUT_BATCH((0 << 16) | dst_x);
      OUT_BATCH((h << 16) | dst_x2);
      OUT_RELOC(dst_buffer, DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_WRITE,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_WRITE,
                dst_offset + dst_y * dst_pitch);
      OUT_BATCH((0 << 16) | src_x);
      OUT_BATCH(((GLint) src_pitch & 0xffff));
      OUT_RELOC(src_buffer, DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_READ,
                DRM_BO_MASK_MEM | DRM_BO_FLAG_READ,
                src_offset + src_y * src_pitch);
      ADVANCE_BATCH();
   }
}



