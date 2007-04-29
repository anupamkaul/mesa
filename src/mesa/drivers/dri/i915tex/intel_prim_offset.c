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
#include "imports.h"

#define INTEL_DRAW_PRIVATE
#include "intel_draw.h"

#define INTEL_PRIM_PRIVATE
#include "intel_prim.h"



struct offset_stage {
   struct prim_stage stage;

   GLuint hw_data_offset;
   GLuint mode;
};



static INLINE struct offset_stage *offset_stage( struct prim_stage *stage )
{
   return (struct offset_stage *)stage;
}


static void offset_begin( struct prim_stage *stage )
{
   struct offset_stage *offset = offset_stage(stage);

   if (stage->pipe->draw->vb_state.clipped_prims)
      offset->hw_data_offset = 16;
   else
      offset->hw_data_offset = 0;	

   offset->mode = stage->pipe->draw->state.offset_mode;

   stage->next->begin( stage->next );
}


/* Offset tri.  i915 can handle this, but not i830, and neither when
 * unfilled rendering.
 */
static void offset_tri( struct prim_pipeline_stage *stage,
			struct vertex_header *v0,
			struct vertex_header *v1,
			struct vertex_header *v2 )
{
   GLfloat det = calc_det(v0, v1, v2);

   /* Should have been detected in a offset stage??
    */
   if (FABSF(det) > 1e-8) {
      GLfloat offset = ctx->Polygon.OffsetUnits * DEPTH_SCALE;
      GLfloat inv_det = 1.0 / det;

      GLfloat z0 = v0->data[2].f;
      GLfloat z1 = v1->data[2].f;
      GLfloat z2 = v2->data[2].f;

      GLfloat ez = z0 - z2;
      GLfloat fz = z1 - z2;
      GLfloat a	= ey*fz - ez*fy;
      GLfloat b	= ez*fx - ex*fz;
      GLfloat ac = a * inv_det;
      GLfloat bc = b * inv_det;

      if ( ac < 0.0f ) ac = -ac;
      if ( bc < 0.0f ) bc = -bc;

      offset += MAX2( ac, bc ) * ctx->Polygon.OffsetFactor;
      offset *= ctx->DrawBuffer->_MRD;

      v0 = dup_vert(stage, v0);
      v1 = dup_vert(stage, v1);
      v2 = dup_vert(stage, v2);

      v0->data[2] += offset;
      v1->data[2] += offset;
      v2->data[2] += offset;

      stage->next->tri( stage->next, v0, v1, v2 );
   }
}



static void offset_line( struct prim_stage *stage,
		       struct prim_header *header )
{
   stage->next->line( stage->next, header );
}


static void offset_point( struct prim_stage *stage,
			struct prim_header *header )
{
   stage->next->point( stage->next, header );
}


static void offset_end( struct prim_stage *stage )
{
   stage->next->end( stage->next );
}

struct prim_stage *intel_prim_offset( struct prim_pipeline *pipe )
{
   struct offset_stage *offset = CALLOC_STRUCT(offset_stage);

   intel_prim_alloc_tmps( &clip->stage, 3 );

   offset->stage.pipe = pipe;
   offset->stage.next = NULL;
   offset->stage.begin = offset_begin;
   offset->stage.point = offset_point;
   offset->stage.line = offset_line;
   offset->stage.tri = offset_tri;
   offset->stage.reset_tmps = intel_prim_reset_tmps;
   offset->stage.end = offset_end;

   return &offset->stage;
}
