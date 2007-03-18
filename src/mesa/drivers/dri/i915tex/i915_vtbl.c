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



#include "glheader.h"
#include "mtypes.h"
#include "imports.h"
#include "macros.h"
#include "colormac.h"

#include "tnl/t_context.h"
#include "tnl/t_vertex.h"

#include "intel_batchbuffer.h"
#include "intel_tex.h"
#include "intel_regions.h"

#include "i915_reg.h"
#include "i915_context.h"
#include "i915_cache.h"

static GLuint debug( const int *stream, 
		     const char *name,
		     GLuint len )
{
   GLuint i;

   switch (len) {
   case 0:
      _mesa_printf("Error - zero length packet (0x%08x)\n", stream[0]);
      break;
   default:
      _mesa_printf("%s (%d dwords):\n", name, len);
      break;
   }

   return len;
}

		   

static GLuint i915_debug_packet(const GLuint *stream)
{
   GLuint cmd = *stream;
   
   switch (((cmd >> 29) & 0x7)) {
   case 0x0:
      switch ((cmd >> 23) & 0x3f) {
      case 0x0:
	 return debug(stream, "MI_NOOP", 1);
      case 0x4:
	 return debug(stream, "MI_FLUSH", 1);
      default:
	 return 0;
      }
      break;
   case 0x1:
      return 0;
   case 0x2:
      switch ((cmd >> 22) & 0xff) {	 
      case 0x50:
	 return debug(stream, "XY_COLOR_BLT", (cmd & 0xff) + 2);
      case 0x53:
	 return debug(stream, "XY_SRC_COPY_BLT", (cmd & 0xff) + 2);
      default:
	 return debug(stream, "blit command", (cmd & 0xff) + 2);
      }
   case 0x3:
      switch ((cmd >> 24) & 0x1f) {	 
      case 0x6:
	 return debug(stream, "3DSTATE_ANTI_ALIASING", 1);
      case 0x7:
	 return debug(stream, "3DSTATE_RASTERIZATION_RULES", 1);
      case 0x8:
	 return debug(stream, "3DSTATE_BACKFACE_STENCIL_OPS", 1);
      case 0x9:
	 return debug(stream, "3DSTATE_BACKFACE_STENCIL_MASKS", 1);
      case 0xb:
	 return debug(stream, "3DSTATE_INDEPENDENT_ALPHA_BLEND", 1);
      case 0xc:
	 return debug(stream, "3DSTATE_MODES5", 1);	 
      case 0xd:
	 return debug(stream, "3DSTATE_MODES4", 1);
      case 0x15:
	 return debug(stream, "3DSTATE_FOG_COLOR", 1);
      case 0x16:
	 return debug(stream, "3DSTATE_COORD_SET_BINDINGS", 1);
      case 0x1c:
	 /* 3DState16NP */
	 switch((cmd >> 19) & 0x1f) {
	 case 0x10:
	    return debug(stream, "3DSTATE_SCISSOR_ENABLE", 1);
	 case 0x11:
	    return debug(stream, "3DSTATE_DEPTH_SUBRECTANGLE_DISABLE", 1);
	 default:
	    return 0;
	 }
      case 0x1d:
	 /* 3DStateMW */
	 switch ((cmd >> 16) & 0xff) {
	 case 0x0:
	    return debug(stream, "3DSTATE_MAP_STATE", (cmd & 0x1f) + 2);
	 case 0x1:
	    return debug(stream, "3DSTATE_SAMPLER_STATE", (cmd & 0x1f) + 2);
	 case 0x4:
	    return debug(stream, "3DSTATE_LOAD_STATE_IMMEDIATE", (cmd & 0xf) + 2);
	 case 0x5:
	    return debug(stream, "3DSTATE_PIXEL_SHADER_PROGRAM", (cmd & 0x1ff) + 2);
	 case 0x6:
	    return debug(stream, "3DSTATE_PIXEL_SHADER_CONSTANTS", (cmd & 0xff) + 2);
	 case 0x7:
	    return debug(stream, "3DSTATE_LOAD_INDIRECT", (cmd & 0xff) + 2);
	 case 0x80:
	    return debug(stream, "3DSTATE_DRAWING_RECTANGLE", (cmd & 0xffff) + 2);
	 case 0x81:
	    return debug(stream, "3DSTATE_SCISSOR_RECTANGLE", (cmd & 0xffff) + 2);
	 case 0x83:
	    return debug(stream, "3DSTATE_SPAN_STIPPLE", (cmd & 0xffff) + 2);
	 case 0x85:
	    return debug(stream, "3DSTATE_DEST_BUFFER_VARS", (cmd & 0xffff) + 2);
	 case 0x88:
	    return debug(stream, "3DSTATE_CONSTANT_BLEND_COLOR", (cmd & 0xffff) + 2);
	 case 0x89:
	    return debug(stream, "3DSTATE_FOG_MODE", (cmd & 0xffff) + 2);
	 case 0x8e:
	    return debug(stream, "3DSTATE_BUFFER_INFO", (cmd & 0xffff) + 2);
	 case 0x97:
	    return debug(stream, "3DSTATE_DEPTH_OFFSET_SCALE", (cmd & 0xffff) + 2);
	 case 0x98:
	    return debug(stream, "3DSTATE_DEFAULT_Z", (cmd & 0xffff) + 2);
	 case 0x99:
	    return debug(stream, "3DSTATE_DEFAULT_DIFFUSE", (cmd & 0xffff) + 2);
	 case 0x9a:
	    return debug(stream, "3DSTATE_DEFAULT_SPECULAR", (cmd & 0xffff) + 2);
	 default:
	    return 0;
	 }
      case 0x1e:
	 if (cmd & (1 << 23))
	    return debug(stream, "???", (cmd & 0xffff) + 1);
	 else
	    return debug(stream, "", 1);
      case 0x1f:
	 if ((cmd & (1 << 23)) == 0)	/* inline vertices */
	    return debug(stream, "3DPRIMITIVE (inline)", (cmd & 0x1ffff) + 2);
	 else if (cmd & (1 << 17))	/* indirect random */
	    if ((cmd & 0xffff) == 0)
	       return debug(stream, "too hard", 0);	/* unknown length, too hard */
	    else
	       return debug(stream, "3DPRIM (indexed)", (((cmd & 0xffff) + 1) / 2) + 1);
	 else
	    return debug(stream, "3DPRIM (indirect sequential)", 2);	/* indirect sequential */
      default:
	 return debug(stream, "", 0);
      }
   default:
      return 0;
   }

   return 0;
}



static void i915_destroy_context(struct intel_context *intel)
{
   struct i915_context *i915 = i915_context( &intel->ctx );

   if (i915->cctx) 
      i915_destroy_caches( i915->cctx );

   FREE( i915 );
}



static GLuint i915_flush_cmd(void)
{
   return MI_FLUSH | FLUSH_MAP_CACHE;
}

static void i915_lost_hardware( struct intel_context *intel )
{
   struct i915_context *i915 = i915_context( &intel->ctx );

   /* This is required currently as we use the batchbuffer to hold all
    * the cached items:
    */
   i915_clear_caches( i915->cctx );

   /* Update the batchbuffer id so the context tracker knows there has
    * been a discontinuity.
    */
   i915->current.id++;
}


void
i915InitVtbl(struct i915_context *i915)
{
   i915->intel.vtbl.destroy = i915_destroy_context;
   i915->intel.vtbl.flush_cmd = i915_flush_cmd;
   i915->intel.vtbl.lost_hardware = i915_lost_hardware;
   i915->intel.vtbl.debug_packet = i915_debug_packet;
}
