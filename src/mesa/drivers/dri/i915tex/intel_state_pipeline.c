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

/*
 * Render stage which notices state changes in vertex data.
 *
 */
#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "imports.h"
#include "mtypes.h"
#include "enums.h"

#include "tnl/t_context.h"
#include "tnl/t_vertex.h"

#include "intel_screen.h"
#include "intel_context.h"
#include "intel_batchbuffer.h"
#include "intel_reg.h"
#include "intel_state.h"

static GLuint frag_attr_to_VB( GLuint attr )
{
   switch(attr) {
   case FRAG_ATTRIB_WPOS: return VERT_ATTRIB_POS;
   case FRAG_ATTRIB_COL0: return VERT_ATTRIB_COLOR0;
   case FRAG_ATTRIB_COL1: return VERT_ATTRIB_COLOR1;
   case FRAG_ATTRIB_FOGC: return VERT_ATTRIB_FOG;
   case FRAG_ATTRIB_TEX0: return VERT_ATTRIB_TEX0;
   case FRAG_ATTRIB_TEX1: return VERT_ATTRIB_TEX1;
   case FRAG_ATTRIB_TEX2: return VERT_ATTRIB_TEX2;
   case FRAG_ATTRIB_TEX3: return VERT_ATTRIB_TEX3;
   case FRAG_ATTRIB_TEX4: return VERT_ATTRIB_TEX4;
   case FRAG_ATTRIB_TEX5: return VERT_ATTRIB_TEX5;
   case FRAG_ATTRIB_TEX6: return VERT_ATTRIB_TEX6;
   case FRAG_ATTRIB_TEX7: return VERT_ATTRIB_TEX7;
   default: return 0;
   }
}


/* A mini stage just to update our state regarding the pipeline:
 */
static GLboolean frag_attrib_size_check( GLcontext * ctx, 
					 struct tnl_pipeline_stage *stage )
{
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint i;


   /* Look at the size of each attribute coming out, and raise a
    * statechange if different. 
    */
   GLuint sizes = 0;
   GLuint varying = 0;

   /* We need to do this first:
    */
   VB->AttribPtr[VERT_ATTRIB_POS] = VB->NdcPtr;

   for (i = 0; i < FRAG_ATTRIB_VAR0; i++) {
      GLvector4f *attrib = VB->AttribPtr[frag_attr_to_VB(i)];
      sizes |= (attrib->size - 1) << (i * 2);
      varying |= (attrib->stride != 0) << i;
   }

   /* Raise statechanges if input sizes and varying have changed: 
    */
   if (intel->frag_attrib_sizes != sizes ||
       intel->frag_attrib_varying != varying) 
   {
      intel->state.dirty.intel |= INTEL_NEW_FRAG_ATTRIB_SIZES;
      intel->frag_attrib_varying = varying;
      intel->frag_attrib_sizes = sizes;

      _mesa_printf("sizes: %x varying: %x\n", sizes, varying);
   }


   /* Catch any changes from the pipeline...
    */
   intel_update_software_state(intel);

   return GL_TRUE;
}

const struct tnl_pipeline_stage _intel_check_frag_attrib_sizes = {
   "intel check frag attrib sizes",
   NULL,
   NULL,
   NULL,
   NULL,
   frag_attrib_size_check
};
