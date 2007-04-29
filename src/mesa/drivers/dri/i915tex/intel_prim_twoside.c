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


struct twoside_stage {
   struct prim_pipeline_stage base;
   
   GLboolean facing_flag;

   GLuint col0_offset;
   GLuint bfc0_offset;

   GLuint col1_offset;
   GLuint bfc1_offset;
};

struct prim_pipeline_stage *intel_prim_twoside( struct prim_pipeline *pipe )
{
   struct twoside_stage *twoside = MALLOC(sizeof(*twoside));

   if (stage) {
      prim_init_stage( pipe, &twoside->base, 2 );

      twoside->base.tri = twoside_tri;
   }

   return stage;
}


static void copy_bfc( struct twoside_stage *twoside, 
		      struct vertex_header *v )
{
   memcpy( v->data + twoside->col0_offset, 
	   v->data + twoside->bfc0_offset, 
	   sizeof(GLuint) );

   if (twoside->col1_offset) 
      memcpy( v->data + twoside->col1_offset, 
	      v->data + twoside->bfc1_offset, 
	      sizeof(GLuint) );
}


/* Twoside tri:
 */
static void twoside_tri( struct prim_pipeline_stage *stage,
			  struct vertex_header *v0,
			  struct vertex_header *v1,
			  struct vertex_header *v2 )
{
   GLuint det = calc_det(v0, v1, v2);
   GLbooean backface = (det < 0) ^ twoside->facing_flag;
  
   if (backface) {
      v0 = dup_vert(stage, 0, v0);
      v1 = dup_vert(stage, 1, v1);
      v2 = dup_vert(stage, 2, v2);
      
      copy_bfc(stage, v0);
      copy_bfc(stage, v1);
      copy_bfc(stage, v2);
   }

   stage->next->tri( stage->next, v0, v1, v2 );
}


static void twoside_line( struct prim_stage *stage,
		       struct prim_header *header )
{
   stage->next->line( stage->next, header );
}


static void twoside_point( struct prim_stage *stage,
			struct prim_header *header )
{
   stage->next->point( stage->next, header );
}

static void twoside_end( struct prim_stage *stage )
{
   stage->next->end( stage->next );
}



struct prim_stage *intel_prim_twoside( struct prim_pipeline *pipe )
{
   struct twoside_stage *twoside = CALLOC_STRUCT(twoside_stage);

   intel_prim_alloc_tmps( &clip->stage, 3 );

   twoside->stage.pipe = pipe;
   twoside->stage.next = NULL;
   twoside->stage.begin = twoside_begin;
   twoside->stage.point = twoside_point;
   twoside->stage.line = twoside_line;
   twoside->stage.tri = twoside_tri;
   twoside->stage.reset_tmps = intel_prim_reset_tmps;
   twoside->stage.end = twoside_end;

   return &twoside->stage;
}
