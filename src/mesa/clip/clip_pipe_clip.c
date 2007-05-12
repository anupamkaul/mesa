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
#include "macros.h"

#define CLIP_PRIVATE
#include "clip/clip_context.h"

#define CLIP_PIPE_PRIVATE
#include "clip/clip_pipe.h"

#define VF_PRIVATE
#include "vf/vf.h"

struct clipper {
   struct clip_pipe_stage stage;
   struct vertex_fetch *vf;

   GLuint active_user_planes;
   GLfloat (*plane)[4];
};

/* This is a bit confusing:
 */
static INLINE struct clipper *clipper_stage( struct clip_pipe_stage *stage )
{
   return (struct clipper *)stage;
}


#define LINTERP(T, OUT, IN) ((OUT) + (T) * ((IN) - (OUT)))


static void interp_attr( const struct vf_attr *a,
			 GLubyte *vdst,
			 GLfloat t,
			 const GLubyte *vin,
			 const GLubyte *vout )
{
   GLuint offset = a->vertoffset;
   GLfloat fin[4], fout[4], fdst[4];
   
   a->extract( a, fin, vin + offset );
   a->extract( a, fout, vout + offset );

   fdst[0] = LINTERP( t, fout[0], fin[0] );
   fdst[1] = LINTERP( t, fout[1], fin[1] );
   fdst[2] = LINTERP( t, fout[2], fin[2] );
   fdst[3] = LINTERP( t, fout[3], fin[3] );

   a->insert[4-1]( a, vdst + offset, fdst );
}




/* Interpolate between two vertices to produce a third.  Delves
 * slightly into the internals of the vf struct, but assumes too much
 * about the layout of the vertex to be included in the vf code
 * itself.
 */
static void interp( struct vertex_fetch *vf,
		    struct vertex_header *dst,
		    GLfloat t,
		    const struct vertex_header *out, 
		    const struct vertex_header *in )
{
   GLubyte *vdst = (GLubyte *)dst;
   const GLubyte *vin  = (const GLubyte *)in;
   const GLubyte *vout = (const GLubyte *)out;

   const struct vf_attr *a = vf->attr;
   const GLuint attr_count = vf->attr_count;
   GLuint j;

   /* Vertex header.
    */
   {
      assert(a[0].attrib == VF_ATTRIB_VERTEX_HEADER);
      dst->clipmask = 0;
      dst->edgeflag = 0;
      dst->pad = 0;
      dst->index = 0xffff;
   }

   /* Clip coordinates:  interpolate normally
    */
   {
      assert(a[1].format == EMIT_4F);
      interp_attr(&a[1], vdst, t, vin, vout);
   }

   /* Do the projective divide and insert window coordinates:
    */
   {
      const GLfloat *pos = (const GLfloat *)&vdst[4];
      GLfloat ndc[4];

      ndc[3] = 1.0 / pos[3];
      ndc[0] = pos[0] * ndc[3];
      ndc[1] = pos[1] * ndc[3];
      ndc[2] = pos[2] * ndc[3];

      /* vf module handles the viewport application.  XXX fix this
       * - vf should take (only) clip coordinates, and do the
       * projection/viewport in one go.
       */
      a[2].insert[4-1]( &a[2], vdst + a[2].vertoffset, ndc );
   }

   
   /* Other attributes
    */
   for (j = 3; j < attr_count; j++) {
      interp_attr(&a[j], vdst, t, vin, vout);
   }
}


#define CLIP_USER_BIT    0x40
#define CLIP_CULL_BIT    0x80


static INLINE GLfloat dot4( const GLfloat *a,
			    const GLfloat *b )
{
   GLfloat result = (a[0]*b[0] +
		     a[1]*b[1] +
		     a[2]*b[2] +
		     a[3]*b[3]);

   return result;
}


#if 0   
static INLINE void do_tri( struct clip_pipe_stage *next,
			   struct prim_header *header )
{
   GLuint i;
   for (i = 0; i < 3; i++) {
      GLfloat *ndc = (GLfloat *)(&header->v[i]->data[16]);
      _mesa_printf("ndc %f %f %f\n", ndc[0], ndc[1], ndc[2]);
      assert(ndc[0] >= -1 && ndc[0] <= 641);
      assert(ndc[1] >= 30 && ndc[1] <= 481);
   }
   _mesa_printf("\n");
   next->tri(next, header);
}
#endif


static void emit_poly( struct clip_pipe_stage *stage,
		       struct vertex_header **inlist,
		       GLuint n )
{
   struct prim_header header;
   GLuint i;

   for (i = 2; i < n; i++) {
      header.v[0] = inlist[0];
      header.v[1] = inlist[i-1];
      header.v[2] = inlist[i];
	
      {
	 GLuint tmp0 = header.v[0]->edgeflag;
	 GLuint tmp2 = header.v[2]->edgeflag;

	 if (i != 2)   header.v[0]->edgeflag = 0;
	 if (i != n-1) header.v[2]->edgeflag = 0;

	 stage->next->tri( stage->next, &header );

	 header.v[0]->edgeflag = tmp0;
	 header.v[2]->edgeflag = tmp2;
      }
   }
}


#if 0
static void emit_poly( struct clip_pipe_stage *stage )
{
   GLuint i;

   for (i = 2; i < n; i++) {
      header->v[0] = inlist[0];
      header->v[1] = inlist[i-1];
      header->v[2] = inlist[i];
	 
      stage->next->tri( stage->next, header );
   }
}
#endif


/* Clip a triangle against the viewport and user clip planes.
 */
static void
do_clip_tri( struct clip_pipe_stage *stage, 
	     struct prim_header *header,
	     GLuint clipmask )
{
   struct clipper *clipper = clipper_stage( stage );
   struct vertex_header *a[MAX_CLIPPED_VERTICES];
   struct vertex_header *b[MAX_CLIPPED_VERTICES];
   struct vertex_header **inlist = a;
   struct vertex_header **outlist = b;
   GLuint tmpnr = 0;
   GLuint n = 3;
   GLuint i;

   inlist[0] = header->v[0];
   inlist[1] = header->v[1];
   inlist[2] = header->v[2];

   /* XXX: Note stupid hack to deal with tnl's 8-bit clipmask.  Remove
    * this once we correctly use 16bit masks for userclip planes.
    */
   clipmask &= ~CLIP_CULL_BIT;
   if (clipmask & CLIP_USER_BIT) {
      clipmask &= ~CLIP_USER_BIT;
      clipmask |= clipper->active_user_planes;
   }

   while (clipmask && n >= 3) {
      GLuint plane_idx = ffs(clipmask)-1;
      const GLfloat *plane = clipper->plane[plane_idx];
      struct vertex_header *vert_prev = inlist[0];
      GLfloat dp_prev = dot4( (GLfloat *)vert_prev->data, plane );
      GLuint outcount = 0;

      clipmask &= ~(1<<plane_idx);

      inlist[n] = inlist[0]; /* prevent rotation of vertices */

      for (i = 1; i <= n; i++) {
	 struct vertex_header *vert = inlist[i];

	 GLfloat dp = dot4( (GLfloat *)vert->data, plane );

	 if (!IS_NEGATIVE(dp_prev)) {
	    outlist[outcount++] = vert_prev;
	 }

	 if (DIFFERENT_SIGNS(dp, dp_prev)) {
	    struct vertex_header *new_vert = clipper->stage.tmp[tmpnr++];
	    outlist[outcount++] = new_vert;

	    if (IS_NEGATIVE(dp)) {
	       /* Going out of bounds.  Avoid division by zero as we
		* know dp != dp_prev from DIFFERENT_SIGNS, above.
		*/
	       GLfloat t = dp / (dp - dp_prev);
	       interp( clipper->vf, new_vert, t, vert, vert_prev );
	       
	       /* Force edgeflag true in this case:
		*/
	       new_vert->edgeflag = 1;
	    } else {
	       /* Coming back in.
		*/
	       GLfloat t = dp_prev / (dp_prev - dp);
	       interp( clipper->vf, new_vert, t, vert_prev, vert );

	       /* Copy starting vert's edgeflag:
		*/
	       new_vert->edgeflag = vert_prev->edgeflag;
	    }
	 }

	 vert_prev = vert;
	 dp_prev = dp;
      }

      {
	 struct vertex_header **tmp = inlist;
	 inlist = outlist;
	 outlist = tmp;
	 n = outcount;
      }
   }

   /* Emit the polygon as triangles to the setup stage:
    */
   if (n >= 3)
      emit_poly( stage, inlist, n );
}


/* Clip a line against the viewport and user clip planes.
 */
static void
do_clip_line( struct clip_pipe_stage *stage,
	      struct prim_header *header,
	      GLuint clipmask )
{
   struct clipper *clipper = clipper_stage( stage );
   struct vertex_header *v0 = header->v[0];
   struct vertex_header *v1 = header->v[1];
   const GLfloat *pos0 = (const GLfloat *)(v0->data);
   const GLfloat *pos1 = (const GLfloat *)(v1->data);
   GLfloat t0 = 0;
   GLfloat t1 = 0;

   /* XXX: Note stupid hack to deal with tnl's 8-bit clipmask.  Remove
    * this once we correctly use 16bit masks for userclip planes.
    */
   clipmask &= ~CLIP_CULL_BIT;
   if (clipmask & CLIP_USER_BIT) {
      clipmask &= ~CLIP_USER_BIT;
      clipmask |= clipper->active_user_planes;
   }

   while (clipmask) {
      GLuint plane_idx = ffs(clipmask)-1;
      const GLfloat *plane = clipper->plane[plane_idx];

      clipmask &= ~(1<<plane_idx);

      const GLfloat dp0 = dot4( pos0, plane );
      const GLfloat dp1 = dot4( pos1, plane );

      if (dp1 < 0) {
	 GLfloat t = dp1 / (dp1 - dp0);
	 if (t > t1) t1 = t;
      } 

      if (dp0 < 0) {
	 GLfloat t = dp0 / (dp0 - dp1);
	 if (t > t0) t0 = t;
      }

      if (t0 + t1 >= 1.0)
	 return; /* discard */
   }

   if (v0->clipmask) {
      interp( clipper->vf, stage->tmp[0], t0, v0, v1 );
      header->v[0] = stage->tmp[0];
   }

   if (v1->clipmask) {
      interp( clipper->vf, stage->tmp[1], t1, v1, v0 );
      header->v[1] = stage->tmp[1];
   }

   stage->next->line( stage->next, header );
}



static void clip_begin( struct clip_pipe_stage *stage )
{
   struct clipper *clipper = clipper_stage(stage);
   GLuint nr = stage->pipe->draw->nr_planes;

   clipper->vf = stage->pipe->draw->vb.vf;
   
   /* Hacky bitmask to use when we hit CLIP_USER_BIT:
    */   
   clipper->active_user_planes = ((1<<nr)-1) & ~((1<<6)-1);

   stage->next->begin( stage->next );
}
     
static void
clip_point( struct clip_pipe_stage *stage, 
	    struct prim_header *header )
{
   if (header->v[0]->clipmask == 0) 
      stage->next->point( stage->next, header );
}


static void
clip_line( struct clip_pipe_stage *stage,
	   struct prim_header *header )
{
   GLuint clipmask = (header->v[0]->clipmask | 
		      header->v[1]->clipmask);
   
   if (clipmask == 0) {
      stage->next->line( stage->next, header );
   }
   else if ((header->v[0]->clipmask & 
	     header->v[1]->clipmask) == 0) {
      do_clip_line(stage, header, clipmask);
   }
}


static void
clip_tri( struct clip_pipe_stage *stage,
	  struct prim_header *header )
{
   GLuint clipmask = (header->v[0]->clipmask | 
		      header->v[1]->clipmask | 
		      header->v[2]->clipmask);
   
   if (clipmask == 0) {
      stage->next->tri( stage->next, header );
   }
   else if ((header->v[0]->clipmask & 
	     header->v[1]->clipmask & 
	     header->v[2]->clipmask) == 0) {
      do_clip_tri(stage, header, clipmask);
   }
}

static void clip_end( struct clip_pipe_stage *stage )
{
   struct clipper *clipper = clipper_stage(stage);

   clipper->vf = NULL;
   stage->next->end( stage->next );
}


struct clip_pipe_stage *clip_pipe_clip( struct clip_pipeline *pipe )
{
   struct clipper *clipper = CALLOC_STRUCT(clipper);

   clip_pipe_alloc_tmps( &clipper->stage, MAX_CLIPPED_VERTICES );

   clipper->stage.pipe = pipe;
   clipper->stage.begin = clip_begin;
   clipper->stage.point = clip_point;
   clipper->stage.line = clip_line;
   clipper->stage.tri = clip_tri;
   clipper->stage.reset_tmps = clip_pipe_reset_tmps;
   clipper->stage.end = clip_end;

   clipper->plane = pipe->draw->plane;

   return &clipper->stage;
}
