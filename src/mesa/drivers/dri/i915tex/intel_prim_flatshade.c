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



struct flatshade_stage {
   struct prim_stage stage;

   GLuint col0_offset;
   GLuint col0_size;

   GLuint col1_offset;
   GLuint col1_size;
};



static INLINE struct flatshade_stage *flatshade_stage( struct prim_stage *stage )
{
   return (struct flatshade_stage *)stage;
}


static void flatshade_begin( struct prim_stage *stage )
{
   struct flatshade_stage *flatshade = flatshade_stage(stage);

/*    if (stage->pipe->draw->vb_state.clipped_prims) */
/*       offset->hw_data_offset = 16; */
/*    else */
/*       offset->hw_data_offset = 0;	 */

/*    flatshade->mode = stage->pipe->draw->state.offset_mode; */

   stage->next->begin( stage->next );
}



#define COLOR_BITS ((1 << VF_BIT_COLOR0) |	\
		    (1 << VF_BIT_COLOR1) |	\
		    (1 << VF_BIT_BFC0) |	\
		    (1 << VF_BIT_BFC1))

static void copy_colors( struct vertex_fetch *vf, 
			 GLubyte *vdst, 
			 const GLubyte *vsrc )
{
   const struct vertex_fetch_attr *a = vf->attr;
   const GLuint attr_count = vf->attr_count;
   GLuint j;

   for (j = 0; j < attr_count; j++) {
      if ((1 << a[j].attrib) & COLOR_BITS) {
	 memcpy( vdst + a[j].vertoffset,
		 vsrc + a[j].vertoffset,
		 a[j].vertattrsize );
      }
   }
}



/* Flatshade tri.  Required for clipping and when unfilled tris are
 * active, otherwise handled by hardware.
 */
static void flatshade_tri( struct prim_pipeline_stage *stage,
			   struct prim_header *header )
{
   struct vertex_shader *vf = flatshade_stage(stage)->vf;

   header->v[0] = dup_vert(stage, header->v[0]);
   header->v[1] = dup_vert(stage, header->v[1]);

   copy_colors(vf, header->v[0], header->v[2]);
   copy_colors(vf, header->v[1], header->v[2]);
   
   stage->next->tri( stage->next, header );
}


/* Flatshade line.  Required for clipping.
 */
static void flatshade_line( struct prim_pipeline_stage *stage,
			   struct prim_header *prim )
{
   struct vertex_shader *vf = flatshade_stage(stage)->vf;

   header->v[0] = dup_vert(stage, header->v[0]);

   copy_colors(vf, header->v[0], header->v[1]);
   
   stage->next->line( stage->next, header );
}


static void flatshade_point( struct prim_stage *stage,
			struct prim_header *header )
{
   stage->next->point( stage->next, header );
}

static void flatshade_end( struct prim_stage *stage )
{
   stage->next->end( stage->next );
}

struct prim_stage *intel_prim_flatshade( struct prim_pipeline *pipe )
{
   struct flatshade_stage *flatshade = CALLOC_STRUCT(flatshade_stage);

   intel_prim_alloc_tmps( &clip->stage, 0 );

   flatshade->stage.pipe = pipe;
   flatshade->stage.next = NULL;
   flatshade->stage.begin = flatshade_begin;
   flatshade->stage.point = flatshade_point;
   flatshade->stage.line = flatshade_line;
   flatshade->stage.tri = flatshade_tri;
   flatshade->stage.reset_tmps = intel_prim_reset_tmps;
   flatshade->stage.end = flatshade_end;

   return &flatshade->stage;
}


