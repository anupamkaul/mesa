/* $Id: ss_tritmp.h,v 1.17.2.1 2002/10/17 14:27:52 keithw Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
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


static void TAG(triangle)(GLcontext *ctx, GLuint e0, GLuint e1, GLuint e2 )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   SWvertex *verts = SWSETUP_CONTEXT(ctx)->verts;
   SWvertex *v[3];
   GLfloat z[3];
   GLfloat offset;
   GLenum mode = GL_FILL;
   GLuint facing = 0;

   v[0] = &verts[e0];
   v[1] = &verts[e1];
   v[2] = &verts[e2];


   if (IND & (SS_TWOSIDE_BIT | SS_OFFSET_BIT | SS_UNFILLED_BIT))
   {
      GLfloat ex = v[0]->win[0] - v[2]->win[0];
      GLfloat ey = v[0]->win[1] - v[2]->win[1];
      GLfloat fx = v[1]->win[0] - v[2]->win[0];
      GLfloat fy = v[1]->win[1] - v[2]->win[1];
      GLfloat cc  = ex*fy - ey*fx;

      if (IND & (SS_TWOSIDE_BIT | SS_UNFILLED_BIT))
      {
	 facing = (cc < 0.0) ^ ctx->Polygon._FrontBit;

	 if (IND & SS_UNFILLED_BIT)
	    mode = facing ? ctx->Polygon.BackMode : ctx->Polygon.FrontMode;
      }

      if (IND & SS_OFFSET_BIT)
      {
	 offset = ctx->Polygon.OffsetUnits;
	 z[0] = v[0]->win[2];
	 z[1] = v[1]->win[2];
	 z[2] = v[2]->win[2];
	 if (cc * cc > 1e-16) {
	    GLfloat ez = z[0] - z[2];
	    GLfloat fz = z[1] - z[2];
	    GLfloat a = ey*fz - ez*fy;
	    GLfloat b = ez*fx - ex*fz;
	    GLfloat ic = 1.0F / cc;
	    GLfloat ac = a * ic;
	    GLfloat bc = b * ic;
	    if (ac < 0.0F) ac = -ac;
	    if (bc < 0.0F) bc = -bc;
	    offset += MAX2(ac, bc) * ctx->Polygon.OffsetFactor;
	 }
         offset *= ctx->MRD;
         /*printf("offset %g\n", offset);*/
      }
   }

   if (mode == GL_POINT) {
      if ((IND & SS_OFFSET_BIT) && ctx->Polygon.OffsetPoint) {
	 v[0]->win[2] += offset;
	 v[1]->win[2] += offset;
	 v[2]->win[2] += offset;
      }
      _swsetup_render_point_tri( ctx, facing, e0, e1, e2 );
   } else if (mode == GL_LINE) {
      if ((IND & SS_OFFSET_BIT) && ctx->Polygon.OffsetLine) {
	 v[0]->win[2] += offset;
	 v[1]->win[2] += offset;
	 v[2]->win[2] += offset;
      }
      _swsetup_render_line_tri( ctx, facing, e0, e1, e2 );
   } else {
      if ((IND & SS_OFFSET_BIT) && ctx->Polygon.OffsetFill) {
	 v[0]->win[2] += offset;
	 v[1]->win[2] += offset;
	 v[2]->win[2] += offset;
      }
      _swrast_Triangle( ctx, facing, v[0], v[1], v[2] );
   }

   if (IND & SS_OFFSET_BIT) {
      v[0]->win[2] = z[0];
      v[1]->win[2] = z[1];
      v[2]->win[2] = z[2];
   }
}



/* Need to fixup edgeflags when decomposing to triangles:
 */
static void TAG(quadfunc)( GLcontext *ctx, GLuint v0,
		       GLuint v1, GLuint v2, GLuint v3 )
{
   if (IND & SS_UNFILLED_BIT) {
      SWvertex *verts = SWSETUP_CONTEXT(ctx)->verts;
      GLubyte ef1 = EDGEFLAG(&verts[v1]);
      GLubyte ef3 = EDGEFLAG(&verts[v3]);
      EDGEFLAG(&verts[v1]) = 0;
      TAG(triangle)( ctx, v0, v1, v3 );
      EDGEFLAG(&verts[v1]) = ef1;
      EDGEFLAG(&verts[v3]) = 0;
      TAG(triangle)( ctx, v1, v2, v3 );
      EDGEFLAG(&verts[v1]) = ef3;
   } else {
      TAG(triangle)( ctx, v0, v1, v3 );
      TAG(triangle)( ctx, v1, v2, v3 );
   }
}




static void TAG(init)( void )
{
   tri_tab[IND] = TAG(triangle);
   quad_tab[IND] = TAG(quadfunc);
}


#undef IND
#undef TAG
