/* $Id: ss_vbtmp.h,v 1.21.2.1 2002/10/17 14:27:52 keithw Exp $ */

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


static void TAG(emit)(GLcontext *ctx, GLuint start, GLuint end, 
		      GLuint newinputs )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   SWvertex *v;
   GLfloat *proj;		/* projected clip coordinates */
   GLfloat *tc[MAX_TEXTURE_UNITS];
   GLchan *color;
   GLchan *spec;
   GLuint *index;
   GLfloat *fog;
   GLfloat *pointSize;
   GLuint tsz[MAX_TEXTURE_UNITS];
   GLuint tstride[MAX_TEXTURE_UNITS];
   GLuint proj_stride, color_stride, spec_stride, index_stride;
   GLuint fog_stride, pointSize_stride;
   GLuint i;
   GLfloat *m = ctx->Viewport._WindowMap.m;
   const GLfloat sx = m[0];
   const GLfloat sy = m[5];
   const GLfloat sz = m[10];
   const GLfloat tx = m[12];
   const GLfloat ty = m[13];
   const GLfloat tz = m[14];
   GLuint maxtex = 0;

   if (IND & TEX0) {
      tc[0] = (GLfloat *)VB->AttrPtr[VERT_ATTRIB_TEX0]->data;
      tsz[0] = VB->AttrPtr[VERT_ATTRIB_TEX0]->size;
      tstride[0] = VB->AttrPtr[VERT_ATTRIB_TEX0]->stride;
   }

   if (IND & EXTRAS && ctx->Texture._EnabledUnits) {
      for (i = 0 ; i < ctx->Const.MaxTextureUnits ; i++) {
	 if (VB->TexCoordPtr[i]) {
	    maxtex = i+1;
	    tc[i] = (GLfloat *)VB->AttrPtr[VERT_ATTRIB_TEX0+i]->data;
	    tsz[i] = VB->AttrPtr[VERT_ATTRIB_TEX0+i]->size;
	    tstride[i] = VB->AttrPtr[VERT_ATTRIB_TEX0+i]->stride;
	 }
	 else tc[i] = 0;
      }
   }

   proj = VB->NdcPtr->data[0];
   proj_stride = VB->NdcPtr->stride;

   if (IND & FOG) {
      fog = (GLfloat *) VB->AttrPtr[VERT_ATTRIB_FOG]->data;
      fog_stride = VB->AttrPtr[VERT_ATTRIB_FOG]->stride;
   }
   if (IND & COLOR) {
      color = (GLfloat *) VB->AttrPtr[VERT_ATTRIB_COLOR0]->data;
      color_stride = VB->AttrPtr[VERT_ATTRIB_COLOR0]->stride;
      if (IND & EXTRAS) {
	 if (VB->AttrPtr[VERT_ATTRIB_BACK_COLOR0]) {
	 }
      }
   }
   if (IND & SPEC) {
      spec = (GLfloat *) VB->AttrPtr[VERT_ATTRIB_COLOR1]->data;
      spec_stride = VB->AttrPtr[VERT_ATTRIB_COLOR1]->stride;
      if (IND & EXTRAS) {
	 if (VB->AttrPtr[VERT_ATTRIB_BACK_COLOR1]) {
	 }
      }
   }
   if (IND & INDEX) {
      index = (GLfloat *) VB->AttrPtr[VERT_ATTRIB_INDEX]->data;
      index_stride = VB->AttrPtr[VERT_ATTRIB_INDEX]->stride;
      if (IND & EXTRAS) {
	 if (VB->AttrPtr[VERT_ATTRIB_BACK_INDEX]) {
	 }
      }
   }

   if (IND & POINT) {
      pointSize = (GLfloat *) VB->PointSizePtr->data;
      pointSize_stride = VB->PointSizePtr->stride;
   }
   

   v = &(SWSETUP_CONTEXT(ctx)->verts[start]);

   for (i=start; i < end; i++, v++) {
      if (VB->ClipMask[i] == 0) {
	 v->win[0] = sx * proj[0] + tx;
	 v->win[1] = sy * proj[1] + ty;
	 v->win[2] = sz * proj[2] + tz;
	 v->win[3] =      proj[3];
      }
      STRIDE_F(proj, proj_stride);

      if (IND & TEX0) {
	 COPY_CLEAN_4V( v->texcoord[0], tsz[0], tc[0] );
	 STRIDE_F(tc[0], tstride[0]);
      }

      if (IND & EXTRAS) {
	 GLuint u;
	 for (u = 0 ; u < maxtex ; u++)
	    if (tc[u]) {
	       COPY_CLEAN_4V( v->texcoord[u], tsz[u], tc[u] );
	       STRIDE_F(tc[u], tstride[u]);
	    }
      }

      if (IND & COLOR) {
	 UNCLAMPED_FLOAT_TO_RGBA_CHAN(v->color[0], color[0]);
	 STRIDE_F(color[0], color_stride[0]);
	 if ((IND & EXTRAS) && color[1]) {
	    UNCLAMPED_FLOAT_TO_RGBA_CHAN(v->color[1], color[1]);
	    STRIDE_F(color[1], color_stride[1]);
	 }
      }

      if (IND & SPEC) {
	 UNCLAMPED_FLOAT_TO_RGB_CHAN(v->specular[0], spec[0]);
	 STRIDE_F(spec[0], spec_stride[0]);
	 if ((IND & EXTRAS) && spec[1]) {
	    UNCLAMPED_FLOAT_TO_RGB_CHAN(v->specular[1], spec[1]);
	    STRIDE_F(spec[1], spec_stride[1]);
	 }
      }

      if (IND & FOG) {
	 v->fog = fog[0];
	 STRIDE_F(fog, fog_stride);
      }

      if (IND & INDEX) {
 	 v->index[0] = (GLuint)index[0];
	 STRIDE_F(index[0], index_stride[0]);
	 if ((IND & EXTRAS) && index[1]) {
	    v->index[1] = (GLuint)index[1];
	    STRIDE_F(index[1], index_stride[1]);
	 }
      }

      if (IND & POINT) {
	 v->pointSize = pointSize[0];
	 STRIDE_F(pointSize, pointSize_stride);
      }

      if (IND & EXTRAS) {
	 v->specular[0][3] = (GLubyte)edgeFlag;
	 STRIDE_F(edgeFlag, edgeFlag_stride);
      }
   }
}



static void TAG(interp)( GLcontext *ctx,
			 GLfloat t,
			 GLuint edst, GLuint eout, GLuint ein,
			 GLboolean force_boundary )
{
   SScontext *swsetup = SWSETUP_CONTEXT(ctx);
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   GLfloat *m = ctx->Viewport._WindowMap.m;
   GLfloat *clip = VB->ClipPtr->data[edst];

   SWvertex *dst = &swsetup->verts[edst];
   SWvertex *in  = &swsetup->verts[ein];
   SWvertex *out = &swsetup->verts[eout];

   /* Avoid division by zero by rearranging order of clip planes?
    */
   if (clip[3] != 0.0) {
      GLfloat oow = 1.0F / clip[3];
      dst->win[0] = m[0]  * clip[0] * oow + m[12];
      dst->win[1] = m[5]  * clip[1] * oow + m[13];
      dst->win[2] = m[10] * clip[2] * oow + m[14];
      dst->win[3] =                   oow;
   }
   
   if (IND & TEX0) {
      INTERP_4F( t, dst->texcoord[0], out->texcoord[0], in->texcoord[0] );
   }

   if ((IND & EXTRAS) && ctx->Texture._EnabledUnits) {
      GLuint u;
      GLuint maxtex = ctx->Const.MaxTextureUnits;
      for (u = 0 ; u < maxtex ; u++)
	 if (VB->AttrPtr[VERT_ATTRIB_TEX0+u]) {
	    INTERP_4F( t, dst->texcoord[u], out->texcoord[u], in->texcoord[u] );
	 }
   }

   if (IND & COLOR) {
      INTERP_CHAN( t, dst->color[0], out->color[0], in->color[0] );
      INTERP_CHAN( t, dst->color[1], out->color[1], in->color[1] );
      INTERP_CHAN( t, dst->color[2], out->color[2], in->color[2] );
      INTERP_CHAN( t, dst->color[3], out->color[3], in->color[3] );
      if (IND & EXTRAS) {
	 INTERP_CHAN( t, dst->color[0], out->color[0], in->color[0] );
	 INTERP_CHAN( t, dst->color[1], out->color[1], in->color[1] );
	 INTERP_CHAN( t, dst->color[2], out->color[2], in->color[2] );
	 INTERP_CHAN( t, dst->color[3], out->color[3], in->color[3] );
      }
   }

   if (IND & SPEC) {
      INTERP_CHAN( t, dst->specular[0], out->specular[0], in->specular[0] );
      INTERP_CHAN( t, dst->specular[1], out->specular[1], in->specular[1] );
      INTERP_CHAN( t, dst->specular[2], out->specular[2], in->specular[2] );
      if (IND & EXTRAS) {
	 INTERP_CHAN( t, dst->specular[0], out->specular[0], in->specular[0] );
	 INTERP_CHAN( t, dst->specular[1], out->specular[1], in->specular[1] );
	 INTERP_CHAN( t, dst->specular[2], out->specular[2], in->specular[2] );
      }
   }

   if (IND & FOG) {
      INTERP_F( t, dst->fog, out->fog, in->fog );
   }

   if (IND & INDEX) {
      INTERP_UI( t, dst->index, out->index, in->index );
      if (IND & EXTRAS) {
	 INTERP_UI( t, dst->back_index, out->back_index, in->back_index );
      }
   }

   /* XXX Point size interpolation??? */
   if (IND & POINT) {
      INTERP_F( t, dst->pointSize, out->pointSize, in->pointSize );
   }

   /* Edgeflag -- stored in spec[0][3] */
   if (IND & EXTRAS) {
      dst->specular[0][3] = out->specular[0][3] || force_boundary;
   }
}


static void TAG(copy_pv)( GLcontext *ctx, GLuint edst, GLuint esrc )
{
   SScontext *swsetup = SWSETUP_CONTEXT(ctx);
   SWvertex *dst = &swsetup->verts[edst];
   SWvertex *src = &swsetup->verts[esrc];

   if (IND & COLOR) {
      COPY_CHAN4( dst->color[0], src->color[0] );
      if (IND & EXTRAS) 
	 COPY_CHAN4( dst->color[1], src->color[1] );
   }

   if (IND & SPEC) {
      COPY_3V( dst->specular[0], src->specular[0] );
      if (IND & EXTRAS) 
	 COPY_3V( dst->specular[1], src->specular[1] );
   }

   if (IND & INDEX) {
      dst->index[0] = src->index[0];
      if (IND & EXTRAS) 
	 dst->index[1] = src->index[1];
   }
}


static void TAG(init)( void )
{
   setup_tab[IND] = TAG(emit);
   interp_tab[IND] = TAG(interp);
   copy_pv_tab[IND] = TAG(copy_pv);
}

#undef TAG
#undef IND
#undef SETUP_FLAGS
