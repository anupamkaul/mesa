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


void tnl_copy_to_current( GLcontext *ctx, struct tnl_vtx_block *v ) 
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   GLint i, update = 0;

   assert(ctx->Driver.NeedFlush & FLUSH_UPDATE_CURRENT);

   for (i = 0 ; i < 16 ; i++)
      if (tnl->vtx.attrib_sz[i]) 
	 COPY_SZ( ctx->Current.Attrib[i], 
		  &(tnl->vtx.attrptr[i][0].f),
		  tnl->vtx.attrib_sz[i] );

   if (tnl->vtx.attrib_sz[VERT_ATTRIB_INDEX])
      ctx->Current.Index = tnl->vtx.attrptr[VERT_ATTRIB_INDEX][0].ui;

   if (tnl->vtx.attrib_sz[VERT_ATTRIB_EDGEFLAG])
      ctx->Current.EdgeFlag = tnl->vtx.attrptr[VERT_ATTRIB_EDGEFLAG][0].ui;

   for (i = 0 ; i < MAT_ATTRIB_MAX ; i++ ) {
      if (tnl->vtx.attrib_sz[i + VERT_ATTRIB_MAT_FRONT_EMISSION]) {
	 COPY_SZ( ctx->Light.Material.Attrib[i],
		  &(tnl->vtx.attrptr[i+VERT_ATTRIB_MAT_FRONT_EMISSION][0].f),
		  tnl->vtx.attrib_sz[i]);
	 update |= 1 << i;
      }

      if (update)
	 _mesa_update_material( ctx, update );
   }

   ctx->Driver.NeedFlush &= ~FLUSH_UPDATE_CURRENT;
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
static void build_prims( TNLcontext *tnl )
{
   int i, j;
   struct tnl_be *be = tnl->vtx.be;
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
   for (i = 0 ; i < tnl->vtx.be_count ; i++) {
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
	
   if (state != PRIM_OUTSIDE_BEGIN_END) {
      prim[j].end = tnl->vtx.initial_counter - tnl->vtx.counter;
   }
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

static GLuint copy_wrapped_verts( TNLcontext *tnl, GLfloat (*tmp)[15] )
{
   GLuint ovf, i;
   GLuint nr = (tnl->initial_counter - tnl->counter) - tnl->primlist[tnl->nrprims].start;

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

static void save_wrapped_verts( TNLcontext *tnl )
{
   tnl->wrap.nr_verts = copy_wrapped_verts( tnl );
}




static void emit_wrapped_verts( TNLcontext *tnl )
{
   /* Reemit saved vertices 
    * *** POSSIBLY IN NEW FORMAT
    *       --> Can't always extend at end of vertex
    */
   for (i = 0 ; i < nrverts; i++) {
      if (MESA_VERBOSE & DEBUG_VERTS) {
	 int j;
	 _mesa_debug(NULL, "re-emit vertex %d to %p\n", i, tnl->dmaptr);
	 if (MESA_VERBOSE & DEBUG_VERBOSE)
	    for (j = 0 ; j < tnl->vertex_size; j++) 
	       _mesa_debug(NULL, "\t%08x/%f\n", *(int*)&tmp[i][j], tmp[i][j]);
      }

      memcpy( tnl->dmaptr, tmp[i], tnl->vertex_size * 4 );
      tnl->dmaptr += tnl->vertex_size;
      tnl->counter--;
   }
}





/* Bind vertex buffer pointers, run pipeline:
 */
void _tnl_execute_buffer( TNLcontext *tnl, struct tnl_vtx_block *v )
{
   /* Bring back wrapped vertices:
    */
   revive_wrapped_verts( tnl, v );

   /* Build primitive list from begin/end events in buffer
    */
   build_prims( tnl, v );

   /* Bind the arrays and run the pipeline
    */
   bind_vertex_buffer( tnl, v );
   tnl->Driver.RunPipeline( ctx, &tnl->vb );

   /* Copy wrapped vertices for next time::
    */
   save_wrapped_verts( tnl, v );
}





