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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */

#include "swrast_setup/swrast_setup.h"
#include "swrast/swrast.h"
      
#include "intel_context.h"
#include "intel_draw.h"
#include "intel_reg.h"
#include "intel_span.h"
#include "intel_vb.h"
#include "intel_state.h"
#include "intel_buffers.h"
#include "intel_batchbuffer.h"


struct swrast_render {
   struct intel_render render;
   struct intel_context *intel;
   GLuint prim;
};

static INLINE struct swrast_render *swrast_render( struct intel_render *render )
{
   return (struct swrast_render *)render;
}



static void tri( struct swrast_render *swrender,
		 const void *v0, 
		 const void *v1, 
		 const void *v2 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[3];

   /* Note: _swsetup_Translate() is a utility function and does not
    * actually require a swsetup context to be created.
    */
   _swsetup_Translate(ctx, v0, &v[0]);
   _swsetup_Translate(ctx, v1, &v[1]);
   _swsetup_Translate(ctx, v2, &v[2]);
   _swrast_Triangle(ctx, &v[0], &v[1], &v[2]);
}


static void line( struct swrast_render *swrender,
		  const void *v0,
		  const void *v1 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[2];

   _swsetup_Translate(ctx, v0, &v[0]);
   _swsetup_Translate(ctx, v1, &v[1]);
   _swrast_Line(ctx, &v[0], &v[1]);
}

static void point( struct swrast_render *swrender,
		   const void *v0 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[1];

   _swsetup_Translate(ctx, v0, &v[0]);
   _swrast_Point(ctx, &v[0]);
}



static void swrender_draw_prim( struct intel_render *render,
				GLuint start,
				GLuint nr )
{
   struct swrast_render *swrender = swrast_render( render );
   GLcontext *ctx = &swrender->intel->ctx;
   struct intel_vb *vb = swrender->intel->vb;
   GLuint i;

   intelSpanRenderStart(ctx);

   switch (swrender->prim) {
   case PRIM3D_POINTLIST:
      for (i = 0; i < nr; i++) {
	 point( swrender, 
		intel_vb_get_vertex(vb, i) );
      }
      break;

   case PRIM3D_LINELIST:
      for (i = 0; i+1 < nr; i += 2) {
	 line( swrender, 
	       intel_vb_get_vertex(vb, i),
	       intel_vb_get_vertex(vb, i+1) );
      }
      break;

   case PRIM3D_LINESTRIP:
      for (i = 0; i+1 < nr; i++) {
	 line( swrender, 
	       intel_vb_get_vertex(vb, i),
	       intel_vb_get_vertex(vb, i+1) );
      }
      break;

   case PRIM3D_TRILIST:
      for (i = 0; i+2 < nr; i += 3) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, i),
	      intel_vb_get_vertex(vb, i+1),
	      intel_vb_get_vertex(vb, i+2) );
      }
      break;

   case PRIM3D_TRISTRIP:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, i),
	      intel_vb_get_vertex(vb, i+1),
	      intel_vb_get_vertex(vb, i+2) );
      }
      break;

   case PRIM3D_TRIFAN:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, 0),
	      intel_vb_get_vertex(vb, i+1),
	      intel_vb_get_vertex(vb, i+2) );
      }
      break;

   case PRIM3D_POLY:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, i+1),
	      intel_vb_get_vertex(vb, i+2),
	      intel_vb_get_vertex(vb, 0) );
      }
      break;

   default:
      assert(0);
      break;
   }

   intelSpanRenderFinish(ctx);
}

static void swrender_draw_indexed_prim( struct intel_render *render,
					const GLuint *indices,
					GLuint nr )
{
   struct swrast_render *swrender = swrast_render( render );
   GLcontext *ctx = &swrender->intel->ctx;
   struct intel_vb *vb = swrender->intel->vb;
   GLuint i;

   intelSpanRenderStart(ctx);

   switch (swrender->prim) {
   case PRIM3D_POINTLIST:
      for (i = 0; i < nr; i++) {
	 point( swrender, 
		intel_vb_get_vertex(vb, indices[i]) );
      }
      break;

   case PRIM3D_LINELIST:
      for (i = 0; i+1 < nr; i += 2) {
	 line( swrender, 
	       intel_vb_get_vertex(vb, indices[i]),
	       intel_vb_get_vertex(vb, indices[i+1]) );
/* 	 _swrast_ResetLineStipple( ctx ); */
      }
      break;

   case PRIM3D_LINESTRIP:
      for (i = 0; i+1 < nr; i++) {
	 line( swrender, 
	       intel_vb_get_vertex(vb, indices[i]),
	       intel_vb_get_vertex(vb, indices[i+1]) );
      }
/*       _swrast_ResetLineStipple( ctx ); */
      break;

   case PRIM3D_TRILIST:
      for (i = 0; i+2 < nr; i += 3) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, indices[i]),
	      intel_vb_get_vertex(vb, indices[i+1]),
	      intel_vb_get_vertex(vb, indices[i+2]) );
      }
      break;

   case PRIM3D_TRISTRIP:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, indices[i]),
	      intel_vb_get_vertex(vb, indices[i+1]),
	      intel_vb_get_vertex(vb, indices[i+2]) );
      }
      break;

   case PRIM3D_TRIFAN:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, indices[0]),
	      intel_vb_get_vertex(vb, indices[i+1]),
	      intel_vb_get_vertex(vb, indices[i+2]) );
      }
      break;

   case PRIM3D_POLY:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      intel_vb_get_vertex(vb, indices[i+1]),
	      intel_vb_get_vertex(vb, indices[i+2]),
	      intel_vb_get_vertex(vb, indices[0]) );
      }
      break;

   default:
      assert(0);
      break;
   }

   intelSpanRenderFinish(ctx);
}

static void swrender_new_vertices( struct intel_render *render )
{
   struct swrast_render *swrender = swrast_render( render );
   intel_vb_validate_vertices( swrender->intel->vb,
			       VB_LOCAL_VERTS );
}


static void swrender_set_prim( struct intel_render *render,
			     GLuint prim )
{
   struct swrast_render *swrender = swrast_render( render );
   swrender->prim = prim;
}


static void swrender_start_render( struct intel_render *render )
{
   struct swrast_render *swrender = swrast_render( render );
   struct intel_context *intel = swrender->intel;


   _mesa_printf("%s\n", __FUNCTION__);

   /* Start a new batchbuffer, emit wait for pending flip.
    */
   intel_wait_flips(intel, 0);

   /* Emit some other synchronization stuff if necessary 
    */
   
   /* Flush the batchbuffer.  Later call to intelSpanRenderStart will
    * ensure we wait for completion.
    */
   if (intel->batch->segment_finish_offset[0] != 0)
      intel_batchbuffer_flush(intel->batch);
}

static void swrender_clear( struct intel_render *render )
{
}

static void swrender_flush( struct intel_render *render,
			    GLboolean finished_frame )
{
   struct swrast_render *swrender = swrast_render( render );

   _mesa_printf("%s\n", __FUNCTION__);

   _swrast_flush( &swrender->intel->ctx );
}

static void swrender_abandon_frame( struct intel_render *render )
{
/*    struct swrast_render *swrender = swrast_render( render ); */

   _mesa_printf("%s\n", __FUNCTION__);
}

static void swrender_destroy_context( struct intel_render *render )
{
   struct swrast_render *swrender = swrast_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(swrender);
}

struct intel_render *intel_create_swrast_render( struct intel_context *intel )
{
   struct swrast_render *swrender = CALLOC_STRUCT(swrast_render);

   /* XXX: Add casts here to avoid the compiler messages:
    */
   swrender->render.destroy_context = swrender_destroy_context;
   swrender->render.start_render = swrender_start_render;
   swrender->render.flush = swrender_flush;
   swrender->render.abandon_frame = swrender_abandon_frame;
   swrender->render.clear = swrender_clear;
   swrender->render.set_prim = swrender_set_prim;
   swrender->render.new_vertices = swrender_new_vertices;
   swrender->render.draw_prim = swrender_draw_prim;
   swrender->render.draw_indexed_prim = swrender_draw_indexed_prim;

   swrender->intel = intel;
   swrender->prim = 0;

   return &swrender->render;
}
