/*
 * Mesa 3-D graphics library
 * Version:  6.5
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */


/* Transform post-tnl primitives to indexed primitives of either
 * GL_TRIANGLES, GL_LINES or GL_POINTS.
 *
 * Clipped vertices are emitted to the end of the VB and the new
 * triangles and lines are incorporated into the generated primitives.
 */




#include "glheader.h"
#include "context.h"
#include "enums.h"
#include "macros.h"
#include "imports.h"
#include "mtypes.h"
#include "t_pipeline.h"

#define IDX_MAX_PRIM 64
#define IDX_MAX_INDEX 2048	/* xxx: fix me! */

struct idx_context {
   GLcontext *ctx;
   TNLcontext *tnl;
   struct vertex_buffer *VB;

   struct _mesa_prim *current_prim;

   GLuint nr_prims;
   struct _mesa_prim prim[IDX_MAX_PRIM];	

   GLuint nr_indices;
   GLuint indices[2048];
   GLuint index_buffer_size;

   GLuint vb_size;
   GLuint orig_VB_count;

   GLboolean flatshade;
};

		     




static GLboolean init_idx( GLcontext *ctx,
			   struct tnl_pipeline_stage *stage )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   struct idx_context *idx = CALLOC_STRUCT(idx_context);

   if (!idx)
      return GL_FALSE;

   stage->privatePtr = (void *)idx;

   idx->index_buffer_size = IDX_MAX_INDEX;

   idx->VB = VB;
   idx->tnl = tnl;
   idx->ctx = ctx;

   return GL_TRUE;
}


static void free_idx( struct tnl_pipeline_stage *stage )
{
   free(stage->privatePtr);
}

/* Have to re-emit the whole vertex buffer in clipped rendering.
 * Important to ensure there is a reasonable amount of room for
 * clipped vertices otherwise this will be a real problem.
 */
static void flush( struct idx_context *idx )
{
   assert(idx->nr_prims <= IDX_MAX_PRIM);
   assert(idx->nr_indices <= IDX_MAX_INDEX);

   if (idx->VB->ClipOrMask) {
      idx->tnl->Driver.Render.EmitBuiltVertices( idx->ctx, idx->VB->Count );
   } 

   if (idx->current_prim) {
      idx->current_prim->count = idx->nr_indices - idx->current_prim->start;
   }

   if (idx->nr_prims) {
      idx->tnl->Driver.Render.EmitPrims( idx->ctx, 
					 idx->prim, idx->nr_prims,
					 idx->indices, idx->nr_indices );
   }

   idx->VB->Count = idx->orig_VB_count;
   idx->nr_prims = 0;
   idx->nr_indices = 0;
   idx->current_prim = NULL;
}

static void check_flush( struct idx_context *idx )
{
   if (idx->index_buffer_size - idx->nr_indices < MAX_CLIPPED_VERTICES ||
       idx->vb_size - idx->VB->Count < MAX_CLIPPED_VERTICES) {
      _mesa_printf("forced flush\n");
      flush( idx );
   }
}

static void elt( struct idx_context *idx, GLuint i )
{
   idx->indices[idx->nr_indices++] = i;
}

static GLenum reduce_mode( GLuint mode )
{
   switch (mode) {
   case GL_POINTS:
      return GL_POINTS;
   case GL_LINES:
   case GL_LINE_LOOP:
   case GL_LINE_STRIP:
      return GL_LINES;
   default:
      return GL_TRIANGLES;
   }
}

static void set_mode( struct idx_context *idx, GLuint flags )
{
   GLenum mode = reduce_mode(flags & PRIM_MODE_MASK);

   if (!idx->current_prim || 
       idx->current_prim->mode != mode) {

      if (idx->nr_prims == IDX_MAX_PRIM) {
	 _mesa_printf("forced flush 2\n");
	 flush(idx);
      }

      idx->current_prim = &idx->prim[idx->nr_prims++];
      idx->current_prim->mode = mode;
      idx->current_prim->start = idx->nr_indices;
      idx->current_prim->begin = 1;
      idx->current_prim->end = 1;
      idx->current_prim->weak = 0;
   }
}


static void points( struct idx_context *idx, GLuint start, GLuint count )
{
   const GLuint *elts = idx->VB->Elts;
   const GLubyte *mask = idx->VB->ClipMask;
   GLuint i;

   if (elts) {
      for (i = 0; i < count; i++) {
	 GLuint e = elts[i];
	 if ( mask[e] == 0 ) 
	    elt( idx, e );
      }
   }
   else {
      for (i = 0; i < count; i++) {
	 if ( mask[i] == 0 ) 
	    elt( idx, i );
      }
   }      
}

static void line( struct idx_context *idx, GLuint a, GLuint b )
{
   elt( idx, a );
   elt( idx, b );
}

static void tri( struct idx_context *idx, GLuint a, GLuint b, GLuint c )
{
   elt( idx, a );
   elt( idx, b );
   elt( idx, c );
}

static void quad( struct idx_context *idx, 
		  GLuint a, GLuint b, 
		  GLuint c, GLuint d )
{
   if (idx->flatshade) {
      elt( idx, a );
      elt( idx, b );
      elt( idx, d );
      
      elt( idx, b );
      elt( idx, c );
      elt( idx, d );
   }
   else {
      elt( idx, a );
      elt( idx, b );
      elt( idx, d );
      
      elt( idx, b );
      elt( idx, c );
      elt( idx, d );
   }
}

static void clipped_poly( struct idx_context *idx, 
			  const GLuint *elts, GLuint nr )
{
   GLuint i;

   for (i = 0; i < nr; i++)
      elt( idx, elts[i] );

   check_flush( idx );
}


static void clipped_line( struct idx_context *idx, GLuint a, GLuint b )
{
   elt( idx, a );
   elt( idx, b );

   check_flush( idx );
}



/**********************************************************************/
/*                        Clip single primitives                      */
/**********************************************************************/

#define CTX_ARG struct idx_context *idx
#define GET_REAL_CTX GLcontext *ctx = idx->ctx;

#define CLIPPED_POLYGON( list, n ) clipped_poly( idx, list, n )
#define CLIPPED_LINE( a, b ) clipped_line( idx, a, b )

#define W(i) coord[i][3]
#define Z(i) coord[i][2]
#define Y(i) coord[i][1]
#define X(i) coord[i][0]
#define SIZE 4
#define TAG(x) x##_4
#include "t_vb_cliptmp.h"



/**********************************************************************/
/*              Clip and render whole begin/end objects               */
/**********************************************************************/


/* NOTE: This does not include the CLIP_USER_BIT, because computing
 * the and-mask of several clipmasks for culling purposes does not
 * work for userplanes, which all share a single bit in the clipmask.
 */
#define CLIPMASK (CLIP_FRUSTUM_BITS | CLIP_CULL_BIT)


/* Vertices, with the possibility of clipping.
 */
#define RENDER_POINTS( start, count ) \
   points( idx, start, count )

#define RENDER_LINE( v1, v2 )			\
do {						\
   GLubyte c1 = MASK(v1), c2 = MASK(v2);	\
   GLubyte ormask = c1|c2;			\
   if (!ormask)					\
      line( idx, v1, v2 );			\
   else if (!(c1 & c2 & CLIPMASK))		\
      clip_line_4( idx, v1, v2, ormask );	\
} while (0)

#define RENDER_TRI( v1, v2, v3 )			\
do {							\
   GLubyte c1 = MASK(v1), c2 = MASK(v2), c3 = MASK(v3);	\
   GLubyte ormask = c1|c2|c3;				\
   if (!ormask)						\
      tri( idx, v1, v2, v3 );			\
   else if (!(c1 & c2 & c3 & CLIPMASK))			\
      clip_tri_4( idx, v1, v2, v3, ormask );		\
} while (0)

#define RENDER_QUAD( v1, v2, v3, v4 )			\
do {							\
   GLubyte c1 = MASK(v1), c2 = MASK(v2);		\
   GLubyte c3 = MASK(v3), c4 = MASK(v4);		\
   GLubyte ormask = c1|c2|c3|c4;			\
   if (!ormask)						\
      quad( idx, v1, v2, v3, v4 );			\
   else if (!(c1 & c2 & c3 & c4 & CLIPMASK)) 		\
      clip_quad_4( idx, v1, v2, v3, v4, ormask );	\
} while (0)

#define INIT(x) \
   set_mode( idx, x )

#define CTX_ARG struct idx_context *idx

#define LOCAL_VARS						\
   const GLuint * const elt = idx->VB->Elts;			\
   const GLubyte *mask = idx->VB->ClipMask;			\
   (void) elt; (void) mask;


#define MASK(x) mask[x]
#define TAG(x) x##_verts_clip
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"

/* Elts, with the possibility of clipping.
 */
#undef ELT
#undef TAG
#define ELT(x) elt[x]
#define TAG(x) x##_elts_clip
#include "t_vb_rendertmp.h"


/**********************************************************************/
/*                  Render whole begin/end objects                    */
/**********************************************************************/

#define NEED_EDGEFLAG_SETUP 0


/* Vertices, no clipping.
 */
#define RENDER_POINTS( start, count ) \
   points( idx, start, count )

#define RENDER_LINE( v1, v2 ) \
   line( idx, v1, v2 )

#define RENDER_TRI( v1, v2, v3 ) \
   tri( idx, v1, v2, v3 )

#define RENDER_QUAD( v1, v2, v3, v4 ) \
   quad( idx, v1, v2, v3, v4 )

#define INIT(x) \
   set_mode( idx, x )

#define LOCAL_VARS						\
   const GLuint * const elt = idx->VB->Elts;			\
   (void) elt; 

#define TAG(x) x##_verts
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"


/* Elts, no clipping.
 */
#undef ELT
#define TAG(x) x##_elts
#define ELT(x) elt[x]
#include "t_vb_rendertmp.h"


/**********************************************************************/
/*              Clip and render whole vertex buffers                  */
/**********************************************************************/


static GLboolean run_render( GLcontext *ctx,
			     struct tnl_pipeline_stage *stage )
{
   struct idx_context *idx = (struct idx_context *)stage->privatePtr;
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   void (**tab)( struct idx_context *, GLuint, GLuint, GLuint );
   GLuint i;


   if (!tnl->Driver.Render.CheckIdxRender( ctx, VB ))
      return GL_TRUE;

   idx->orig_VB_count = VB->Count;
   idx->nr_prims = 0;
   idx->nr_indices = 0;


   /* Allow the drivers to lock before projected verts are built so
    * that window coordinates are guarenteed not to change before
    * rendering.
    */
   tnl->Driver.Render.Start( ctx );

   tnl->clipspace.new_inputs |= VERT_BIT_POS;


   if (VB->ClipOrMask) {
      tab = VB->Elts ? render_tab_elts_clip : render_tab_verts_clip;

      idx->vb_size = tnl->Driver.Render.GetMaxVBSize( ctx );

      /* The driver must guarentee this, it is not our fault if this
       * fails:
       */
      assert (idx->vb_size >= VB->Count * 1.2);
	 

      /* Have to build these before clipping.  Not ideal, but there
       * you have it.
       *
       * TODO: Modify t_vertex.c to request a VBO from the client and
       * place vertices in that rather than its own memory.
       */
      tnl->Driver.Render.BuildVertices( ctx, 0, VB->Count, ~0 );
   }
   else {
      tab = VB->Elts ? render_tab_elts : render_tab_verts;

      /* Can build and emit vertices to VBO now:
       */
      idx->tnl->Driver.Render.BuildAndEmitVertices( idx->ctx, idx->VB->Count );
   }


   for (i = 0 ; i < VB->PrimitiveCount ; i++)
   {
      const struct _mesa_prim *prim = &VB->Primitive[i];
      
      if (prim->count)
	 tab[prim->mode]( idx, 
			  prim->start, 
			  prim->start + prim->count, 
			  _tnl_translate_prim(prim));
   }

   flush( idx );

   tnl->Driver.Render.Finish( ctx );

   return GL_FALSE;		/* finished the pipe */
}


/**********************************************************************/
/*                          Render pipeline stage                     */
/**********************************************************************/





const struct tnl_pipeline_stage _tnl_indexed_render_stage =
{
   "indexed render",		/* name */
   NULL,			/* private data */
   init_idx,			/* creator */
   free_idx,			/* destructor */
   NULL,			/* validate */
   run_render			/* run */
};





#if 0
/* Helper to renumber indices for a driver which is doing uploads
 * which omit clipped vertices.  Currently not used.
 */
void idx_renumber_elts( struct veretx_buffer *VB,
			GLuint nr_elts )
{
   GLuint *renumber = malloc(VB->Count * sizeof(GLuint));
   GLuint i, j;

   for (i = j = 0; i < VB->Count; i++) {
      if (!VB->ClipMask[i])
	 renumber[i] = j++;
   }
   
   for (i = 0; i < nr_elts; i++)
      VB->Elts[i] = renumber[VB->Elts[i]];

   free(renumber);
}
#endif
