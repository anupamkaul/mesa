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
}
