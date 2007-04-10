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
#include "dd.h"

#include "shader/arbprogparse.h"

#include "intel_screen.h"
#include "intel_batchbuffer.h"
#include "intel_regions.h"
#include "intel_context.h"
#include "intel_metaops.h"
#include "intel_reg.h"
#include "intel_state.h"


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
   intel->state.dirty.mesa |= STATE;		\
} while (0)

#define RESTORE(intel, ATTRIB, STATE)			\
do {							\
   intel->state.ATTRIB = &intel->ctx.ATTRIB;	\
   intel->state.dirty.mesa |= STATE;			\
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

/* It would certainly be preferably to skip the vertex program step
 * and go straight to post-transform draw.  We can probably do that
 * with the indexed-render code and after noting that fallbacks are
 * never active during metaops.
 */
static const char *vp_prog_tex =
      "!!ARBvp1.0\n"
      "MOV  result.texcoord[0], vertex.texcoord[0];\n"
      "MOV  result.color, vertex.color;\n"
      "MOV  result.position, vertex.position;\n"
      "END\n";


/* Because the texenv program is calculated from ctx derived state,
 * and bypasses our intel->state attribute pointer mechanism, it won't
 * be correctly calculated in metaops.  So, we have to supply our own
 * fragment programs to do the right thing:
 */
static const char *fp_prog =
      "!!ARBfp1.0\n"
      "MOV result.color, fragment.color;\n"
      "END\n";

static const char *fp_tex_prog =
      "!!ARBfp1.0\n"
      "TEX result.color, fragment.texcoord[0], texture[0], 2D;\n"
      "END\n";


void intel_meta_flat_shade( struct intel_context *intel )
{
   intel->metaops.state.Light->ShadeModel = GL_FLAT;
   intel->state.dirty.mesa |= _NEW_LIGHT;
}


void intel_meta_no_stencil_write( struct intel_context *intel )
{
   intel->metaops.state.Stencil->Enabled = GL_FALSE;
   intel->metaops.state.Stencil->WriteMask[0] = GL_FALSE; 
   intel->state.dirty.mesa |= _NEW_STENCIL;
}

void intel_meta_no_depth_write( struct intel_context *intel )
{
   intel->metaops.state.Depth->Test = GL_FALSE;
   intel->metaops.state.Depth->Mask = GL_FALSE;
   intel->state.dirty.mesa |= _NEW_DEPTH;
}


void intel_meta_depth_replace( struct intel_context *intel )
{
   /* ctx->Driver.Enable( ctx, GL_DEPTH_TEST, GL_TRUE )
    * ctx->Driver.DepthMask( ctx, GL_TRUE )
    */
   intel->metaops.state.Depth->Test = GL_TRUE;
   intel->metaops.state.Depth->Mask = GL_TRUE;
   intel->state.dirty.mesa |= _NEW_DEPTH;

   /* ctx->Driver.DepthFunc( ctx, GL_ALWAYS )
    */
   intel->metaops.state.Depth->Func = GL_ALWAYS;

   intel->state.dirty.mesa |= _NEW_DEPTH;
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
   intel->state.dirty.mesa |= _NEW_STENCIL;
}


void intel_meta_color_mask( struct intel_context *intel, GLboolean state )
{
   if (state)
      COPY_4V(intel->metaops.state.Color->ColorMask, 
	      intel->ctx.Color.ColorMask); 
   else
      ASSIGN_4V(intel->metaops.state.Color->ColorMask, 0, 0, 0, 0);

   intel->state.dirty.mesa |= _NEW_COLOR;
}

void intel_meta_no_texture( struct intel_context *intel )
{
   intel->metaops.state.Texture->CurrentUnit = 0;
   intel->metaops.state.Texture->_EnabledUnits = 0;
   intel->metaops.state.Texture->_EnabledCoordUnits = 0;
   intel->metaops.state.Texture->Unit[ 0 ].Enabled = 0;
   intel->metaops.state.Texture->Unit[ 0 ]._ReallyEnabled = 0;

   intel->state.dirty.mesa |= _NEW_TEXTURE | _NEW_PROGRAM;
}

void intel_meta_texture_blend_replace(struct intel_context *intel)
{
   intel->metaops.state.Texture->CurrentUnit = 0;
   intel->metaops.state.Texture->_EnabledUnits = 1;
   intel->metaops.state.Texture->_EnabledCoordUnits = 1;
   intel->metaops.state.Texture->Unit[ 0 ].Enabled = TEXTURE_2D_BIT;
   intel->metaops.state.Texture->Unit[ 0 ]._ReallyEnabled = TEXTURE_2D_BIT;
/*    intel->metaops.state.Texture->Unit[ 0 ].Current2D = */
/*       intel->frame_buffer_texobj; */
/*    intel->metaops.state.Texture->Unit[ 0 ]._Current = */
/*       intel->frame_buffer_texobj; */

   intel->state.dirty.mesa |= _NEW_TEXTURE | _NEW_PROGRAM;
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
				     struct _DriBufferObject *buffer,
				     GLuint offset,
				     GLuint pitch, GLuint height, 
				     GLenum format, GLenum type)
{
   assert(0);
#if 0
   GLuint unit = 0;
   GLint numLevels = 1;
   GLuint *state = intel->meta.Tex[0];
   GLuint textureFormat;
   GLuint cpp;

   /* A full implementation of this would do the upload through
    * glTexImage2d, and get all the conversion operations at that
    * point.  We are restricted, but still at least have access to the
    * fragment program swizzle.
    */
   switch (format) {
   case GL_BGRA:
      switch (type) {
      case GL_UNSIGNED_INT_8_8_8_8_REV:
      case GL_UNSIGNED_BYTE:
         textureFormat = (MAPSURF_32BIT | MT_32BIT_ARGB8888);
         cpp = 4;
         break;
      default:
         return GL_FALSE;
      }
      break;
   case GL_RGBA:
      switch (type) {
      case GL_UNSIGNED_INT_8_8_8_8_REV:
      case GL_UNSIGNED_BYTE:
         textureFormat = (MAPSURF_32BIT | MT_32BIT_ABGR8888);
         cpp = 4;
         break;
      default:
         return GL_FALSE;
      }
      break;
   case GL_BGR:
      switch (type) {
      case GL_UNSIGNED_SHORT_5_6_5_REV:
         textureFormat = (MAPSURF_16BIT | MT_16BIT_RGB565);
         cpp = 2;
         break;
      default:
         return GL_FALSE;
      }
      break;
   case GL_RGB:
      switch (type) {
      case GL_UNSIGNED_SHORT_5_6_5:
         textureFormat = (MAPSURF_16BIT | MT_16BIT_RGB565);
         cpp = 2;
         break;
      default:
         return GL_FALSE;
      }
      break;

   default:
      return GL_FALSE;
   }


   if ((pitch * cpp) & 3) {
      _mesa_printf("%s: texture is not dword pitch\n", __FUNCTION__);
      return GL_FALSE;
   }

/*    intel_region_release(&intel->meta.tex_region[0]); */
/*    intel_region_reference(&intel->meta.tex_region[0], region); */
   intel->meta.tex_buffer[0] = buffer;
   intel->meta.tex_offset[0] = offset;
#endif

   /* Don't do this, fill in the state objects as appropriate instead. 
    */
#if 0
   state[I915_TEXREG_MS3] = (((height - 1) << MS3_HEIGHT_SHIFT) |
                             ((pitch - 1) << MS3_WIDTH_SHIFT) |
                             textureFormat | MS3_USE_FENCE_REGS);

   state[I915_TEXREG_MS4] = (((((pitch * cpp) / 4) - 1) << MS4_PITCH_SHIFT) |
                             MS4_CUBE_FACE_ENA_MASK |
                             ((((numLevels - 1) * 4)) << MS4_MAX_LOD_SHIFT));

   state[I915_TEXREG_SS2] = ((FILTER_NEAREST << SS2_MIN_FILTER_SHIFT) |
                             (MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
                             (FILTER_NEAREST << SS2_MAG_FILTER_SHIFT));

   state[I915_TEXREG_SS3] = ((TEXCOORDMODE_WRAP << SS3_TCX_ADDR_MODE_SHIFT) |
                             (TEXCOORDMODE_WRAP << SS3_TCY_ADDR_MODE_SHIFT) |
                             (TEXCOORDMODE_WRAP << SS3_TCZ_ADDR_MODE_SHIFT) |
                             (unit << SS3_TEXTUREMAP_INDEX_SHIFT));

   state[I915_TEXREG_SS4] = 0;

   i915->meta.emitted &= ~I915_UPLOAD_TEX(0);
#endif

   return GL_TRUE;
}



void intel_meta_draw_region( struct intel_context *intel,
			     struct intel_region *draw_region,
			     struct intel_region *depth_region )
{
#if 0
   if (!intel->metaops.saved_draw_region) {
      intel->metaops.saved_draw_region = intel->state.draw_region;
      intel->metaops.saved_depth_region = intel->state.depth_region;
   }

   intel->state.draw_region = draw_region;
   intel->state.depth_region = depth_region;

   intel->state.dirty.mesa |= _NEW_BUFFERS;
#endif
}


/* Big problem is that tnl doesn't know about our internal
 * state-swaping mechanism so wouldn't respect it if we passed this
 * here.  That *should* get fixed by pushing the state mechanism
 * higher into mesa, but I haven't got there yet.  In the meantime, 
 */
void
intel_meta_draw_poly(struct intel_context *intel,
                     GLuint n,
                     GLfloat xy[][2],
                     GLfloat z, GLuint color, GLfloat tex[][2])
{
   GLint i;
   intel_update_software_state( intel );

   {
      /* Must be after call to intel_update_software_state():
       */
      GLuint dwords = 2+n*intel->vertex_size;
      intel_emit_hardware_state( intel, dwords );

      /* All 3d primitives should be emitted with INTEL_BATCH_CLIPRECTS,
       * otherwise the drawing origin (DR4) might not be set correctly.
       *
       * XXX: use the vb for vertices!
       */
      BEGIN_BATCH(dwords, INTEL_BATCH_CLIPRECTS);

      OUT_BATCH( _3DPRIMITIVE |
		 PRIM3D_TRIFAN | 
		 (n * intel->vertex_size - 1 ));


      for (i = 0; i < n; i++) {
	 OUT_BATCH_F( xy[i][0] );
	 OUT_BATCH_F( xy[i][1] );
	 OUT_BATCH_F( z );
	 OUT_BATCH( color );
	 if (intel->vertex_size == 6) {
	    OUT_BATCH_F( tex[i][0] );
	    OUT_BATCH_F( tex[i][1] );
	 }
	 else {
	    assert(intel->vertex_size == 4);
	 }
      }
      ADVANCE_BATCH();
   }
}

void
intel_meta_draw_quad(struct intel_context *intel,
                     GLfloat x0, GLfloat x1,
                     GLfloat y0, GLfloat y1,
                     GLfloat z,
                     GLuint color,
                     GLfloat s0, GLfloat s1, GLfloat t0, GLfloat t1)
{
   GLfloat xy[4][2];
   GLfloat tex[4][2];

   xy[0][0] = x0;
   xy[0][1] = y0;
   xy[1][0] = x1;
   xy[1][1] = y0;
   xy[2][0] = x1;
   xy[2][1] = y1;
   xy[3][0] = x0;
   xy[3][1] = y1;

   tex[0][0] = s0;
   tex[0][1] = t0;
   tex[1][0] = s1;
   tex[1][1] = t0;
   tex[2][0] = s1;
   tex[2][1] = t1;
   tex[3][0] = s0;
   tex[3][1] = t1;

   intel_meta_draw_poly(intel, 4, xy, z, color, tex);
}


void intel_install_meta_state( struct intel_context *intel )
{
   assert(0);

   install_state(intel);
   
   intel_meta_no_texture(intel);
   intel_meta_flat_shade(intel);

/*    intel->metaops.restore_draw_mask = ctx->DrawBuffer->_ColorDrawBufferMask[0]; */
/*    intel->metaops.restore_fp = ctx->FragmentProgram.Current; */

   /* This works without adjusting refcounts.  Fix later? 
    */
/*    intel->metaops.saved_draw_region = intel->state.draw_region; */
/*    intel->metaops.saved_depth_region = intel->state.depth_region; */
   intel->metaops.active = 1;
   
   intel->state.dirty.intel |= INTEL_NEW_METAOPS;
}

void intel_leave_meta_state( struct intel_context *intel )
{
   restore_state(intel);

/*    ctx->DrawBuffer->_ColorDrawBufferMask[0] = intel->metaops.restore_draw_mask; */
/*    ctx->FragmentProgram.Current = intel->metaops.restore_fp; */

/*    intel->state.draw_region = intel->metaops.saved_draw_region; */
/*    intel->state.depth_region = intel->metaops.saved_depth_region; */
/*    intel->metaops.saved_draw_region = NULL; */
/*    intel->metaops.saved_depth_region = NULL; */
   intel->metaops.active = 0;

   intel->state.dirty.mesa |= _NEW_BUFFERS;
   intel->state.dirty.intel |= INTEL_NEW_METAOPS;
}



void intel_metaops_init( struct intel_context *intel )
{
/*    GLcontext *ctx = &intel->ctx; */

   init_state(intel);

#if 0
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
#endif

/*    intel->metaops.state.VertexProgram->Current = intel->metaops.vp; */
/*    intel->metaops.state.VertexProgram->_Enabled = GL_TRUE; */
}

void intel_metaops_destroy( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   if (intel->metaops.vbo)
      ctx->Driver.DeleteBuffer( ctx, intel->metaops.vbo );

/*    ctx->Driver.DeleteProgram( ctx, intel->metaops.vp ); */
}
