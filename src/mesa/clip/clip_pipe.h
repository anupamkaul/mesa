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

#ifndef CLIP_PIPE_H
#define CLIP_PIPE_H

#include "glheader.h"

/* The prim pipeline probably needs to know about the internals of the
 * clip_context struct.  Need to figure that out shortly.
 */

struct clip;
struct clip_state;
struct clip_vb_state;
struct clip_render;
struct clip_pipeline;

struct clip_render *clip_create_prim_render( struct clip_context *draw );

GLboolean clip_pipe_validate_state( struct clip_render *render );

void clip_pipe_set_hw_render( struct clip_render *render,
			       struct clip_render *hw );

void clip_pipe_set_vb_state( struct clip_render *render,
			      struct clip_vb_state *state );

void clip_pipe_set_clip_state( struct clip_render *render,
				struct clip_state *state );




/* Carry some useful information around with the vertices in the prim
 * pipe.  
 */
struct vertex_header {
   GLuint clipmask:12;
   GLuint edgeflag:1;
   GLuint pad:3;
   GLuint index:16;
   GLubyte data[];
};


/***********************************************************************
 * Private structs and data for the clip_pipe* files.
 */
#ifdef CLIP_PIPE_PRIVATE

struct clip_pipe_stage;

struct clip_pipeline {
   struct clip_render render;
   struct clip_context *draw;

   struct clip_pipe_stage *emit;
   struct clip_pipe_stage *unfilled;
   struct clip_pipe_stage *twoside;
   struct clip_pipe_stage *clip;
   struct clip_pipe_stage *flatshade;
   struct clip_pipe_stage *offset;
   struct clip_pipe_stage *cull;

   struct clip_pipe_stage *first;

   GLubyte *verts;
   GLuint nr_vertices;
   GLuint vertex_size;

   GLenum prim;
   GLboolean need_validate;
};


#define PRIM_POINT 1
#define PRIM_LINE  2
#define PRIM_TRI   3

struct prim_header {
   GLfloat det;

   struct vertex_header *v[3];
};


/* Internal structs and helpers for the primitive clip/setup pipeline:
 */
struct clip_pipe_stage {
   struct clip_pipeline *pipe;
   struct clip_pipe_stage *next;
   struct vertex_header **tmp;
   GLuint nr_tmps;

   void (*begin)( struct clip_pipe_stage * );

   void (*point)( struct clip_pipe_stage *,
		  struct prim_header * );

   void (*line)( struct clip_pipe_stage *,
		 struct prim_header * );

   void (*tri)( struct clip_pipe_stage *,
		struct prim_header * );
   
   /* Can occur at any time, even within a call to tri() or end().
    */
   void (*reset_tmps)( struct clip_pipe_stage * );

   void (*end)( struct clip_pipe_stage * );
};


struct clip_pipe_stage *clip_pipe_emit( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_unfilled( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_twoside( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_offset( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_clip( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_flatshade( struct clip_pipeline *pipe );
struct clip_pipe_stage *clip_pipe_cull( struct clip_pipeline *pipe );


void clip_pipe_alloc_tmps( struct clip_pipe_stage *stage, GLuint nr );
void clip_pipe_free_tmps( struct clip_pipe_stage *stage );
void clip_pipe_reset_tmps( struct clip_pipe_stage *stage );

/* Reset vertex indices for the incoming vertices and all temporary
 * vertices within the pipeline.
 */
void clip_pipe_reset_vertex_indices( struct clip_pipeline *pipe );


/* Get a writeable copy of a vertex:
 */
static INLINE struct vertex_header *
dup_vert( struct clip_pipe_stage *stage,
	  const struct vertex_header *vert,
	  GLuint idx )
{   
   struct vertex_header *tmp = stage->tmp[idx];
   memcpy(tmp, vert, stage->pipe->vertex_size );
   tmp->index = ~0;
   return tmp;
}

#endif
#endif
