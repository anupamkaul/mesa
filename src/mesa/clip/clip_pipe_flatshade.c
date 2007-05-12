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

#define CLIP_PRIVATE
#include "clip/clip_context.h"

#define CLIP_PIPE_PRIVATE
#include "clip/clip_pipe.h"



struct flatshade_stage {
   struct clip_pipe_stage stage;

   struct vertex_fetch *vf;
};



static INLINE struct flatshade_stage *flatshade_stage( struct clip_pipe_stage *stage )
{
   return (struct flatshade_stage *)stage;
}


static void flatshade_begin( struct clip_pipe_stage *stage )
{
   struct flatshade_stage *flatshade = flatshade_stage(stage);

   flatshade->vf = stage->pipe->draw->vb.vf;
   stage->next->begin( stage->next );
}



static INLINE void copy_color( const struct vf_attr *attr,
			       GLubyte *dst,
			       const GLubyte *src )
{
   if (attr) {
      memcpy( dst + attr->vertoffset,
	      src + attr->vertoffset,
	      attr->vertattrsize );
   }
}

static void copy_colors( const struct vertex_fetch *vf, 
			 struct vertex_header *vdst, 
			 const struct vertex_header *vsrc )
{
   GLubyte *dst = (GLubyte *)vdst;
   const GLubyte *src = (const GLubyte *)vsrc;

   copy_color( vf->lookup[VF_ATTRIB_COLOR0], dst, src );
   copy_color( vf->lookup[VF_ATTRIB_COLOR1], dst, src );
   copy_color( vf->lookup[VF_ATTRIB_BFC0], dst, src );
   copy_color( vf->lookup[VF_ATTRIB_BFC1], dst, src );
}



/* Flatshade tri.  Required for clipping and when unfilled tris are
 * active, otherwise handled by hardware.
 */
static void flatshade_tri( struct clip_pipe_stage *stage,
			   struct prim_header *header )
{
   struct prim_header tmp;
   struct vertex_fetch *vf = flatshade_stage(stage)->vf;

   tmp.det = header->det;
   tmp.v[0] = dup_vert(stage, header->v[0], 0);
   tmp.v[1] = dup_vert(stage, header->v[1], 1);
   tmp.v[2] = header->v[2];

   copy_colors(vf, tmp.v[0], tmp.v[2]);
   copy_colors(vf, tmp.v[1], tmp.v[2]);
   
   stage->next->tri( stage->next, &tmp );
}


/* Flatshade line.  Required for clipping.
 */
static void flatshade_line( struct clip_pipe_stage *stage,
			    struct prim_header *header )
{
   struct prim_header tmp;
   struct vertex_fetch *vf = flatshade_stage(stage)->vf;

   tmp.v[0] = dup_vert(stage, header->v[0], 0);
   tmp.v[1] = header->v[1];

   copy_colors(vf, tmp.v[0], tmp.v[1]);
   
   stage->next->line( stage->next, &tmp );
}


static void flatshade_point( struct clip_pipe_stage *stage,
			struct prim_header *header )
{
   stage->next->point( stage->next, header );
}

static void flatshade_end( struct clip_pipe_stage *stage )
{
   struct flatshade_stage *flatshade = flatshade_stage(stage);

   flatshade->vf = NULL;
   stage->next->end( stage->next );
}

struct clip_pipe_stage *clip_pipe_flatshade( struct clip_pipeline *pipe )
{
   struct flatshade_stage *flatshade = CALLOC_STRUCT(flatshade_stage);

   clip_pipe_alloc_tmps( &flatshade->stage, 2 );

   flatshade->stage.pipe = pipe;
   flatshade->stage.next = NULL;
   flatshade->stage.begin = flatshade_begin;
   flatshade->stage.point = flatshade_point;
   flatshade->stage.line = flatshade_line;
   flatshade->stage.tri = flatshade_tri;
   flatshade->stage.reset_tmps = clip_pipe_reset_tmps;
   flatshade->stage.end = flatshade_end;
   flatshade->vf = NULL;

   return &flatshade->stage;
}


