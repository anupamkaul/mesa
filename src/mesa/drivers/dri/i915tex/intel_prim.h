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

/* The prim pipeline probably needs to know about the internals of the
 * intel_draw struct.  Need to figure that out shortly.
 */

struct intel_draw;
struct intel_draw_state;
struct intel_render;
struct prim_pipeline;

#define PRIM_POINT 0x0
#define PRIM_LINE  0x1
#define PRIM_TRI   0x2


struct prim_draw_state {

   /* Draw state:
    */
   GLuint reduced_primitive:2;
   GLuint clipped_prims:1;
};


struct intel_render *intel_create_prim_render( struct intel_draw *draw );

GLboolean prim_set_state( struct intel_render *pipe,
			  struct intel_draw_state *draw );


void prim_set_hw_render( struct intel_render *pipe,
			 struct intel_render *render );



/***********************************************************************
 * Private structs and data for the intel_prim* files.
 */
#ifdef INTEL_PRIM_PRIVATE

struct prim_pipeline_stage;

struct prim_pipeline {
   struct intel_render render;
   struct intel_draw *draw;

   GLuint vertex_copy_size;
   
   struct prim_pipeline_stage *emit;
   struct prim_pipeline_stage *unfilled;
   struct prim_pipeline_stage *twoside;
   struct prim_pipeline_stage *clip;
   struct prim_pipeline_stage *flatshade;
   struct prim_pipeline_stage *cull;


   struct {
      struct prim_pipeline_stage *first;
      GLuint prim;

      struct intel_vb *vb;
      
   } input;
};


/* Carry a tiny bit of useful information around with the vertices.
 * This will end up getting sent to hardware in most cases, unless we
 * make data a pointer into a hardware-only vb.  That would compilcate
 * things on this end.  Upload is pretty cheap on the integrated
 * chipsets, so possibly not yet worth the effort.
 *
 * Would like to store a bit more, eg. unprojected clip-space
 * coordinates for clipping without the nasty unproject/unviewport
 * operations.  
 */
struct vertex_header {
   GLuint clipmask:12;
   GLuint edgeflag:1;
   GLuint pad:3;
   GLuint index:16;
   GLubyte data[];
};

/* Not used yet, maybe later:
 */
struct prim_header {
   GLfloat det;

   GLuint clipormask:16;
   GLuint edgeflags:3;
   GLuint pad:13;

   struct vertex_header *v0;
   struct vertex_header *v1;
   struct vertex_header *v2;
};


/* Internal structs and helpers for the primitive clip/setup pipeline:
 */
struct prim_pipeline_stage {
   struct prim_pipeline *pipeline;
   struct prim_pipeline_stage *next;
   struct vertex_header **tmp;

   void (*point)( struct prim_pipeline_stage *,
		  struct vertex_header * );

   void (*line)(  struct prim_pipeline_stage *,
		  struct vertex_header *,
		  struct vertex_header * );

   void (*tri)(   struct prim_pipeline_stage *,
		  struct vertex_header *,
		  struct vertex_header *,
		  struct vertex_header *); 
};


struct prim_pipeline_stage *intel_prim_emit( struct prim_pipeline *pipe );
struct prim_pipeline_stage *intel_prim_unfilled( struct prim_pipeline *pipe );
struct prim_pipeline_stage *intel_prim_twoside( struct prim_pipeline *pipe );
struct prim_pipeline_stage *intel_prim_clip( struct prim_pipeline *pipe );
struct prim_pipeline_stage *intel_prim_flatshade( struct prim_pipeline *pipe );
struct prim_pipeline_stage *intel_prim_cull( struct prim_pipeline *pipe );




/* Helpers
 */
static INLINE GLfloat 
calc_det( struct vertex_header *v0,
	  struct vertex_header *v1,
	  struct vertex_header *v2 )
{
   GLfloat ex = v0->data[0] - v2->data[0];
   GLfloat ey = v0->data[1] - v2->data[1];
   GLfloat fx = v1->data[0] - v2->data[0];
   GLfloat fy = v1->data[1] - v2->data[1];
   GLfloat cc = ex*fy - ey*fx;
   
   return cc;
}



/* Get a writeable copy of a vertex:
 */
static struct vertex_header *
dup_vert( struct prim_pipeline_stage *stage,
	  struct vertex_header *vert,
	  GLuint idx )
{   
   struct vertex_header *tmp = stage->tmp[idx];
   memcpy(tmp, vert, stage->pipeline->vertex_copy_size );
   tmp->index = ~0;
   return tmp;
}

#endif
#endif
