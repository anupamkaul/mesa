/* $Id: t_array_import.c,v 1.25.2.3 2002/11/19 12:01:29 keithw Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  4.1
 *
 * Copyright (C) 1999-2002  Brian Paul   All Rights Reserved.
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
 *    Keith Whitwell <keithw@valinux.com>
 */

#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "mem.h"
#include "mmath.h"
#include "state.h"
#include "mtypes.h"

#include "array_cache/acache.h"
#include "math/m_translate.h"

#include "t_array_import.h"
#include "t_context.h"


static void _tnl_import_vertex( GLcontext *ctx,
				GLboolean writeable,
				GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_POS] = _ac_import_vertex(ctx,
						    GL_FLOAT,
						    0, 0, 0, 0);
}

static void _tnl_import_normal( GLcontext *ctx,
				GLboolean writeable,
				GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_NORMAL] = _ac_import_normal(ctx, GL_FLOAT, 0, 0, 0);
}


static void _tnl_import_color( GLcontext *ctx,
			       GLenum type,
			       GLboolean writeable,
			       GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_COLOR0] = _ac_import_color(ctx, GL_FLOAT,
						      0, 4, 0, 0);
}


static void _tnl_import_secondarycolor( GLcontext *ctx,
					GLenum type,
					GLboolean writeable,
					GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_COLOR1] = _ac_import_secondarycolor(ctx, GL_FLOAT,
							       0, 4, 0, 0);
}

static void _tnl_import_fogcoord( GLcontext *ctx,
				  GLboolean writeable,
				  GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_FOG] = _ac_import_fogcoord(ctx, GL_FLOAT,
						      0, 0, 0);
}

static void _tnl_import_index( GLcontext *ctx,
			       GLboolean writeable,
			       GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_INDEX] = _ac_import_index(ctx, GL_UNSIGNED_INT,
						     0, 0, 0);
}


static void _tnl_import_texcoord( GLcontext *ctx,
				  GLuint unit,
				  GLboolean writeable,
				  GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_TEX0+unit] = _ac_import_texcoord(ctx, unit, GL_FLOAT,
							    0, 0, 0, 0);
}


static void _tnl_import_edgeflag( GLcontext *ctx,
				  GLboolean writeable,
				  GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[VERT_ATTRIB_EDGEFLAG] = _ac_import_edgeflag(ctx, 
							   GL_UNSIGNED_BYTE,
							   0, 0, 0);
}



static void _tnl_import_attrib( GLcontext *ctx,
                                GLuint index,
                                GLboolean writeable,
                                GLboolean stride )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   VB->Attrib[index] = _ac_import_attrib(ctx, index, GL_FLOAT,
					  0,
					  4,  /* want GLfloat[4]??? */
					  0,
					  0);
}





void _tnl_vb_bind_arrays( GLcontext *ctx, GLint start, GLsizei count )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   const GLuint *inputs = tnl->pipeline.inputs;

   VB->Count = count - start;
   VB->FirstClipped = VB->Count;
   VB->Elts = NULL;

   if (ctx->Array.LockCount) {
      ASSERT(start == (GLint) ctx->Array.LockFirst);
      ASSERT(count == (GLint) ctx->Array.LockCount);
   }

   _ac_import_range( ctx, start, count );

   if (TEST_BIT(inputs, VERT_ATTRIB_POS)) 
      _tnl_import_vertex( ctx, 0, 0 );

   if (TEST_BIT(inputs, VERT_ATTRIB_NORMAL)) 
      _tnl_import_normal( ctx, 0, 0 );

   /* XXX: For colormaterial, should bind incoming color to *material*
    * attributes, not color.
    */
   if (TEST_BIT(inputs, VERT_ATTRIB_COLOR0)) 
      _tnl_import_color( ctx, 0, 0, 0 );

   {
      GLuint unit;
      for (unit = 0; unit < ctx->Const.MaxTextureUnits; unit++) 
	 if (TEST_BIT(inputs, (VERT_ATTRIB_TEX0+unit))) 
	    _tnl_import_texcoord( ctx, unit, 0, 0 );
   }

   if (TEST_BIT(inputs, VERT_ATTRIB_INDEX))
      _tnl_import_index( ctx, 0, 0 );

   if (TEST_BIT(inputs, VERT_ATTRIB_EDGEFLAG))
      _tnl_import_edgeflag( ctx, 0, 0 );

   if (TEST_BIT(inputs, VERT_ATTRIB_FOG))
      _tnl_import_fogcoord( ctx, 0, 0 );

   if (TEST_BIT(inputs, VERT_BIT_COLOR1))
      _tnl_import_secondarycolor( ctx, 0, 0, 0 );

   /* If vertex programs are enabled, vertex-attrib arrays override
    * the old type arrays, where enabled.
    *
    * XXX check program->InputsRead to reduce work here 
    */
   if (ctx->VertexProgram.Enabled) {
      GLuint index;
      for (index = 0; index < VERT_ATTRIB_MAX; index++) 
         _tnl_import_attrib( ctx, index, 0, 0 );
   }
}
