/* radeon_state.c -- State support for Radeon -*- linux-c -*-
 *
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Kevin E. Martin <martin@valinux.com>
 */

#include "radeon.h"
#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "radeon_drm.h"
#include "radeon_drv.h"


/* Emit 1.1 state
 */
static void radeon_emit_state( drm_radeon_private_t *dev_priv,
			       drm_radeon_context_regs_t *ctx,
			       drm_radeon_texture_regs_t *tex,
			       unsigned int dirty )
{
	RING_LOCALS;
	DRM_DEBUG( "dirty=0x%08x\n", dirty );

	if ( dirty & RADEON_UPLOAD_CONTEXT ) {
		BEGIN_RING( 14 );
		OUT_RING( CP_PACKET0( RADEON_PP_MISC, 6 ) );
		OUT_RING( ctx->pp_misc );
		OUT_RING( ctx->pp_fog_color );
		OUT_RING( ctx->re_solid_color );
		OUT_RING( ctx->rb3d_blendcntl );
		OUT_RING( ctx->rb3d_depthoffset );
		OUT_RING( ctx->rb3d_depthpitch );
		OUT_RING( ctx->rb3d_zstencilcntl );
		OUT_RING( CP_PACKET0( RADEON_PP_CNTL, 2 ) );
		OUT_RING( ctx->pp_cntl );
		OUT_RING( ctx->rb3d_cntl );
		OUT_RING( ctx->rb3d_coloroffset );
		OUT_RING( CP_PACKET0( RADEON_RB3D_COLORPITCH, 0 ) );
		OUT_RING( ctx->rb3d_colorpitch );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_VERTFMT ) {
		BEGIN_RING( 2 );
		OUT_RING( CP_PACKET0( RADEON_SE_COORD_FMT, 0 ) );
		OUT_RING( ctx->se_coord_fmt );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_LINE ) {
		BEGIN_RING( 5 );
		OUT_RING( CP_PACKET0( RADEON_RE_LINE_PATTERN, 1 ) );
		OUT_RING( ctx->re_line_pattern );
		OUT_RING( ctx->re_line_state );
		OUT_RING( CP_PACKET0( RADEON_SE_LINE_WIDTH, 0 ) );
		OUT_RING( ctx->se_line_width );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_BUMPMAP ) {
		BEGIN_RING( 5 );
		OUT_RING( CP_PACKET0( RADEON_PP_LUM_MATRIX, 0 ) );
		OUT_RING( ctx->pp_lum_matrix );
		OUT_RING( CP_PACKET0( RADEON_PP_ROT_MATRIX_0, 1 ) );
		OUT_RING( ctx->pp_rot_matrix_0 );
		OUT_RING( ctx->pp_rot_matrix_1 );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_MASKS ) {
		BEGIN_RING( 4 );
		OUT_RING( CP_PACKET0( RADEON_RB3D_STENCILREFMASK, 2 ) );
		OUT_RING( ctx->rb3d_stencilrefmask );
		OUT_RING( ctx->rb3d_ropcntl );
		OUT_RING( ctx->rb3d_planemask );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_VIEWPORT ) {
		BEGIN_RING( 7 );
		OUT_RING( CP_PACKET0( RADEON_SE_VPORT_XSCALE, 5 ) );
		OUT_RING( ctx->se_vport_xscale );
		OUT_RING( ctx->se_vport_xoffset );
		OUT_RING( ctx->se_vport_yscale );
		OUT_RING( ctx->se_vport_yoffset );
		OUT_RING( ctx->se_vport_zscale );
		OUT_RING( ctx->se_vport_zoffset );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_SETUP ) {
		BEGIN_RING( 4 );
		OUT_RING( CP_PACKET0( RADEON_SE_CNTL, 0 ) );
		OUT_RING( ctx->se_cntl );
		OUT_RING( CP_PACKET0( RADEON_SE_CNTL_STATUS, 0 ) );
		OUT_RING( ctx->se_cntl_status );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_MISC ) {
		BEGIN_RING( 2 );
		OUT_RING( CP_PACKET0( RADEON_RE_MISC, 0 ) );
		OUT_RING( ctx->re_misc );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_TEX0 ) {
		BEGIN_RING( 9 );
		OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_0, 5 ) );
		OUT_RING( tex[0].pp_txfilter );
		OUT_RING( tex[0].pp_txformat );
		OUT_RING( tex[0].pp_txoffset );
		OUT_RING( tex[0].pp_txcblend );
		OUT_RING( tex[0].pp_txablend );
		OUT_RING( tex[0].pp_tfactor );
		OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_0, 0 ) );
		OUT_RING( tex[0].pp_border_color );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_TEX1 ) {
		BEGIN_RING( 9 );
		OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_1, 5 ) );
		OUT_RING( tex[1].pp_txfilter );
		OUT_RING( tex[1].pp_txformat );
		OUT_RING( tex[1].pp_txoffset );
		OUT_RING( tex[1].pp_txcblend );
		OUT_RING( tex[1].pp_txablend );
		OUT_RING( tex[1].pp_tfactor );
		OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_1, 0 ) );
		OUT_RING( tex[1].pp_border_color );
		ADVANCE_RING();
	}

	if ( dirty & RADEON_UPLOAD_TEX2 ) {
		BEGIN_RING( 9 );
		OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_2, 5 ) );
		OUT_RING( tex[2].pp_txfilter );
		OUT_RING( tex[2].pp_txformat );
		OUT_RING( tex[2].pp_txoffset );
		OUT_RING( tex[2].pp_txcblend );
		OUT_RING( tex[2].pp_txablend );
		OUT_RING( tex[2].pp_tfactor );
		OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_2, 0 ) );
		OUT_RING( tex[2].pp_border_color );
		ADVANCE_RING();
	}
}


/* Emit 1.2 state
 */
static void radeon_emit_state2( drm_radeon_private_t *dev_priv,
				drm_radeon_state_t *state )
{
	RING_LOCALS;

	if (state->dirty & RADEON_UPLOAD_ZBIAS) {
		BEGIN_RING( 3 );
		OUT_RING( CP_PACKET0( RADEON_SE_ZBIAS_FACTOR, 1 ) );
		OUT_RING( state->context2.se_zbias_factor ); 
		OUT_RING( state->context2.se_zbias_constant ); 
		ADVANCE_RING();
	}

	radeon_emit_state( dev_priv, &state->context, 
			   state->tex, state->dirty );
}

static int bad_prim_vertex_nr( int primitive, int nr )
{
	switch (primitive & RADEON_PRIM_TYPE_MASK) {
	case RADEON_PRIM_TYPE_NONE:
	case RADEON_PRIM_TYPE_POINT:
		return nr < 1;
	case RADEON_PRIM_TYPE_LINE:
		return (nr & 1) || nr == 0;
	case RADEON_PRIM_TYPE_LINE_STRIP:
		return nr < 2;
	case RADEON_PRIM_TYPE_TRI_LIST:
	case RADEON_PRIM_TYPE_3VRT_POINT_LIST:
	case RADEON_PRIM_TYPE_3VRT_LINE_LIST:
	case RADEON_PRIM_TYPE_RECT_LIST:
		return nr % 3 || nr == 0;
	case RADEON_PRIM_TYPE_TRI_FAN:
	case RADEON_PRIM_TYPE_TRI_STRIP:
		return nr < 3;
	default:
		return 1;
	}	
}



static void radeon_cp_dispatch_indices( drm_device_t *dev,
					drm_buf_t *elt_buf,
					drm_radeon_tcl_prim_t *prim, 
					drm_clip_rect_t *boxes,
					int nbox )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_clip_rect_t box;
	int offset = dev_priv->agp_buffers_offset + prim->offset;
	u32 *data;
	int dwords;
	int i = 0;
	int start = prim->start + RADEON_INDEX_PRIM_OFFSET;
	int count = (prim->finish - start) / sizeof(u16);

	DRM_DEBUG("hwprim 0x%x vfmt 0x%x %d..%d offset: %x nr %d\n",
		  prim->prim,
		  prim->vc_format,
		  prim->start,
		  prim->finish,
		  prim->offset,
		  prim->numverts);

	if (bad_prim_vertex_nr( prim->prim, count )) {
		DRM_ERROR( "bad prim %x count %d\n", 
			   prim->prim, count );
		return;
	}


	if ( start >= prim->finish ||
	     (prim->start & 0x7) ) {
		DRM_ERROR( "buffer prim %d\n", prim->prim );
		return;
	}

	dwords = (prim->finish - prim->start + 3) / sizeof(u32);

	data = (u32 *)((char *)dev_priv->buffers->handle +
		       elt_buf->offset + prim->start);

	data[0] = CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, dwords-2 );
	data[1] = offset;
	data[2] = prim->numverts;
	data[3] = prim->vc_format;
	data[4] = (prim->prim |
		   RADEON_PRIM_WALK_IND |
		   RADEON_COLOR_ORDER_RGBA |
		   RADEON_VTX_FMT_RADEON_MODE |
		   (count << RADEON_NUM_VERTICES_SHIFT) );

	do {
		if ( i < nbox ) {
			if (DRM_COPY_FROM_USER_UNCHECKED( &box, &boxes[i], sizeof(box) ))
				return;
			
			radeon_emit_clip_rect( dev_priv, &box );
		}

		radeon_cp_dispatch_indirect( dev, elt_buf,
					     prim->start,
					     prim->finish );

		i++;
	} while ( i < nbox );

}

typedef struct {
	unsigned int start;
	unsigned int finish;
	unsigned int prim;
	unsigned int numverts;
	unsigned int offset;   
        unsigned int vc_format;
} drm_radeon_tcl_prim_t;

static void radeon_cp_dispatch_vertex( drm_device_t *dev,
				       drm_buf_t *buf,
				       drm_radeon_tcl_prim_t *prim,
				       drm_clip_rect_t *boxes,
				       int nbox )

{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_clip_rect_t box;
	int offset = dev_priv->agp_buffers_offset + buf->offset + prim->start;
	int numverts = (int)prim->numverts;
	int i = 0;
	RING_LOCALS;

	DRM_DEBUG("hwprim 0x%x vfmt 0x%x %d..%d %d verts\n",
		  prim->prim,
		  prim->vc_format,
		  prim->start,
		  prim->finish,
		  prim->numverts);

	if (bad_prim_vertex_nr( prim->prim, prim->numverts )) {
		DRM_ERROR( "bad prim %x numverts %d\n", 
			   prim->prim, prim->numverts );
		return;
	}

	do {
		/* Emit the next cliprect */
		if ( i < nbox ) {
			if (DRM_COPY_FROM_USER_UNCHECKED( &box, &boxes[i], sizeof(box) ))
				return;

			radeon_emit_clip_rect( dev_priv, &box );
		}

		/* Emit the vertex buffer rendering commands */
		BEGIN_RING( 5 );

		OUT_RING( CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, 3 ) );
		OUT_RING( offset );
		OUT_RING( numverts );
		OUT_RING( prim->vc_format );
		OUT_RING( prim->prim | RADEON_PRIM_WALK_LIST |
			  RADEON_COLOR_ORDER_RGBA |
			  RADEON_VTX_FMT_RADEON_MODE |
			  (numverts << RADEON_NUM_VERTICES_SHIFT) );

		ADVANCE_RING();

		i++;
	} while ( i < nbox );
}


int radeon_cp_vertex( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_vertex_t vertex;
	drm_radeon_tcl_prim_t prim;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( vertex, (drm_radeon_vertex_t *)data,
			     sizeof(vertex) );

	DRM_DEBUG( "pid=%d index=%d count=%d discard=%d\n",
		   DRM_CURRENTPID,
		   vertex.idx, vertex.count, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}
	if ( vertex.prim < 0 ||
	     vertex.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", vertex.prim );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[vertex.idx];

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return DRM_ERR(EINVAL);
	}

	/* Build up a prim_t record:
	 */
	if (vertex.count) {
		buf->used = vertex.count; /* not used? */

		if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
			radeon_emit_state( dev_priv,
					   &sarea_priv->context_state,
					   sarea_priv->tex_state,
					   sarea_priv->dirty );
			
			sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
					       RADEON_UPLOAD_TEX1IMAGES |
					       RADEON_UPLOAD_TEX2IMAGES |
					       RADEON_REQUIRE_QUIESCENCE);
		}

		prim.start = 0;
		prim.finish = vertex.count; /* unused */
		prim.prim = vertex.prim;
		prim.numverts = vertex.count;
		prim.vc_format = dev_priv->sarea_priv->vc_format;
		
		radeon_cp_dispatch_vertex( dev, buf, &prim,
					   dev_priv->sarea_priv->boxes,
					   dev_priv->sarea_priv->nbox );
	}

	if (vertex.discard) {
		radeon_cp_discard_buffer( dev, buf );
	}

	COMMIT_RING();
	return 0;
}

int radeon_cp_indices( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_indices_t elts;
	drm_radeon_tcl_prim_t prim;
	int count;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( elts, (drm_radeon_indices_t *)data,
			     sizeof(elts) );

	DRM_DEBUG( "pid=%d index=%d start=%d end=%d discard=%d\n",
		   DRM_CURRENTPID,
		   elts.idx, elts.start, elts.end, elts.discard );

	if ( elts.idx < 0 || elts.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   elts.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}
	if ( elts.prim < 0 ||
	     elts.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", elts.prim );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[elts.idx];

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", elts.idx );
		return DRM_ERR(EINVAL);
	}

	count = (elts.end - elts.start) / sizeof(u16);
	elts.start -= RADEON_INDEX_PRIM_OFFSET;

	if ( elts.start & 0x7 ) {
		DRM_ERROR( "misaligned buffer 0x%x\n", elts.start );
		return DRM_ERR(EINVAL);
	}
	if ( elts.start < buf->used ) {
		DRM_ERROR( "no header 0x%x - 0x%x\n", elts.start, buf->used );
		return DRM_ERR(EINVAL);
	}

	buf->used = elts.end;

	if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
		radeon_emit_state( dev_priv,
				   &sarea_priv->context_state,
				   sarea_priv->tex_state,
				   sarea_priv->dirty );

		sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
				       RADEON_UPLOAD_TEX1IMAGES |
				       RADEON_UPLOAD_TEX2IMAGES |
				       RADEON_REQUIRE_QUIESCENCE);
	}


	/* Build up a prim_t record:
	 */
	prim.start = elts.start;
	prim.finish = elts.end; 
	prim.prim = elts.prim;
	prim.offset = 0;	/* offset from start of dma buffers */
	prim.numverts = RADEON_MAX_VB_VERTS; /* duh */
	prim.vc_format = dev_priv->sarea_priv->vc_format;
	
	radeon_cp_dispatch_indices( dev, buf, &prim,
				   dev_priv->sarea_priv->boxes,
				   dev_priv->sarea_priv->nbox );
	if (elts.discard) {
		radeon_cp_discard_buffer( dev, buf );
	}

	COMMIT_RING();
	return 0;
}


int radeon_cp_vertex2( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_vertex2_t vertex;
	int i;
	unsigned char laststate;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( vertex, (drm_radeon_vertex2_t *)data,
			     sizeof(vertex) );

	DRM_DEBUG( "pid=%d index=%d discard=%d\n",
		   DRM_CURRENTPID,
		   vertex.idx, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[vertex.idx];

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}

	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return DRM_ERR(EINVAL);
	}
	
	if (sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS)
		return DRM_ERR(EINVAL);

	for (laststate = 0xff, i = 0 ; i < vertex.nr_prims ; i++) {
		drm_radeon_prim_t prim;
		drm_radeon_tcl_prim_t tclprim;
		
		if ( DRM_COPY_FROM_USER( &prim, &vertex.prim[i], sizeof(prim) ) )
			return DRM_ERR(EFAULT);
		
		if ( prim.stateidx != laststate ) {
			drm_radeon_state_t state;			       
				
			if ( DRM_COPY_FROM_USER( &state, 
					     &vertex.state[prim.stateidx], 
					     sizeof(state) ) )
				return DRM_ERR(EFAULT);

			radeon_emit_state2( dev_priv, &state );

			laststate = prim.stateidx;
		}

		tclprim.start = prim.start;
		tclprim.finish = prim.finish;
		tclprim.prim = prim.prim;
		tclprim.vc_format = prim.vc_format;

		if ( prim.prim & RADEON_PRIM_WALK_IND ) {
			tclprim.offset = prim.numverts * 64;
			tclprim.numverts = RADEON_MAX_VB_VERTS; /* duh */

			radeon_cp_dispatch_indices( dev, buf, &tclprim,
						    sarea_priv->boxes,
						    sarea_priv->nbox);
		} else {
			tclprim.numverts = prim.numverts;
			tclprim.offset = 0; /* not used */

			radeon_cp_dispatch_vertex( dev, buf, &tclprim,
						   sarea_priv->boxes,
						   sarea_priv->nbox);
		}
		
		if (sarea_priv->nbox == 1)
			sarea_priv->nbox = 0;
	}

	if ( vertex.discard ) {
		radeon_cp_discard_buffer( dev, buf );
	}

	COMMIT_RING();
	return 0;
}
