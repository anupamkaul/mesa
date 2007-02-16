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
#define INITIAL_INDEX_BUFSZ 2048


struct idx_context {
   GLcontext *ctx;
   TNLcontext *tnl;
   struct vertex_buffer *VB;

   struct _mesa_prim *current_prim;

   GLuint nr_prims;
   struct _mesa_prim prim[IDX_MAX_PRIM];	

   GLuint nr_indices;
   GLuint *indices;
   GLuint index_buffer_size;

   GLuint orig_VB_count;

   GLuint hw_max_indexable_verts;
   GLuint hw_max_indices;

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

   idx->index_buffer_size = INITIAL_INDEX_BUFSZ;
   idx->indices = _mesa_malloc(idx->index_buffer_size * sizeof(GLuint));
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
   assert(idx->nr_indices <= idx->index_buffer_size);

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

static void try_grow_indices( struct idx_context *idx, GLuint indices )
{
   GLuint new_size = idx->index_buffer_size * 2;

   while (new_size < indices) 
      new_size *= 2;

   if (new_size > idx->hw_max_indices)
      new_size = idx->hw_max_indices;

   if (new_size > idx->index_buffer_size &&
       new_size > indices) {

      GLuint old_size = idx->index_buffer_size;
      idx->index_buffer_size = new_size;
      idx->indices = _mesa_realloc(idx->indices, 
				   old_size * sizeof(GLuint),
				   new_size * sizeof(GLuint));
   }
}


/* We clip into the pre-allocated vertex buffer (held by t_vertex.c).
 * This may eventually fill up, so need to check after each clipped
 * primitive how we are doing.  Similarly, we may run out of space for
 * indices. For non-clipped prims, this is done once at the start of
 * drawing.
 */
static void check_flush( struct idx_context *idx, 
			 GLuint elts )
{
   GLuint indices = elts + idx->nr_indices + MAX_CLIPPED_VERTICES * 3;

   assert(idx->nr_prims <= IDX_MAX_PRIM);
   assert(idx->nr_indices <= idx->index_buffer_size);

   if (idx->hw_max_indexable_verts < idx->VB->Count + MAX_CLIPPED_VERTICES) {
      flush( idx );
   }

   if (idx->index_buffer_size < indices) {
      try_grow_indices( idx, indices );

      if (idx->index_buffer_size < indices) 
	 flush( idx );
   }

   assert (idx->index_buffer_size > indices);
   assert (idx->hw_max_indexable_verts > idx->VB->Count + MAX_CLIPPED_VERTICES);
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

static GLenum nr_elts( GLuint mode, GLuint count )
{
   switch (mode) {
   case GL_POINTS:
      return count;
   case GL_LINES:
      return count;
   case GL_LINE_LOOP:
      return count * 2 + 2;
   case GL_LINE_STRIP:
      return count * 2;
   case GL_TRIANGLES:
      return count;
   case GL_TRIANGLE_STRIP:
      return count * 3;
   case GL_TRIANGLE_FAN:
      return count * 3;
   case GL_QUADS:
      return (count / 4) * 6;
   case GL_QUAD_STRIP:
      return (count / 2) * 6;
   case GL_POLYGON:
      return count * 3;
   default:
      return 0;
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

   if (idx->VB->ClipOrMask)
      check_flush(idx, count);

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

   if (idx->VB->ClipOrMask)
      check_flush(idx, 0);
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
   /* If smooth shading, draw like a trifan which gives better
    * rasterization on some hardware.  Otherwise draw as two triangles
    * with provoking vertex in third position as required for flat
    * shading.
    */
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
      elt( idx, c );
      
      elt( idx, a );
      elt( idx, c );
      elt( idx, d );
   }
}

/* 
 */
static void polygon( struct idx_context *idx, 
		     const GLuint *elts, GLuint nr )
{
   GLuint i;

   for (i = 2; i < nr; i++) {
      elt( idx, elts[0] );
      elt( idx, elts[i-1] );
      elt( idx, elts[i] );
   }
}



/**********************************************************************/
/*                        Clip single primitives                      */
/**********************************************************************/

#define CTX_ARG struct idx_context *idx
#define GET_REAL_CTX GLcontext *ctx = idx->ctx;

#define CLIPPED_POLYGON( list, n ) polygon( idx, list, n )
#define CLIPPED_LINE( a, b ) line( idx, a, b )

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
   CHECK_FLUSH( idx ); \
} while (0)

#define RENDER_TRI( v1, v2, v3 )			\
do {							\
   GLubyte c1 = MASK(v1), c2 = MASK(v2), c3 = MASK(v3);	\
   GLubyte ormask = c1|c2|c3;				\
   if (!ormask)						\
      tri( idx, v1, v2, v3 );			\
   else if (!(c1 & c2 & c3 & CLIPMASK))			\
      clip_tri_4( idx, v1, v2, v3, ormask );		\
   CHECK_FLUSH( idx ); \
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
   CHECK_FLUSH( idx ); \
} while (0)

#define INIT(x) \
   set_mode( idx, x )

#define CTX_ARG struct idx_context *idx

#define LOCAL_VARS						\
   const GLuint * const elt = idx->VB->Elts;			\
   const GLubyte *mask = idx->VB->ClipMask;			\
   (void) elt; (void) mask;


/* Verts, clipping.
 */
#define CHECK_FLUSH(idx) check_flush(idx, 6)
#define MASK(x) mask[x]
#define TAG(x) x##_verts_clip
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"

/* Elts, clipping.
 */
#undef ELT
#undef TAG
#define ELT(x) elt[x]
#define TAG(x) x##_elts_clip
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"


/* Elts, no clipping.
 */
#undef MASK
#undef TAG
#undef CHECK_FLUSH
#define CHECK_FLUSH(idx) ((void)idx)
#define MASK(x) 0
#define TAG(x) x##_elts
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"


/* Verts, no clipping
 */
#undef ELT
#undef TAG
#define ELT(x) x
#define TAG(x) x##_verts
#include "t_vb_rendertmp.h"




/**********************************************************************/
/*              Clip and render whole vertex buffers                  */
/**********************************************************************/


static GLboolean run_index_render( GLcontext *ctx,
				   struct tnl_pipeline_stage *stage )
{
   struct idx_context *idx = (struct idx_context *)stage->privatePtr;
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   void (**tab)( struct idx_context *, GLuint, GLuint, GLuint );
   GLuint i;


   if (!tnl->Driver.Render.CheckIdxRender( ctx,
					   VB,
					   &idx->hw_max_indexable_verts,
					   &idx->hw_max_indices))
      return GL_TRUE;

   idx->orig_VB_count = VB->Count;
   idx->nr_prims = 0;
   idx->nr_indices = 0;
   idx->flatshade = (ctx->Light.ShadeModel == GL_FLAT);

   if (idx->index_buffer_size > idx->hw_max_indices) {
      idx->index_buffer_size = MIN2(INITIAL_INDEX_BUFSZ, 
				    idx->hw_max_indices);

      _mesa_free(idx->indices);

      idx->indices = _mesa_malloc(idx->index_buffer_size * 
				  sizeof(GLuint));
   }


   /* Allow the drivers to lock before projected verts are built so
    * that window coordinates are guarenteed not to change before
    * rendering.
    */
   tnl->Driver.Render.Start( ctx );

   tnl->clipspace.new_inputs |= VERT_BIT_POS;


   if (VB->ClipOrMask) {
      tab = VB->Elts ? render_tab_elts_clip : render_tab_verts_clip;


      /* In this case, what do we do?  Split the primitives?  Just
       * fail?  Currently make it the driver's responsibility to set
       * VB->Size small enough that this never happens.  Then any
       * splitting will be done earlier in t_draw.c.
       */
      assert (idx->hw_max_indexable_verts >= VB->Count * 1.2);
	 

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

      if (!VB->ClipOrMask)
	 check_flush( idx, 
		      nr_elts( prim->mode, prim->count ));
      
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
   run_index_render		/* run */
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
