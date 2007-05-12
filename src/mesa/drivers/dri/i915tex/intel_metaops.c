/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 develop this 3D driver.
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  *   frame buffer texture by Gary Wong <gtw@gnu.org>
  */

/* TODO: push this type of operation into core mesa.
 */


#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "enums.h"
#include "teximage.h"
#include "dd.h"

#include "shader/arbprogparse.h"

#include "intel_screen.h"
#include "intel_batchbuffer.h"
#include "intel_regions.h"
#include "intel_context.h"
#include "intel_mipmap_tree.h"
#include "intel_metaops.h"
#include "intel_reg.h"
#include "intel_state.h"
#include "intel_vb.h"
#include "intel_tex.h"


#define STATECHANGE( intelctx, flags ) 			\
do {							\
   (intelctx)->state.dirty.intel |= INTEL_NEW_MESA;	\
   (intelctx)->state.dirty.mesa |= (flags);		\
} while (0)

#define DUP(i915, STRUCT, ATTRIB) 		\
do {						\
   intel->metaops.state.ATTRIB = MALLOC_STRUCT(STRUCT);	\
   memcpy(intel->metaops.state.ATTRIB, 			\
	  intel->state.ATTRIB,			\
	  sizeof(struct STRUCT));		\
} while (0)


#define INSTALL(intel, ATTRIB, STATE)		\
do {						\
   intel->state.ATTRIB = intel->metaops.state.ATTRIB;	\
   STATECHANGE(intel, STATE);		\
} while (0)

#define RESTORE(intel, ATTRIB, STATE)			\
do {							\
   intel->state.ATTRIB = &intel->ctx.ATTRIB;	\
   STATECHANGE(intel, STATE);			\
} while (0)

static void init_state( struct intel_context *intel )
{
   DUP(intel, gl_colorbuffer_attrib, Color);
   DUP(intel, gl_depthbuffer_attrib, Depth);
   DUP(intel, gl_fog_attrib, Fog);
   DUP(intel, gl_hint_attrib, Hint);
   DUP(intel, gl_light_attrib, Light);
   DUP(intel, gl_line_attrib, Line);
   DUP(intel, gl_point_attrib, Point);
   DUP(intel, gl_polygon_attrib, Polygon);
   DUP(intel, gl_scissor_attrib, Scissor);
   DUP(intel, gl_stencil_attrib, Stencil);
   DUP(intel, gl_texture_attrib, Texture);
   DUP(intel, gl_transform_attrib, Transform);
   DUP(intel, gl_viewport_attrib, Viewport);
   DUP(intel, gl_vertex_program_state, VertexProgram);
   DUP(intel, gl_fragment_program_state, FragmentProgram);
}

static void install_state( struct intel_context *intel )
{
   INSTALL(intel, Color, _NEW_COLOR);
   INSTALL(intel, Depth, _NEW_DEPTH);
   INSTALL(intel, Fog, _NEW_FOG);
   INSTALL(intel, Hint, _NEW_HINT);
   INSTALL(intel, Light, _NEW_LIGHT);
   INSTALL(intel, Line, _NEW_LINE);
   INSTALL(intel, Point, _NEW_POINT);
   INSTALL(intel, Polygon, _NEW_POLYGON);
   INSTALL(intel, Scissor, _NEW_SCISSOR);
   INSTALL(intel, Stencil, _NEW_STENCIL);
   INSTALL(intel, Texture, _NEW_TEXTURE);
   INSTALL(intel, Transform, _NEW_TRANSFORM);
   INSTALL(intel, Viewport, _NEW_VIEWPORT);
   INSTALL(intel, VertexProgram, _NEW_PROGRAM);
   INSTALL(intel, FragmentProgram, _NEW_PROGRAM);
}

static void restore_state( struct intel_context *intel )
{
   RESTORE(intel, Color, _NEW_COLOR);
   RESTORE(intel, Depth, _NEW_DEPTH);
   RESTORE(intel, Fog, _NEW_FOG);
   RESTORE(intel, Hint, _NEW_HINT);
   RESTORE(intel, Light, _NEW_LIGHT);
   RESTORE(intel, Line, _NEW_LINE);
   RESTORE(intel, Point, _NEW_POINT);
   RESTORE(intel, Polygon, _NEW_POLYGON);
   RESTORE(intel, Scissor, _NEW_SCISSOR);
   RESTORE(intel, Stencil, _NEW_STENCIL);
   RESTORE(intel, Texture, _NEW_TEXTURE);
   RESTORE(intel, Transform, _NEW_TRANSFORM);
   RESTORE(intel, Viewport, _NEW_VIEWPORT);
   RESTORE(intel, VertexProgram, _NEW_PROGRAM);
   RESTORE(intel, FragmentProgram, _NEW_PROGRAM);
}



/* Using fragment programs like this is pretty i915 specific...
 */
static const char *fp_prog =
      "!!ARBfp1.0\n"
      "MOV result.color, fragment.color;\n"
      "END\n";

static const char *fp_tex_prog =
      "!!ARBfp1.0\n"
      "TEX result.color, fragment.texcoord[0], texture[0], RECT;\n"
      "END\n";


void intel_meta_flat_shade( struct intel_context *intel )
{
   intel->metaops.state.Light->ShadeModel = GL_FLAT;
   STATECHANGE(intel, _NEW_LIGHT);
}


void intel_meta_no_stencil_write( struct intel_context *intel )
{
   intel->metaops.state.Stencil->Enabled = GL_FALSE;
   STATECHANGE(intel, _NEW_STENCIL);
}

void intel_meta_no_depth_write( struct intel_context *intel )
{
   intel->metaops.state.Depth->Test = GL_FALSE;
   intel->metaops.state.Depth->Mask = GL_FALSE;
   STATECHANGE(intel, _NEW_DEPTH);
}


void intel_meta_depth_replace( struct intel_context *intel )
{
   /* ctx->Driver.Enable( ctx, GL_DEPTH_TEST, GL_TRUE )
    * ctx->Driver.DepthMask( ctx, GL_TRUE )
    */
   intel->metaops.state.Depth->Test = GL_TRUE;
   intel->metaops.state.Depth->Mask = GL_TRUE;
   STATECHANGE(intel, _NEW_DEPTH);

   /* ctx->Driver.DepthFunc( ctx, GL_ALWAYS )
    */
   intel->metaops.state.Depth->Func = GL_ALWAYS;

   STATECHANGE(intel, _NEW_DEPTH);
}


void intel_meta_stencil_replace( struct intel_context *intel,
				 GLuint s_mask,
				 GLuint s_clear)
{
   intel->metaops.state.Stencil->Enabled = GL_TRUE;
   intel->metaops.state.Stencil->WriteMask[0] = s_mask;
   intel->metaops.state.Stencil->ValueMask[0] = 0xff;
   intel->metaops.state.Stencil->Ref[0] = s_clear;
   intel->metaops.state.Stencil->Function[0] = GL_ALWAYS;
   intel->metaops.state.Stencil->FailFunc[0] = GL_REPLACE;
   intel->metaops.state.Stencil->ZPassFunc[0] = GL_REPLACE;
   intel->metaops.state.Stencil->ZFailFunc[0] = GL_REPLACE;
   STATECHANGE(intel, _NEW_STENCIL);
}


void intel_meta_color_mask( struct intel_context *intel, GLboolean state )
{
   if (state)
      COPY_4V(intel->metaops.state.Color->ColorMask, 
	      intel->ctx.Color.ColorMask); 
   else
      ASSIGN_4V(intel->metaops.state.Color->ColorMask, 0, 0, 0, 0);

   STATECHANGE(intel, _NEW_COLOR);
}

void intel_meta_no_texture( struct intel_context *intel )
{
   intel->metaops.state.FragmentProgram->_Current = intel->metaops.fp;

   intel->metaops.state.Texture->CurrentUnit = 0;
   intel->metaops.state.Texture->_EnabledUnits = 0;
   intel->metaops.state.Texture->_EnabledCoordUnits = 0;
   intel->metaops.state.Texture->Unit[ 0 ].Enabled = 0;
   intel->metaops.state.Texture->Unit[ 0 ]._ReallyEnabled = 0;

   STATECHANGE(intel, _NEW_TEXTURE | _NEW_PROGRAM);
}

void intel_meta_texture_blend_replace(struct intel_context *intel)
{
   intel->metaops.state.FragmentProgram->_Current = intel->metaops.fp_tex;

   intel->metaops.state.Texture->CurrentUnit = 0;
   intel->metaops.state.Texture->_EnabledUnits = 1;
   intel->metaops.state.Texture->_EnabledCoordUnits = 1;
   intel->metaops.state.Texture->Unit[ 0 ].Enabled = TEXTURE_RECT_BIT;
   intel->metaops.state.Texture->Unit[ 0 ]._ReallyEnabled = TEXTURE_RECT_BIT;
/*    intel->metaops.state.Texture->Unit[ 0 ].CurrentRect = */
/*       intel->metaops.texobj; */
/*    intel->metaops.state.Texture->Unit[ 0 ]._Current =  */
/*       intel->metaops.texobj;  */

   STATECHANGE(intel, _NEW_TEXTURE | _NEW_PROGRAM);
}

void intel_meta_import_pixel_state(struct intel_context *intel)
{
   RESTORE(intel, Color, _NEW_COLOR);
   RESTORE(intel, Depth, _NEW_DEPTH);
   RESTORE(intel, Fog, _NEW_FOG);
   RESTORE(intel, Scissor, _NEW_SCISSOR);
   RESTORE(intel, Stencil, _NEW_STENCIL);
   RESTORE(intel, Texture, _NEW_TEXTURE);
   RESTORE(intel, FragmentProgram, _NEW_PROGRAM);
}



/* Set up an arbitary piece of memory as a rectangular texture
 * (including the front or back buffer).
 */
GLboolean intel_meta_tex_rect_source(struct intel_context *intel,
				     struct intel_region *region,
				     GLuint offset,
				     GLenum format, GLenum type)
{
   GLcontext *ctx = &intel->ctx;
   struct gl_texture_image *texImage;
   struct gl_texture_object *texObj;

   assert(offset == 0);		/* for now */

   if ((region->pitch * region->cpp) & 3) {
      _mesa_printf("%s: texture is not dword pitch\n", __FUNCTION__);
      return GL_FALSE;
   }


   texObj = ctx->Driver.NewTextureObject( ctx, (GLuint) -1, GL_TEXTURE_RECTANGLE_NV );
   texImage = ctx->Driver.NewTextureImage( ctx );

   texObj->MinFilter = GL_NEAREST;
   texObj->MagFilter = GL_NEAREST;

   _mesa_init_teximage_fields( ctx, GL_TEXTURE_RECTANGLE_NV, 
			       texImage,
			       region->pitch, region->height, 
			       1, 0,
			       format );
   
   _mesa_set_tex_image( texObj, GL_TEXTURE_RECTANGLE_NV, 0, texImage );


   {
      struct intel_texture_image *intelImage = intel_texture_image( texImage );
      struct intel_texture_object *intelObj = intel_texture_object( texObj );
      
      struct intel_mipmap_tree *mt = 
	 intel_miptree_from_region( region,
				    GL_TEXTURE_RECTANGLE_NV,
				    format );

      intelObj->mt = mt;
      intelImage->mt = mt;
   }
   
   intel->metaops.texobj = texObj;
   intel->metaops.state.Texture->Unit[0].CurrentRect = texObj;
   intel->metaops.state.Texture->Unit[0]._Current = texObj;
   return GL_TRUE;
}



void intel_meta_draw_region( struct intel_context *intel,
			     struct intel_region *draw_region,
			     struct intel_region *depth_region )
{
   if (!intel->metaops.saved_draw_region) {
      intel->metaops.saved_draw_region = intel->state.draw_region;
      intel->metaops.saved_depth_region = intel->state.depth_region;
   }

   intel->state.draw_region = draw_region;
   intel->state.depth_region = depth_region;

   STATECHANGE(intel, _NEW_BUFFERS);
}


void
intel_meta_draw_color_quad(struct intel_context *intel,
		      struct intel_metaops_color_vertex *vertex )
{
   intel_update_software_state( intel );

   assert(sizeof(*vertex) == intel->vb->vertex_size_bytes);

   {
      struct intel_render *render = intel->render;
      void *vb = render->allocate_vertices( render,
					    intel->vb->vertex_size_bytes,
					    4 );
      
      memcpy(vb, vertex, 4 * sizeof(*vertex));

      render->set_prim( render, GL_TRIANGLE_FAN );
      render->draw_prim( render, 0, 4 );
      render->release_vertices( render, vb );
   }
}

void
intel_meta_draw_textured_quad(struct intel_context *intel,
			      struct intel_metaops_tex_vertex *vertex )
{
   intel_update_software_state( intel );

   assert(sizeof(*vertex) == intel->vb->vertex_size_bytes);

   {
      struct intel_render *render = intel->render;
      void *vb = render->allocate_vertices( render, 
					    intel->vb->vertex_size_bytes,
					    4 );
      
      memcpy(vb, vertex, 4 * sizeof(*vertex));
      
      render->set_prim( render, GL_TRIANGLE_FAN );
      render->draw_prim( render, 0, 4 );
      render->release_vertices( render, vb );
   }
}




void intel_meta_draw_quad(struct intel_context *intel,
			  GLfloat x0, GLfloat x1,
			  GLfloat y0, GLfloat y1,
			  GLfloat z,
			  GLuint color,
			  GLfloat s0, GLfloat s1, GLfloat t0, GLfloat t1)
{
   struct intel_metaops_tex_vertex vertex[4];

   vertex[0].xyz[0] = x0;
   vertex[0].xyz[1] = y0;
   vertex[0].xyz[2] = z;
   vertex[0].st[0]  = s0;
   vertex[0].st[1]  = t0;

   vertex[1].xyz[0] = x1;
   vertex[1].xyz[1] = y0;
   vertex[1].xyz[2] = z;
   vertex[1].st[0]  = s1;
   vertex[1].st[1]  = t0;

   vertex[2].xyz[0] = x1;
   vertex[2].xyz[1] = y1;
   vertex[2].xyz[2] = z;
   vertex[2].st[0]  = s1;
   vertex[2].st[1]  = t1;

   vertex[3].xyz[0] = x0;
   vertex[3].xyz[1] = y1;
   vertex[3].xyz[2] = z;
   vertex[3].st[0]  = s0;
   vertex[3].st[1]  = t1;

   intel_meta_draw_textured_quad( intel, vertex );
}

void intel_install_meta_state( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   assert(ctx->NewState == 0);

   install_state(intel);
   
   intel_meta_no_texture(intel);
//   intel_meta_flat_shade(intel);

   intel->metaops.restore_draw_mask = ctx->DrawBuffer->_ColorDrawBufferMask[0]; 
   intel->metaops.restore_fp = ctx->FragmentProgram.Current; 

   /* This works without adjusting refcounts.  Fix later? 
    */
   intel->metaops.saved_draw_region = intel->state.draw_region; 
   intel->metaops.saved_depth_region = intel->state.depth_region; 
   intel->metaops.active = 1;
   
   intel->state.dirty.intel |= INTEL_NEW_METAOPS;
}

void intel_leave_meta_state( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;
   restore_state(intel);

   ctx->DrawBuffer->_ColorDrawBufferMask[0] = intel->metaops.restore_draw_mask; 
   ctx->FragmentProgram.Current = intel->metaops.restore_fp; 

   intel->state.draw_region = intel->metaops.saved_draw_region; 
   intel->state.depth_region = intel->metaops.saved_depth_region; 
   intel->metaops.saved_draw_region = NULL;
   intel->metaops.saved_depth_region = NULL; 
   intel->metaops.active = 0;

   if (intel->metaops.texobj) {
      ctx->Driver.DeleteTexture( ctx, intel->metaops.texobj );
      intel->metaops.state.Texture->Unit[ 0 ].CurrentRect = NULL;
      intel->metaops.state.Texture->Unit[ 0 ]._Current = NULL;
      intel->metaops.texobj = 0;
   }

   STATECHANGE(intel, _NEW_BUFFERS);
   intel->state.dirty.intel |= INTEL_NEW_METAOPS;
}



void intel_metaops_init( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   init_state(intel);

   intel->metaops.fp = (struct gl_fragment_program *)
      ctx->Driver.NewProgram(ctx, GL_FRAGMENT_PROGRAM_ARB, 1 );

   intel->metaops.fp_tex = (struct gl_fragment_program *)
      ctx->Driver.NewProgram(ctx, GL_FRAGMENT_PROGRAM_ARB, 1 );

   _mesa_parse_arb_fragment_program(ctx, GL_FRAGMENT_PROGRAM_ARB, 
				    fp_prog, strlen(fp_prog),
				    intel->metaops.fp);

   _mesa_parse_arb_fragment_program(ctx, GL_FRAGMENT_PROGRAM_ARB, 
				    fp_tex_prog, strlen(fp_tex_prog),
				    intel->metaops.fp_tex);

   intel->metaops.state.FragmentProgram->_Current = intel->metaops.fp;
}

void intel_metaops_destroy( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   ctx->Driver.DeleteProgram( ctx, &intel->metaops.fp->Base );
   ctx->Driver.DeleteProgram( ctx, &intel->metaops.fp_tex->Base );
}
