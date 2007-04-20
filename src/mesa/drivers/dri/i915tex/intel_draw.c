/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

/* TNL pipeline stage which submits drawing commands to the render
 * backends.  Will (eventually) take care of all the
 * unfilled/twoside/clipping/etc business so that the backends never
 * see that stuff.
 *
 * This is a replacement for intel_tris.[ch], intel_render.c and
 * intel_idx_render.c.
 */
#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "imports.h"
#include "mtypes.h"
#include "enums.h"
#include "texobj.h"
#include "state.h"

#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"

#include "intel_context.h"
#include "intel_batchbuffer.h"
#include "intel_reg.h"
#include "intel_state.h"
#include "intel_vb.h"
#include "intel_draw.h"

static GLuint hw_prim[GL_POLYGON+1] = {
   PRIM3D_POINTLIST,
   PRIM3D_LINELIST,
   PRIM3D_LINESTRIP,
   PRIM3D_LINESTRIP,
   PRIM3D_TRILIST,
   PRIM3D_TRISTRIP,
   PRIM3D_TRIFAN,
   0,
   0,
   PRIM3D_POLY
};

static const GLenum reduced_prim[GL_POLYGON+1] = {  
   GL_POINTS,
   GL_LINES,
   GL_LINES,
   GL_LINES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES
};


/* All the per-primitive fallback stuff now ends up in here:
 *
 *    - polygon stipple
 *    - line stipple
 *    - point attenuation ???
 *    -  
 */
static void intel_check_reduced_prim_state( struct intel_context *intel,
					    GLenum reduced_prim ) 
{
#if 0
   /* XXX: shortcircuit this statechange to only those cases where we
    * know that the driver cares.  This is a bit naughty, but the
    * gains are probably worth the single special case.
    */
   if (intel->state.Polygon->StippleFlag ||
       intel->state.Line->StippleFlag ||
       intel->state.Point->Atten) {

      intel->state.dirty.intel |= INTEL_NEW_REDUCED_PRIMITIVE;
   }
#else
   intel->state.dirty.intel |= INTEL_NEW_REDUCED_PRIMITIVE;
#endif

   /* Where does the state get validated/emitted?
    */
   intel_update_software_state( intel );
}

static void intel_check_prim_state( struct intel_context *intel,
				    GLenum prim,
				    GLuint hw_prim )
{
   GLenum rprim = reduced_prim[prim];

   if (rprim != intel->draw.reduced_primitive) {
      intel->draw.reduced_primitive = rprim;
      intel_check_reduced_prim_state( intel, rprim );
   }

   if (hw_prim != intel->draw.hw_prim) {
      intel->draw.hw_prim = hw_prim;
      intel->render->set_prim( intel->render, hw_prim );
   }
}

static void intel_draw_indexed_prim( struct intel_context *intel,
				     const struct _mesa_prim *prim,
				     const GLuint *indices )
{
   GLenum mode = prim->mode;
   GLuint start = prim->start;
   GLuint length = prim->count;

   if (!length)
      return;

   switch (mode) {
   case GL_POINTS:
   case GL_LINES:
   case GL_LINE_STRIP:
   case GL_TRIANGLES:
   case GL_TRIANGLE_STRIP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      intel_check_prim_state( intel, mode, hw_prim[mode] );

      intel->render->draw_indexed_prim( intel->render, 
					indices + start, 
					length );
      break;



   case GL_LINE_LOOP: {
      GLuint tmp_indices[2] = { indices[start + length],
				indices[start] };

      intel_check_prim_state( intel, GL_LINE_LOOP, PRIM3D_LINESTRIP );

      if (!prim->begin) {
	 /* Maybe need to adjust the start and length if this is not a
	  * BEGIN primitive, to avoid spokes from the loop start:
	  */
	 start++;
	 length--;
      }

      intel->render->draw_indexed_prim( intel->render, 
					indices + start, 
					length );

      /* Probably wont work as stipple would get reset.  Need to
       * build a single indexed linestrip to cover the whole
       * primitive:
       */
      if (prim->end) 
	 intel->render->draw_indexed_prim( intel->render, 
					   tmp_indices, 
					   2 );
      break;
   }


   case GL_QUAD_STRIP:
      if (intel->state.Light->ShadeModel != GL_FLAT) {
	 intel_check_prim_state( intel, mode, PRIM3D_TRISTRIP );
	 intel->render->draw_prim( intel->render, start, length );
      }
      else {
	 GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 2 * 6) );
	 GLuint i, j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = indices[i+0];
	    tmp[j+1] = indices[i+1]; /* this is wrong! */
	    tmp[j+2] = indices[i+3];

	    tmp[j+3] = indices[i+1];
	    tmp[j+4] = indices[i+2]; /* this is wrong! */
	    tmp[j+5] = indices[i+3];
	 }

	 intel_check_prim_state( intel, mode, PRIM3D_TRILIST );
	 intel->render->draw_indexed_prim( intel->render, tmp, j );
	 _mesa_free(tmp);
      }
      break;

   case GL_QUADS: {
      GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 4 * 6) );
      GLuint i, j;

      for (j = i = 0; i + 3 < length; i += 4, j += 6) {
	 tmp[j+0] = indices[i+0];
	 tmp[j+1] = indices[i+1];
	 tmp[j+2] = indices[i+3];

	 tmp[j+3] = indices[i+1];
	 tmp[j+4] = indices[i+2];
	 tmp[j+5] = indices[i+3];
      }

      intel_check_prim_state( intel, mode, PRIM3D_TRILIST );
      intel->render->draw_indexed_prim( intel->render, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      assert(0);
      break;
   }
}



static void intel_draw_prim( struct intel_context *intel,
			     const struct _mesa_prim *prim )
{
   GLenum mode = prim->mode;
   GLuint start = prim->start;
   GLuint length = prim->count;

   if (!length)
      return;

   switch (mode) {
   case GL_POINTS:
   case GL_LINES:
   case GL_LINE_STRIP:
   case GL_TRIANGLES:
   case GL_TRIANGLE_STRIP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      intel_check_prim_state( intel, mode, hw_prim[mode] );

      intel->render->draw_prim( intel->render, start, length );
      break;



   case GL_LINE_LOOP: {
      GLuint indices[2] = { start + length - 1, start };

      intel_check_prim_state( intel, mode, PRIM3D_LINESTRIP );

      if (!prim->begin) {
	 /* Maybe need to adjust the start and length if this is not a
	  * BEGIN primitive, to avoid spokes from the loop start:
	  */
	 start++;
	 length--;
      }

      intel->render->draw_prim( intel->render, start, length );

      /* Probably wont work as stipple would get reset.  Need to
       * build a single indexed linestrip to cover the whole
       * primitive:
       */
      if (prim->end) 
	 intel->render->draw_indexed_prim( intel->render, indices, 2 );
      break;
   }


   case GL_QUAD_STRIP:
      if (intel->state.Light->ShadeModel != GL_FLAT) {
	 intel_check_prim_state( intel, mode, PRIM3D_TRISTRIP );
	 intel->render->draw_prim( intel->render, start, length );
      }
      else {
	 GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 2 * 6) );
	 GLuint i,j;

	 for (j = i = 0; i + 3 < length; i += 2, j += 6) {
	    tmp[j+0] = start+i+0;
	    tmp[j+1] = start+i+1; /* this is wrong! */
	    tmp[j+2] = start+i+3;

	    tmp[j+3] = start+i+1;
	    tmp[j+4] = start+i+2; /* this is wrong! */
	    tmp[j+5] = start+i+3;
	 }

	 intel_check_prim_state( intel, mode, PRIM3D_TRILIST );
	 intel->render->draw_indexed_prim( intel->render, tmp, j );
	 _mesa_free(tmp);
      }
      break;

   case GL_QUADS: {
      GLuint *tmp = _mesa_malloc( sizeof(int) * (length / 4 * 6) );
      GLuint i,j;

      for (j = i = 0; i + 3 < length; i += 4, j += 6) {
	 tmp[j+0] = start+i+0;
	 tmp[j+1] = start+i+1;
	 tmp[j+2] = start+i+3;

	 tmp[j+3] = start+i+1;
	 tmp[j+4] = start+i+2;
	 tmp[j+5] = start+i+3;
      }

      intel_check_prim_state( intel, mode, PRIM3D_TRILIST );
      intel->render->draw_indexed_prim( intel->render, tmp, j );
      _mesa_free(tmp);
      break;
   }

   default:
      assert(0);
      break;
   }
}





static GLboolean
intel_run_render(GLcontext *ctx, struct tnl_pipeline_stage *stage)
{
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint i;


#if 0
   /* Build window space vertex buffer in local memory.  Everything
    * required from here down is in the vertex buffer.
    */

   /* Clip window-space vertex buffer if necessary.  This involves
    * nasty unprojection operations.  Fix later.
    */

   /* Do twosided stuff, using secondary color stored in vertex
    * buffer, create a new vertex buffer as the result.  
    */

   /* Do unfilled + offset stuff, creating new vertexbuffer +
    * primitive list as a result.
    *
    * OR: Do unfilled stuff, just creating new primitive lists as a
    * result.
    */

   /* Finally, copy vertex buffer to graphics buffer object.  Use
    * pointers to the original version for swz, but index information
    * refers to the copied buffer.
    *
    * Can optimize:
    *     - classic render or hwz, no clipping/twiddling: emit direct to hw
    *     - swz, no clipping:  viewport transform locally, emit direct to hw.
    */

   /* Note: change of primitive is a statechange and may result in the
    * render pointer changing (eg to swrast/fallback).
    */

   /* ____OR____ */
   
   /* Do the above, but in a deep, per-triangle fashion, rather than a
    * wide, per-vertex-buffer approach.  Would probably still want to
    * have the option of creating a vertex buffer at the end.  The
    * below would be a fastpath in that case.
    */

#else
   /* These will need some work...
    */
   assert(VB->ClipOrMask == 0);
#endif

   intel_update_software_state(intel);
   intel->draw.in_draw = 1;
   if (!intel->draw.in_frame) {
      intel->render->start_render( intel->render );
      intel->draw.in_frame = 1;
   }

   /* Currently this is hardwired to know about the tnl VB, etc.  Just
    * need to prod it to note there is new data available.  The
    * renderer will then pull either local or vbo vertices from it, or
    * perhaps both.
    */
   intel_vb_new_vertices( intel->vb );
   intel->render->new_vertices( intel->render );

   if (VB->Elts) {
      for (i = 0; i < VB->PrimitiveCount; i++) {
	 intel_draw_indexed_prim( intel, 
				  &VB->Primitive[i],
				  VB->Elts );
      }
   }
   else {
      for (i = 0; i < VB->PrimitiveCount; i++) {
	 intel_draw_prim( intel, &VB->Primitive[i] );
      }
	 
   }

   intel_vb_release_vertices( intel->vb );
   intel->draw.in_draw = 0;

   return GL_FALSE;             /* finished the pipe */
}

const struct tnl_pipeline_stage _intel_render_stage = {
   "intel render",
   NULL,
   NULL,
   NULL,
   NULL,
   intel_run_render             /* run */
};



void intelRunPipeline(GLcontext * ctx)
{
   struct intel_context *intel = intel_context(ctx);

   _mesa_lock_context_textures(ctx);
   
   if (ctx->NewState)
      _mesa_update_state_locked(ctx);

   intel_update_software_state( intel );

   _tnl_run_pipeline(ctx);

   _mesa_unlock_context_textures(ctx);
}

