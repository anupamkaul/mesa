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
#include "colormac.h"

#include "swrast/swrast.h"
      
#include "intel_context.h"
#include "intel_batchbuffer.h"
#include "intel_swapbuffers.h"
#include "intel_frame_tracker.h"
#include "clip/clip_context.h"
#include "intel_reg.h"
#include "intel_span.h"
#include "intel_vb.h"
#include "intel_state.h"


struct swrast_render {
   struct clip_render render;
   struct intel_context *intel;

   struct vertex_fetch *vf;

   GLubyte *hw_verts;
   GLuint hw_vert_buffer_size;
   GLuint hw_vert_size;

   GLenum prim;
};

static INLINE struct swrast_render *swrast_render( struct clip_render *render )
{
   return (struct swrast_render *)render;
}


static const GLubyte *get_vertex( struct swrast_render *swrender,
				  GLuint i )
{
   return swrender->hw_verts + i * swrender->hw_vert_size;
}


static void *swrender_allocate_vertices( struct clip_render *render,
					 GLuint vertex_size,
					 GLuint nr_vertices )
{
   struct swrast_render *swrender = swrast_render( render );

   swrender->vf = clip_get_hw_vf( swrender->intel->clip );
   swrender->hw_vert_size = vertex_size;
   swrender->hw_verts = MALLOC( nr_vertices * swrender->hw_vert_size );

   assert(vertex_size == swrender->vf->vertex_stride);

   return swrender->hw_verts;
}


static void swrender_release_vertices( struct clip_render *render, 
				       void *hw_verts)
{
   struct swrast_render *swrender = swrast_render( render );

   swrender->vf = NULL;

   if (swrender->hw_verts) {
      FREE( swrender->hw_verts );
      swrender->hw_verts = NULL;
   }
}






/**
 * Populate a swrast SWvertex from an attrib-style vertex.
 */
static void translate( struct swrast_render *swrender,
		       const void *vertex, 
		       SWvertex *dest )
{
   struct vertex_fetch *vf = swrender->vf;
   struct intel_context *intel = swrender->intel;
   GLcontext *ctx = &intel->ctx;
   const GLfloat *m = ctx->Viewport._WindowMap.m;
   GLfloat tmp[4];
   GLuint i;


   /* Need to use the hardware vertex attributes to pluck apart this
    * vertex, also need the hardware viewport matrix installed.
    */
   vf_get_attr( vf, vertex, VF_ATTRIB_POS, NULL, tmp );

   dest->win[0] = m[0]  * tmp[0] + m[12];
   dest->win[1] = m[5]  * tmp[1] + m[13];
   dest->win[2] = m[10] * tmp[2] + m[14];
   dest->win[3] =         tmp[3];


   for (i = 0 ; i < ctx->Const.MaxTextureCoordUnits ; i++)
      vf_get_attr( vf, vertex, VF_ATTRIB_TEX0+i,
		ctx->Current.Attrib[VERT_ATTRIB_TEX0+i],
		dest->attrib[FRAG_ATTRIB_TEX0 + i] );

#if 0
   for (i = 0 ; i < ctx->Const.MaxVarying ; i++)
      vf_get_attr( vf, vertex, VF_ATTRIB_GENERIC0+i,
		dest->attrib[FRAG_ATTRIB_VAR0 + i] );
#endif


   vf_get_attr( vf, vertex, VF_ATTRIB_COLOR0,
	     ctx->Current.Attrib[VERT_ATTRIB_COLOR0],
	     tmp );
   UNCLAMPED_FLOAT_TO_RGBA_CHAN( dest->color, tmp );

   vf_get_attr( vf, vertex, VF_ATTRIB_COLOR1,
	     ctx->Current.Attrib[VERT_ATTRIB_COLOR1],
	     tmp );
   UNCLAMPED_FLOAT_TO_RGBA_CHAN( dest->specular, tmp );

   vf_get_attr( vf, vertex, VF_ATTRIB_FOG,
	     ctx->Current.Attrib[VERT_ATTRIB_FOG],
	     tmp );
   dest->fog = tmp[0];

   vf_get_attr( vf, vertex, VF_ATTRIB_COLOR_INDEX,
	     ctx->Current.Attrib[VERT_ATTRIB_COLOR_INDEX],
	     tmp );
   dest->index = tmp[0];

   /* XXX: default here is bogus.  But if it isn't in the hardware
    * vertex, swrast shouldn't need it either.
    */
   vf_get_attr( vf, vertex, VF_ATTRIB_POINTSIZE, tmp, tmp);
   dest->pointSize = tmp[0];
}



static void tri( struct swrast_render *swrender,
		 const void *v0, 
		 const void *v1, 
		 const void *v2 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[3];

   /* Note: translate() is a utility function and does not
    * actually require a swsetup context to be created.
    */
   translate(swrender, v0, &v[0]);
   translate(swrender, v1, &v[1]);
   translate(swrender, v2, &v[2]);
   _swrast_Triangle(ctx, &v[0], &v[1], &v[2]);
}


static void line( struct swrast_render *swrender,
		  const void *v0,
		  const void *v1 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[2];

   translate(swrender, v0, &v[0]);
   translate(swrender, v1, &v[1]);
   _swrast_Line(ctx, &v[0], &v[1]);
}

static void point( struct swrast_render *swrender,
		   const void *v0 )
{
   GLcontext *ctx = &swrender->intel->ctx;
   SWvertex v[1];

   translate(swrender, v0, &v[0]);
   _swrast_Point(ctx, &v[0]);
}



static void swrender_draw_prim( struct clip_render *render,
				GLuint start,
				GLuint nr )
{
   struct swrast_render *swrender = swrast_render( render );
   struct intel_context *intel = swrender->intel;
   GLuint i;

   intel_frame_set_mode( intel->ft, INTEL_FT_SWRAST );

   intel_do_SpanRenderStart( intel );

   switch (swrender->prim) {
   case GL_POINTS:
      for (i = 0; i < nr; i++) {
	 point( swrender, 
		get_vertex(swrender, start+i) );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < nr; i += 2) {
	 line( swrender, 
	       get_vertex(swrender, start+i),
	       get_vertex(swrender, start+i+1) );
      }
      break;

   case GL_LINE_STRIP:
      for (i = 0; i+1 < nr; i++) {
	 line( swrender, 
	       get_vertex(swrender, start+i),
	       get_vertex(swrender, start+i+1) );
      }
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < nr; i += 3) {
	 tri( swrender, 
	      get_vertex(swrender, start+i),
	      get_vertex(swrender, start+i+1),
	      get_vertex(swrender, start+i+2) );
      }
      break;

   case GL_TRIANGLE_STRIP:
      for (i = 0; i+2 < nr; i++) {
	 if (i & 1) 
	    tri( swrender, 
		 get_vertex(swrender, start+i+1),
		 get_vertex(swrender, start+i+0),
		 get_vertex(swrender, start+i+2) );
	 else
	    tri( swrender, 
		 get_vertex(swrender, start+i+0),
		 get_vertex(swrender, start+i+1),
		 get_vertex(swrender, start+i+2) );
      }
      break;

   case GL_TRIANGLE_FAN:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      get_vertex(swrender, start+0),
	      get_vertex(swrender, start+i+1),
	      get_vertex(swrender, start+i+2) );
      }
      break;

   case GL_POLYGON:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      get_vertex(swrender, start+i+1),
	      get_vertex(swrender, start+i+2),
	      get_vertex(swrender, start+0) );
      }
      break;

   default:
      assert(0);
      break;
   }

   intel_do_SpanRenderFinish( intel );
}

static void swrender_draw_indexed_prim( struct clip_render *render,
					const GLuint *indices,
					GLuint nr )
{
   struct swrast_render *swrender = swrast_render( render );
   struct intel_context *intel = swrender->intel;
   GLcontext *ctx = &intel->ctx;
   GLuint i;

   intel_frame_set_mode( intel->ft, INTEL_FT_SWRAST );

   intel_do_SpanRenderStart( intel );

   switch (swrender->prim) {
   case GL_POINTS:
      for (i = 0; i < nr; i++) {
	 point( swrender, 
		get_vertex(swrender, indices[i]) );
      }
      break;

   case GL_LINES:
      for (i = 0; i+1 < nr; i += 2) {
	 line( swrender, 
	       get_vertex(swrender, indices[i]),
	       get_vertex(swrender, indices[i+1]) );
 	 _swrast_ResetLineStipple( ctx );
      }
      break;

   case GL_LINE_STRIP:
      for (i = 0; i+1 < nr; i++) {
	 line( swrender, 
	       get_vertex(swrender, indices[i]),
	       get_vertex(swrender, indices[i+1]) );
      }
      _swrast_ResetLineStipple( ctx ); 
      break;

   case GL_TRIANGLES:
      for (i = 0; i+2 < nr; i += 3) {
	 tri( swrender, 
	      get_vertex(swrender, indices[i]),
	      get_vertex(swrender, indices[i+1]),
	      get_vertex(swrender, indices[i+2]) );
      }
      break;

   case GL_TRIANGLE_STRIP:
      for (i = 0; i+2 < nr; i++) {
	 if (i & 1) 
	    tri( swrender, 
		 get_vertex(swrender, indices[i+1]),
		 get_vertex(swrender, indices[i+0]),
		 get_vertex(swrender, indices[i+2]) );
	 else
	    tri( swrender, 
		 get_vertex(swrender, indices[i+0]),
		 get_vertex(swrender, indices[i+1]),
		 get_vertex(swrender, indices[i+2]) );
      }
      break;

   case GL_TRIANGLE_FAN:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      get_vertex(swrender, indices[0]),
	      get_vertex(swrender, indices[i+1]),
	      get_vertex(swrender, indices[i+2]) );
      }
      break;

   case GL_POLYGON:
      for (i = 0; i+2 < nr; i++) {
	 tri( swrender, 
	      get_vertex(swrender, indices[i+1]),
	      get_vertex(swrender, indices[i+2]),
	      get_vertex(swrender, indices[0]) );
      }
      break;

   default:
      assert(0);
      break;
   }

   intel_do_SpanRenderFinish( intel );
}



static void swrender_set_prim( struct clip_render *render,
			     GLuint prim )
{
   struct swrast_render *swrender = swrast_render( render );
   swrender->prim = prim;
}


static void swrender_start_render( struct clip_render *render,
				   GLboolean start_of_frame)
{
   struct swrast_render *swrender = swrast_render( render );
   struct intel_context *intel = swrender->intel;

   //_mesa_printf("%s\n", __FUNCTION__);

   /* Wait for pending flip.
    */
   if (start_of_frame)
      intel_do_wait_flips(intel);

   /* Wait for last fence to clear:
    */
   intel_batchbuffer_wait_last_fence(intel->batch);
}

static void swrender_clear_rect( struct clip_render *render,
				 GLuint mask,
				 GLuint x1, GLuint y1, 
				 GLuint x2, GLuint y2 )
{
   struct intel_context *intel = swrast_render(render)->intel;

   intel->classic->clear_rect( intel->classic, mask, x1, y1, x2, y2 );
}



static void swrender_flush( struct clip_render *render,
			    GLboolean finished_frame )
{
   /* all done in SpanRenderFinish */
}


static void swrender_destroy_context( struct clip_render *render )
{
   struct swrast_render *swrender = swrast_render( render );

   _mesa_free(swrender);
}

struct clip_render *intel_create_swrast_render( struct intel_context *intel )
{
   struct swrast_render *swrender = CALLOC_STRUCT(swrast_render);

   swrender->render.limits.max_indices = ~0;

   swrender->render.start_render = swrender_start_render;
   swrender->render.allocate_vertices = swrender_allocate_vertices;
   swrender->render.set_prim = swrender_set_prim;
   swrender->render.draw_prim = swrender_draw_prim;
   swrender->render.draw_indexed_prim = swrender_draw_indexed_prim;
   swrender->render.flush = swrender_flush;
   swrender->render.release_vertices = swrender_release_vertices;
   swrender->render.destroy = swrender_destroy_context;
   swrender->render.clear_rect = swrender_clear_rect;

   swrender->intel = intel;
   swrender->prim = 0;

   return &swrender->render;
}
