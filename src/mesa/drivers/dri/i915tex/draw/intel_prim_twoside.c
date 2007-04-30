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
#include "draw/intel_draw.h"

#define INTEL_PRIM_PRIVATE
#include "draw/intel_prim.h"


struct twoside_stage {
   struct prim_stage stage;
   
   GLfloat facing;

   struct vertex_fetch *vf;
};


static INLINE struct twoside_stage *twoside_stage( struct prim_stage *stage )
{
   return (struct twoside_stage *)stage;
}


static void twoside_begin( struct prim_stage *stage )
{
   struct twoside_stage *twoside = twoside_stage(stage);

   twoside->vf = stage->pipe->draw->vb.vf;
   twoside->facing = (stage->pipe->draw->state.front_winding == WINDING_CW) ? -1 : 1;

   stage->next->begin( stage->next );
}


static INLINE void copy_color( const struct vf_attr *attr_dst,
			       const struct vf_attr *attr_src,
			       GLubyte *v )
{
   if (attr_dst && attr_src) {
      ASSERT(attr_dst->vertattrsize == 
	     attr_src->vertattrsize);

      memcpy( v + attr_dst->vertoffset,
	      v + attr_src->vertoffset,
	      attr_dst->vertattrsize );
   }
}


static struct vertex_header *copy_bfc( struct twoside_stage *twoside, 
				       const struct vertex_header *v,
				       GLuint idx )
{   
   struct vertex_fetch *vf = twoside->vf;
   struct vertex_header *tmp = dup_vert( &twoside->stage, v, idx );
   
   copy_color( vf->lookup[VF_ATTRIB_COLOR0], 
	       vf->lookup[VF_ATTRIB_BFC0],
	       (GLubyte *)tmp );

   copy_color( vf->lookup[VF_ATTRIB_COLOR1], 
	       vf->lookup[VF_ATTRIB_BFC1],
	       (GLubyte *)tmp );

   return tmp;
}


/* Twoside tri:
 */
static void twoside_tri( struct prim_stage *stage,
			 struct prim_header *header )
{
   struct twoside_stage *twoside = twoside_stage(stage);

   if (header->det * twoside->facing < 0) {
      struct prim_header tmp;

      tmp.det = header->det;
      tmp.v[0] = copy_bfc(twoside, header->v[0], 0);
      tmp.v[1] = copy_bfc(twoside, header->v[1], 1);
      tmp.v[2] = copy_bfc(twoside, header->v[2], 2);

      stage->next->tri( stage->next, &tmp );
   }
   else {
      stage->next->tri( stage->next, header );
   }
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

   intel_prim_alloc_tmps( &twoside->stage, 3 );

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
