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

#ifndef INTEL_PRIM_H
#define INTEL_PRIM_H

#include "glheader.h"

/* The prim pipeline probably needs to know about the internals of the
 * intel_draw struct.  Need to figure that out shortly.
 */

struct intel_draw;
struct intel_draw_state;
struct intel_draw_vb_state;
struct intel_render;
struct prim_pipeline;

struct intel_render *intel_create_prim_render( struct intel_draw *draw );

GLboolean intel_prim_validate_state( struct intel_render *render );

void intel_prim_set_hw_render( struct intel_render *render,
			       struct intel_render *hw );

void intel_prim_set_vb_state( struct intel_render *render,
			      struct intel_draw_vb_state *state );

void intel_prim_set_draw_state( struct intel_render *render,
				struct intel_draw_state *state );




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
 * Private structs and data for the intel_prim* files.
 */
#ifdef INTEL_PRIM_PRIVATE

struct prim_stage;

struct prim_pipeline {
   struct intel_render render;
   struct intel_draw *draw;

   struct prim_stage *emit;
   struct prim_stage *unfilled;
   struct prim_stage *twoside;
   struct prim_stage *clip;
   struct prim_stage *flatshade;
   struct prim_stage *cull;

   struct prim_stage *first;

   GLubyte *verts;
   GLuint nr_vertices;
   GLuint vertex_size;

   GLenum prim;
   GLboolean need_validate;
};


#define PRIM_POINT 1
#define PRIM_LINE  2
#define PRIM_TRI   3
#define PRIM_QUAD  4

/* Not used yet, maybe later:
 */
struct prim_header {
   GLuint clipmask:16;
   GLuint edgeflags:4;		
   GLuint prim:3;		/* also == number of vertices */
   GLuint pad:9;

   GLfloat det;

   struct vertex_header *v[4];
};


/* Internal structs and helpers for the primitive clip/setup pipeline:
 */
struct prim_stage {
   struct prim_pipeline *pipe;
   struct prim_stage *next;
   struct vertex_header **tmp;

   void (*begin)( struct prim_stage * );

   void (*point)( struct prim_stage *,
		  struct prim_header * );

   void (*line)( struct prim_stage *,
		 struct prim_header * );

   void (*tri)( struct prim_stage *,
		struct prim_header * );

   void (*quad)( struct prim_stage *,
		 struct prim_header * );

   void (*end)( struct prim_stage * );
};


struct prim_stage *intel_prim_emit( struct prim_pipeline *pipe );
struct prim_stage *intel_prim_unfilled( struct prim_pipeline *pipe );
struct prim_stage *intel_prim_twoside( struct prim_pipeline *pipe );
struct prim_stage *intel_prim_clip( struct prim_pipeline *pipe );
struct prim_stage *intel_prim_flatshade( struct prim_pipeline *pipe );
struct prim_stage *intel_prim_cull( struct prim_pipeline *pipe );


void intel_prim_clear_vertex_indices( struct prim_pipeline *pipe );

/* Helpers
 */
static INLINE void calc_det( struct prim_header *prim )
{
   GLfloat ex = prim->v[0]->data[0] - prim->v[2]->data[0];
   GLfloat ey = prim->v[0]->data[1] - prim->v[2]->data[1];
   GLfloat fx = prim->v[1]->data[0] - prim->v[2]->data[0];
   GLfloat fy = prim->v[1]->data[1] - prim->v[2]->data[1];
   
   prim->det = ex * fy - ey * fx;
}



/* Get a writeable copy of a vertex:
 */
static INLINE struct vertex_header *
dup_vert( struct prim_stage *stage,
	  struct vertex_header *vert,
	  GLuint idx )
{   
   struct vertex_header *tmp = stage->tmp[idx];
   memcpy(tmp, vert, stage->pipe->vertex_size );
   tmp->index = ~0;
   return tmp;
}

#endif
#endif
