/* $XFree86$ */
/**************************************************************************

Copyright 2002 Tungsten Graphics Inc., Cedar Park, Texas.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */
#include "api_noop.h"
#include "api_arrayelt.h"
#include "context.h"
#include "mem.h"
#include "mmath.h"
#include "mtypes.h"
#include "enums.h"
#include "glapi.h"
#include "colormac.h"
#include "light.h"
#include "state.h"

#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "tnl/t_array_api.h"

static GLint min_sz[TNL_ATTRIB_MAX] = {
   2, /* VERT_ATTRIB_POS */
   1, /* VERT_ATTRIB_WEIGHT */
   3, /* VERT_ATTRIB_NORMAL */
   3, /* VERT_ATTRIB_COLOR0 */
   3, /* VERT_ATTRIB_COLOR1 */
   1, /* VERT_ATTRIB_FOG */
   1, /* VERT_ATTRIB_SIX */
   1, /* VERT_ATTRIB_SEVEN */
   2, /* VERT_ATTRIB_TEX0 */
   2, /* VERT_ATTRIB_TEX1 */
   2, /* VERT_ATTRIB_TEX2 */
   2, /* VERT_ATTRIB_TEX3 */
   2, /* VERT_ATTRIB_TEX4 */
   2, /* VERT_ATTRIB_TEX5 */
   2, /* VERT_ATTRIB_TEX6 */
   2, /* VERT_ATTRIB_TEX7 */
   4, /* MAT_ATTRIB_FRONT_AMBIENT */
   4, /* MAT_ATTRIB_FRONT_DIFFUSE */
   4, /* MAT_ATTRIB_FRONT_SPECULAR */
   4, /* MAT_ATTRIB_FRONT_EMISSION */
   1, /* MAT_ATTRIB_FRONT_SHININESS */
   3, /* MAT_ATTRIB_FRONT_INDEXES */
   4, /* MAT_ATTRIB_BACK_AMBIENT */
   4, /* MAT_ATTRIB_BACK_DIFFUSE */
   4, /* MAT_ATTRIB_BACK_SPECULAR */
   4, /* MAT_ATTRIB_BACK_EMISSION */
   3, /* MAT_ATTRIB_BACK_SHININESS */
   1, /* MAT_ATTRIB_BACK_INDEXES */
   1, /* VERT_ATTRIB_INDEX */
   1, /* VERT_ATTRIB_EDGEFLAG */
   1, /* VERT_ATTRIB_POINTSIZE */
   3, /* VERT_ATTRIB_BACK_COLOR0 */
   3, /* VERT_ATTRIB_BACK_COLOR1 */
   1, /* VERT_ATTRIB_BACK_INDEX */
};


/* NOTE: can precompute all this stuff:
 */
static void *init_current_arrays( GLcontext *ctx )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   int i;

   for (i = 0 ; i <= VERT_ATTRIB_EDGEFLAG ; i++) {
      struct gl_client_array *array = &tnl->exec.Current[i];

      array->Stride = 0;
      array->StrideB = 0;

      if (i < VERT_ATTRIB_MAX) {
	 array->Ptr = ctx->Current.Attrib[i];
/* 	 array->Size = sz_4f(ctx->Current.Attrib[i]); */
	 array->Type = GL_FLOAT;
      }
      else if (i <= VERT_ATTRIB_MAT_BACK_INDEXES) { 
	 array->Ptr =
	    ctx->Light.Material.Attrib[i-VERT_ATTRIB_MAT_BACK_INDEXES];
	 array->Size = min_sz[i];
	 array->Type = GL_FLOAT;
      }
      else if (i == VERT_ATTRIB_INDEX) {
	 array->Ptr = &ctx->Current.Index;
	 array->Size = 1;
	 array->Type = GL_UNSIGNED_INT;
      }
      else if (i == VERT_ATTRIB_EDGEFLAG) {
	 array->Ptr = &ctx->Current.Edgeflag;
	 array->Size = 1;
	 array->Type = GL_UNSIGNED_INT;
      }
   }
}


static GLint sz_4f( const GLfloat *f )
{
   if (f[3] != 1.0) return 4;
   if (f[2] != 0.0) return 3;
   if (f[1] != 0.0) return 2;
   return 1;
}

static GLint get_attrib_sz( GLcontext *ctx, GLint i )
{
   if (i <= VERT_ATTRIB_TEX7) 
      return sz_4f(ctx->Current.Attrib[i]);
   else 
      return min_sz[i];
}

static void bind_current( GLcontext *ctx, GLint i )
{
   get_current_array( ctx, i, &tnl->vb.Attrib[i] );
} 

/* Need to increment v->vert_store->ref_count???
 */ 
static void bind_vert( GLcontext *ctx, struct tnl_vtx_node *v, 
		       GLint i, GLint offset )
{
   struct gl_client_array *array = &tnl->vb.Attrib[i];
   array->Stride = v->vert_size * sizeof(union uif);
   array->StrideB = v->vert_size * sizeof(union uif);
   array->Ptr = v->verts[offset];
   array->Size = v->attrib_sz[i];
   array->Type = attrib_type[i]; /* always the same */
}


/* Allocate an array big enough, copy wrapped vertices to array,
 * extend last vertex to end:
 *
 * Allocated data will have to be from a tnl_vtx_store struct, and be
 * refcounted.
 *
 * Can special case wrap.nr == 1 --> bind_current.
 * Can special case where all wrapped vertices have the same value
 * for this attribute --> bind_current again.
 */
static void bind_wrapped( GLcontext *ctx, struct tnl_vtx_node *v, GLint i )
{
   struct gl_client_array *array = &tnl->vb.Attrib[i];

   get_wrap_array( ctx, i, &wrap );

   /* How does this get freed?
    */
   alloc_array( ctx, wrap->Size, &tnl->vb.Attrib[i], 
		v->vertex_count + tnl->wrap.nr );


   copy_wrap_array( &tnl->vb.Attrib[i], tnl->wrap.nr, &current );
   extend_array( &tnl->vb.Attrib[i], tnl->wrap.nr, 
		 v->vert_count + tnl->wrap.nr );
}


/* Current is like wrapped with stride==0.  Can they be unified?
 */
static void bind_current_and_vert( GLcontext *ctx, struct tnl_vtx_node *v, 
				   GLint i )
{
   struct gl_client_array current;

   get_current_array( ctx, i, &current );

   if (current->Size > v->attrib_sz[i]) {
      /* Need to allocate a wider array & copy:
       */
   }
   else {
      /* Bind vertex data in place, leaving room for wrapped data:
       */
      get_vert_array( ctx, i, &tnl->vb.Attrib[i], - tnl->wrap.nr );

      /* Copy current into wrap slots
       */
      copy_wrap_array( &tnl->vb.Attrib[i], tnl->wrap.nr, &current );
   }
}

static void bind_wrapped_and_vert( GLcontext *ctx, struct tnl_vtx_node *v, 
				   GLint i )
{
   struct gl_client_array wrap;

   get_wrap_array( ctx, i, &wrap );

   if (wrap->Size > v->attrib_sz[i]) {
      /* Need to allocate a wider array & copy:
       */
   }
   else {
      /* Bind vertex data in place, leaving room for wrapped data:
       */
      get_vert_array( ctx, i, &tnl->vb.Attrib[i], - tnl->wrap.nr );

      /* Copy current into wrap slots
       */
      copy_wrap_array( &tnl->vb.Attrib[i], tnl->wrap.nr, &wrap );
   }
}


/* This is the tricky bit, as always.  Pull saved (wrapped) vertices
 * out of their temporary storage and prepend them to the incoming
 * tnl_vtx_node.
 *
 * Will the vertices fit into the space left for them?  Do the vertex
 * sizes match?  If not, fallback.
 *
 * .. actually it looks like this should be part of the 'bind'
 * operation, avoiding a fallback in favor of a copy.
 *
 */
static bind_wrapped_and_verts( GLcontext *ctx, struct tnl_vtx_node *v )
{
   
   if (tnl->exec.wrap_nr) {
      for (i = 0 ; i < TNL_ATTRIB_MAX ; i++) {
	 if (tnl->wrap.attrib_sz[i] && v->attrib_sz[i])
	    bind_wrapped_and_vert( tnl, v, i );
	 else if (tnl->wrap.attrib_sz[i])
	    bind_wrapped( tnl, i );
	 else if (v->attrib_sz[i]) 
	    bind_current_and_vert( tnl, v, i );
	 else {
	    /* This is already done */
 	    /* bind_current( tnl, v, i ); */
	 }
      }
   }
   else {
      for (i = 0 ; i < TNL_ATTRIB_MAX ; i++) {
	 if (v->attrib_sz[i]) 
	    bind_vert( tnl, v, i );
	 else
	    bind_current( tnl, v, i );
      }
   }

   for (i = 0 ; i < TNL_ATTRIB_MAX; i++)
      tnl->vb.Attrib[i] = &tnl->exec.Attrib[i];

   tnl->vb.Count = v->vert_count + tnl->exec.wrap_nr;
   tnl->vb.Elts = 0;
   tnl->vb.Primitive = &tnl->exec.Primitive;
   tnl->vb.NrPrimitives = tnl->exec.NrPrimitives;
   tnl->vb.NormalLengthPtr = 0;
   tnl->vb.PointSizePtr = 0;
}

static GLboolean discrete_gl_prim[GL_POLYGON+1] = {
   1,	/* 0 points */
   1,	/* 1 lines */
   0,	/* 2 line_strip */
   0,	/* 3 line_loop */
   1,	/* 4 tris */
   0,	/* 5 tri_fan */
   0,	/* 6 tri_strip */
   1,	/* 7 quads */
   0,	/* 8 quadstrip */
   0,	/* 9 poly */
};

static GLint prim_modulo[GL_POLYGON+1] = {
   1,	/* 0 points */
   2,	/* 1 lines */
   0,	/* 2 line_strip */
   0,	/* 3 line_loop */
   3,	/* 4 tris */
   0,	/* 5 tri_fan */
   0,	/* 6 tri_strip */
   4,	/* 7 quads */
   0,	/* 8 quadstrip */
   0,	/* 9 poly */
};

/* Optimize primitive list where possible.
 */
static void optimize_prims( TNLcontext *tnl )
{
   struct tnl_prim *prim = tnl->primlist;

   if (tnl->nrprims <= 1)
      return;

   for (j = 0, i = 1 ; i < tnl->nrprims; i++) {
      int pj = prim[j].prim & 0xf;
      int pi = prim[i].prim & 0xf;
      
      if (pj == pi && 
	  discrete_gl_prim[pj] &&
	  prim[i].start == prim[j].end &&
	  (prim[i].end - prim[i].start) % prim_modulo[pi] == 0) {
	 prim[j].end = prim[i].end;
      }
      else {
	 j++;
	 if (j != i) prim[j] = prim[i];
      }
   }

   tnl->nrprims = j+1;
}



/* Build a primitive list from the begin/end buffer.
 */
static void build_prims( TNLcontext *tnl, struct tnl_vtx_node *v )
{
   int i, j;
   struct tnl_be *be = v->be;
   struct tnl_prim *prim = tnl->primlist;
   GLenum state = ctx->Driver.CurrentExecPrimitive;

   tnl->nrprims = 0;

   /* Initialize first prim if inside begin/end
    */
   if (state != PRIM_OUTSIDE_BEGIN_END) {
      prim[j].start = be[i].idx;
      prim[j].mode = state;
   }


   /* Convert begin/ends into prims
    */
   for (i = 0 ; i < v->be_count ; i++) {
      switch (be[i].type) {
      case TNL_BEGIN:
	 if (state != PRIM_OUTSIDE_BEGIN_END ||
	     be[i].mode > GL_POLYGON) {
	    error = 1;
	 }
	 else { 
	    prim[j].start = be[i].idx;
	    prim[j].mode = be[i].mode | PRIM_BEGIN;
	    state = be[i].mode;
	 }
	 break;
      case TNL_END:
	 if (state == PRIM_OUTSIDE_BEGIN_END) {
	    error = 1;
	 }
	 else {
	    prim[j].mode |= PRIM_END;
	    prim[j].end = be[i].idx;
	 }
      }
   }
	
   if (state != PRIM_OUTSIDE_BEGIN_END) 
      prim[j].end = v->vert_count;
}


static void copy_vertex( TNLcontext *tnl, GLuint n, GLfloat *dst )
{
   GLuint i;
   GLfloat *src = (GLfloat *)(tnl->dma.current.address + 
			      tnl->dma.current.ptr + 
			      (tnl->primlist[tnl->nrprims].start + n) * 
			      tnl->vertex_size * 4);

   if (MESA_VERBOSE & DEBUG_VFMT) 
      _mesa_debug(NULL, "copy_vertex %d\n", 
	      tnl->primlist[tnl->nrprims].start + n);

   for (i = 0 ; i < tnl->vertex_size; i++) {
      dst[i] = src[i];
   }
}

static GLuint copy_wrapped_verts( TNLcontext *tnl, struct tnl_vtx_node *v )
{
   GLuint ovf, i;
   GLuint nr = v->vert_count - tnl->primlist[tnl->nrprims].start;

   if (MESA_VERBOSE & DEBUG_VFMT)
      _mesa_debug(NULL, "%s %d verts\n", __FUNCTION__, nr);

   switch( tnl->prim[0] )
   {
   case GL_POINTS:
      return 0;
   case GL_LINES:
      ovf = nr&1;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( tnl, nr-ovf+i, tmp[i] );
      return i;
   case GL_TRIANGLES:
      ovf = nr%3;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( tnl, nr-ovf+i, tmp[i] );
      return i;
   case GL_QUADS:
      ovf = nr&3;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( tnl, nr-ovf+i, tmp[i] );
      return i;
   case GL_LINE_STRIP:
      if (nr == 0) 
	 return 0;
      copy_vertex( tnl, nr-1, tmp[0] );
      return 1;
   case GL_LINE_LOOP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      if (nr == 0) 
	 return 0;
      else if (nr == 1) {
	 copy_vertex( tnl, 0, tmp[0] );
	 return 1;
      } else {
	 copy_vertex( tnl, 0, tmp[0] );
	 copy_vertex( tnl, nr-1, tmp[1] );
	 return 2;
      }
   case GL_TRIANGLE_STRIP:
      /* FIXME:  parity.
       */
      ovf = MIN2( nr-1, 2 );
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( tnl, nr-ovf+i, tmp[i] );
      return i;
   case GL_QUAD_STRIP:
      ovf = MIN2( nr-1, 2 );
      if (nr > 2) ovf += nr&1;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( tnl, nr-ovf+i, tmp[i] );
      return i;
   default:
      assert(0);
      return 0;
   }

}

static void save_wrapped_verts( TNLcontext *tnl, struct tnl_vtx_node *v )
{
   if (ctx->Driver.CurrentExecPrimitive != PRIM_OUTSIDE_BEGIN_END)
      tnl->wrap.nr_verts = copy_wrapped_verts( tnl, v );
   else
      tnl->wrap.nr_verts = 0;
}



/* This must be done for each node.  To do it only when outside
 * begin/end, would need to take wrapped vertices into account (which
 * wouldn't necessarily be a bad thing).
 */
static void copy_to_current( GLcontext *ctx, struct tnl_vtx_node *v ) 
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   GLint i;
   const union uif *vert = v->verts + (v->vert_count-1) * v->vert_size;
   const GLubyte *attrib_sz = v->attrib_sz;

   for (i = 0 ; i < 16 ; i++)
      if (attrib_sz[i]) {
	 COPY_SZ( ctx->Current.Attrib[i], &(vert->f), attrib_sz[i] );
	 vert += attrib_sz[i];
      }

   for (i = 0 ; i < MAT_ATTRIB_MAX ; i++ ) {
      GLuint update = 0;

      if (attrib_sz[i + VERT_ATTRIB_MAT_FRONT_EMISSION]) {
	 COPY_SZ( ctx->Light.Material.Attrib[i], &(vert->f), attrib_sz[i]);
	 vert += attrib_sz[i];
	 update |= 1 << i;
      }

      if (update)
	 _mesa_update_material( ctx, update );
   }

   if (attrib_sz[VERT_ATTRIB_INDEX]) {
      ctx->Current.Index = vert->ui;
      vert++;
   }

   if (attrib_sz[VERT_ATTRIB_EDGEFLAG]) {
      ctx->Current.EdgeFlag = vert->ui;
      vert++;
   }

   

   if (ctx->Light.ColorMaterialEnabled) {
   }
}




/* Bind vertex buffer pointers, run pipeline:
 */
void _tnl_execute_buffer( GLcontext *ctx, struct tnl_vtx_block *v )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   /* Bring back wrapped vertices and bind (with the new verts) into the VB:
    */
   emit_wrapped_verts_bind_vb( tnl, v );

   /* Build primitive list from begin/end events in buffer (taking
    * into account wrapped vertices).
    *
    * Also updates ctx->Driver.CurrentExecPrimitive:
    */
   build_prims( tnl, v );

   /* Run the pipeline:
    */
   tnl->Driver.RunPipeline( ctx, &tnl->vb );

   /* Copy wrapped vertices for next time.  Need to save from bound
    * data, not from incoming verts which would be easier.
    */
   save_wrapped_verts( tnl, v );

   /* Copy to current:
    */
   copy_to_current( ctx, v );
}





