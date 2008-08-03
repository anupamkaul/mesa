/*
Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.

The Weather Channel (TM) funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86 license.
This notice must be preserved.

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

**************************************************************************/

/**
 * \file
 *
 * \author Keith Whitwell <keith@tungstengraphics.com>
 */

#include "glheader.h"
#include "mtypes.h"
#include "colormac.h"
#include "imports.h"
#include "macros.h"
#include "image.h"

#include "swrast_setup/swrast_setup.h"
#include "math/m_translate.h"
#include "tnl/tnl.h"
#include "tnl/t_context.h"

#include "r300_context.h"
#include "radeon_ioctl.h"
#include "r300_state.h"
#include "r300_emit.h"
#include "r300_ioctl.h"

#include "r300_mem.h"

#if SWIZZLE_X != R300_INPUT_ROUTE_SELECT_X || \
    SWIZZLE_Y != R300_INPUT_ROUTE_SELECT_Y || \
    SWIZZLE_Z != R300_INPUT_ROUTE_SELECT_Z || \
    SWIZZLE_W != R300_INPUT_ROUTE_SELECT_W || \
    SWIZZLE_ZERO != R300_INPUT_ROUTE_SELECT_ZERO || \
    SWIZZLE_ONE != R300_INPUT_ROUTE_SELECT_ONE
#error Cannot change these!
#endif

#define DEBUG_ALL DEBUG_VERTS

#if defined(USE_X86_ASM)
#define COPY_DWORDS( dst, src, nr )					\
do {									\
	int __tmp;							\
	__asm__ __volatile__( "rep ; movsl"				\
			      : "=%c" (__tmp), "=D" (dst), "=S" (__tmp)	\
			      : "0" (nr),				\
			        "D" ((long)dst),			\
			        "S" ((long)src) );			\
} while (0)
#else
#define COPY_DWORDS( dst, src, nr )		\
do {						\
   int j;					\
   for ( j = 0 ; j < nr ; j++ )			\
      dst[j] = ((int *)src)[j];			\
   dst += nr;					\
} while (0)
#endif

static void r300EmitVec4(uint32_t *out, GLvoid * data, int stride, int count)
{
	int i;

	if (RADEON_DEBUG & DEBUG_VERTS)
		fprintf(stderr, "%s count %d stride %d out %p data %p\n",
			__FUNCTION__, count, stride, (void *)out, (void *)data);

	if (stride == 4)
		COPY_DWORDS(out, data, count);
	else
		for (i = 0; i < count; i++) {
			out[0] = *(int *)data;
			out++;
			data += stride;
		}
}

static void r300EmitVec8(uint32_t *out, GLvoid * data, int stride, int count)
{
	int i;

	if (RADEON_DEBUG & DEBUG_VERTS)
		fprintf(stderr, "%s count %d stride %d out %p data %p\n",
			__FUNCTION__, count, stride, (void *)out, (void *)data);

	if (stride == 8)
		COPY_DWORDS(out, data, count * 2);
	else
		for (i = 0; i < count; i++) {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out += 2;
			data += stride;
		}
}

static void r300EmitVec12(uint32_t *out, GLvoid * data, int stride, int count)
{
	int i;

	if (RADEON_DEBUG & DEBUG_VERTS)
		fprintf(stderr, "%s count %d stride %d out %p data %p\n",
			__FUNCTION__, count, stride, (void *)out, (void *)data);

	if (stride == 12)
		COPY_DWORDS(out, data, count * 3);
	else
		for (i = 0; i < count; i++) {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out[2] = *(int *)(data + 8);
			out += 3;
			data += stride;
		}
}

static void r300EmitVec16(uint32_t *out, GLvoid * data, int stride, int count)
{
	int i;

	if (RADEON_DEBUG & DEBUG_VERTS)
		fprintf(stderr, "%s count %d stride %d out %p data %p\n",
			__FUNCTION__, count, stride, (void *)out, (void *)data);

	if (stride == 16)
		COPY_DWORDS(out, data, count * 4);
	else
		for (i = 0; i < count; i++) {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out[2] = *(int *)(data + 8);
			out[3] = *(int *)(data + 12);
			out += 4;
			data += stride;
		}
}


static void r300EmitVec(GLcontext * ctx, struct r300_aos *aos,
			GLvoid * data, int size, int stride, int count)
{
	r300ContextPtr rmesa = R300_CONTEXT(ctx);
	struct r300_dma_region dma;
	uint32_t *out;

	dma.bo = 0;
	if (stride == 0) {
		r300AllocDmaRegion(rmesa, &dma, size * 4, 32);
		count = 1;
		aos->stride = 0;
	} else {
		r300AllocDmaRegion(rmesa, &dma, size * count * 4, 32);
		aos->stride = size;
	}

	aos->bo = dma.bo; /* Steal reference to bo from dma region */
	aos->offset = dma.start;
	aos->components = size;
	aos->count = count;

	out = (uint32_t*)((char*)aos->bo->virtual + aos->offset);
	switch (size) {
	case 1: r300EmitVec4(out, data, stride, count); break;
	case 2: r300EmitVec8(out, data, stride, count); break;
	case 3: r300EmitVec12(out, data, stride, count); break;
	case 4: r300EmitVec16(out, data, stride, count); break;
	default:
		assert(0);
		break;
	}
}

#define DW_SIZE(x) ((inputs[tab[(x)]] << R300_DST_VEC_LOC_SHIFT) |	\
		    (attribptr[tab[(x)]]->size - 1) << R300_DATA_TYPE_0_SHIFT)

GLuint r300VAPInputRoute0(uint32_t * dst, GLvector4f ** attribptr,
				 int *inputs, GLint * tab, GLuint nr)
{
	GLuint i, dw;

	/* type, inputs, stop bit, size */
	for (i = 0; i < nr; i += 2) {
		/* make sure input is valid, would lockup the gpu */
		assert(inputs[tab[i]] != -1);
		dw = (R300_SIGNED | DW_SIZE(i));
		if (i + 1 == nr) {
			dw |= R300_LAST_VEC << R300_DATA_TYPE_0_SHIFT;
		} else {
			assert(inputs[tab[i + 1]] != -1);
			dw |= (R300_SIGNED |
			       DW_SIZE(i + 1)) << R300_DATA_TYPE_1_SHIFT;
			if (i + 2 == nr) {
				dw |= R300_LAST_VEC << R300_DATA_TYPE_1_SHIFT;
			}
		}
		dst[i >> 1] = dw;
	}

	return (nr + 1) >> 1;
}

static GLuint r300VAPInputRoute1Swizzle(int swizzle[4])
{
	return (swizzle[0] << R300_SWIZZLE_SELECT_X_SHIFT) |
	    (swizzle[1] << R300_SWIZZLE_SELECT_Y_SHIFT) |
	    (swizzle[2] << R300_SWIZZLE_SELECT_Z_SHIFT) |
	    (swizzle[3] << R300_SWIZZLE_SELECT_W_SHIFT);
}

GLuint r300VAPInputRoute1(uint32_t * dst, int swizzle[][4], GLuint nr)
{
	GLuint i, dw;

	for (i = 0; i < nr; i += 2) {
		dw = (r300VAPInputRoute1Swizzle(swizzle[i]) |
		      ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y |
			R300_WRITE_ENA_Z | R300_WRITE_ENA_W) << R300_WRITE_ENA_SHIFT)) << R300_SWIZZLE0_SHIFT;
		if (i + 1 < nr) {
			dw |= (r300VAPInputRoute1Swizzle(swizzle[i + 1]) |
			       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y |
				 R300_WRITE_ENA_Z | R300_WRITE_ENA_W) << R300_WRITE_ENA_SHIFT)) << R300_SWIZZLE1_SHIFT;
		}
		dst[i >> 1] = dw;
	}

	return (nr + 1) >> 1;
}

GLuint r300VAPInputCntl0(GLcontext * ctx, GLuint InputsRead)
{
	/* No idea what this value means. I have seen other values written to
	 * this register... */
	return 0x5555;
}

GLuint r300VAPInputCntl1(GLcontext * ctx, GLuint InputsRead)
{
	r300ContextPtr rmesa = R300_CONTEXT(ctx);
	GLuint i, vic_1 = 0;

	if (InputsRead & (1 << VERT_ATTRIB_POS))
		vic_1 |= R300_INPUT_CNTL_POS;

	if (InputsRead & (1 << VERT_ATTRIB_NORMAL))
		vic_1 |= R300_INPUT_CNTL_NORMAL;

	if (InputsRead & (1 << VERT_ATTRIB_COLOR0))
		vic_1 |= R300_INPUT_CNTL_COLOR;

	rmesa->state.texture.tc_count = 0;
	for (i = 0; i < ctx->Const.MaxTextureUnits; i++)
		if (InputsRead & (1 << (VERT_ATTRIB_TEX0 + i))) {
			rmesa->state.texture.tc_count++;
			vic_1 |= R300_INPUT_CNTL_TC0 << i;
		}

	return vic_1;
}

GLuint r300VAPOutputCntl0(GLcontext * ctx, GLuint OutputsWritten)
{
	GLuint ret = 0;

	if (OutputsWritten & (1 << VERT_RESULT_HPOS))
		ret |= R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT;

	if (OutputsWritten & (1 << VERT_RESULT_COL0))
		ret |= R300_VAP_OUTPUT_VTX_FMT_0__COLOR_0_PRESENT;

	if (OutputsWritten & (1 << VERT_RESULT_COL1))
		ret |= R300_VAP_OUTPUT_VTX_FMT_0__COLOR_1_PRESENT;

	if (OutputsWritten & (1 << VERT_RESULT_BFC0)
	    || OutputsWritten & (1 << VERT_RESULT_BFC1))
		ret |=
		    R300_VAP_OUTPUT_VTX_FMT_0__COLOR_1_PRESENT |
		    R300_VAP_OUTPUT_VTX_FMT_0__COLOR_2_PRESENT |
		    R300_VAP_OUTPUT_VTX_FMT_0__COLOR_3_PRESENT;

#if 0
	if (OutputsWritten & (1 << VERT_RESULT_FOGC)) ;
#endif

	if (OutputsWritten & (1 << VERT_RESULT_PSIZ))
		ret |= R300_VAP_OUTPUT_VTX_FMT_0__PT_SIZE_PRESENT;

	return ret;
}

GLuint r300VAPOutputCntl1(GLcontext * ctx, GLuint OutputsWritten)
{
	GLuint i, ret = 0;

	for (i = 0; i < ctx->Const.MaxTextureUnits; i++) {
		if (OutputsWritten & (1 << (VERT_RESULT_TEX0 + i))) {
			ret |= (4 << (3 * i));
		}
	}

	return ret;
}

/* Emit vertex data to GART memory
 * Route inputs to the vertex processor
 * This function should never return R300_FALLBACK_TCL when using software tcl.
 */
int r300EmitArrays(GLcontext * ctx)
{
	r300ContextPtr rmesa = R300_CONTEXT(ctx);
	TNLcontext *tnl = TNL_CONTEXT(ctx);
	struct vertex_buffer *vb = &tnl->vb;
	GLuint nr;
	GLuint count = vb->Count;
	GLuint i;
	GLuint InputsRead = 0, OutputsWritten = 0;
	int *inputs = NULL;
	int vir_inputs[VERT_ATTRIB_MAX];
	GLint tab[VERT_ATTRIB_MAX];
	int swizzle[VERT_ATTRIB_MAX][4];
	struct r300_vertex_program *prog =
	    (struct r300_vertex_program *)CURRENT_VERTEX_SHADER(ctx);

	if (hw_tcl_on) {
		inputs = prog->inputs;
		InputsRead = prog->key.InputsRead;
		OutputsWritten = prog->key.OutputsWritten;
	} else {
		inputs = rmesa->state.sw_tcl_inputs;

		DECLARE_RENDERINPUTS(render_inputs_bitset);
		RENDERINPUTS_COPY(render_inputs_bitset, tnl->render_inputs_bitset);

		vb->AttribPtr[VERT_ATTRIB_POS] = vb->ClipPtr;

		assert(RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_POS));
		assert(RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_NORMAL) == 0);
		//assert(RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_COLOR0));

		if (RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_POS)) {
			InputsRead |= 1 << VERT_ATTRIB_POS;
			OutputsWritten |= 1 << VERT_RESULT_HPOS;
		}

		if (RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_COLOR0)) {
			InputsRead |= 1 << VERT_ATTRIB_COLOR0;
			OutputsWritten |= 1 << VERT_RESULT_COL0;
		}

		if (RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_COLOR1)) {
			InputsRead |= 1 << VERT_ATTRIB_COLOR1;
			OutputsWritten |= 1 << VERT_RESULT_COL1;
		}

		for (i = 0; i < ctx->Const.MaxTextureUnits; i++) {
			if (RENDERINPUTS_TEST(render_inputs_bitset, _TNL_ATTRIB_TEX(i))) {
				InputsRead |= 1 << (VERT_ATTRIB_TEX0 + i);
				OutputsWritten |= 1 << (VERT_RESULT_TEX0 + i);
			}
		}

		for (i = 0, nr = 0; i < VERT_ATTRIB_MAX; i++) {
			if (InputsRead & (1 << i)) {
				inputs[i] = nr++;
			} else {
				inputs[i] = -1;
			}
		}

		/* Fixed, apply to vir0 only */
		memcpy(vir_inputs, inputs, VERT_ATTRIB_MAX * sizeof(int));
		inputs = vir_inputs;
		if (InputsRead & VERT_ATTRIB_POS)
			inputs[VERT_ATTRIB_POS] = 0;
		if (InputsRead & (1 << VERT_ATTRIB_COLOR0))
			inputs[VERT_ATTRIB_COLOR0] = 2;
		if (InputsRead & (1 << VERT_ATTRIB_COLOR1))
			inputs[VERT_ATTRIB_COLOR1] = 3;
		for (i = VERT_ATTRIB_TEX0; i <= VERT_ATTRIB_TEX7; i++)
			if (InputsRead & (1 << i))
				inputs[i] = 6 + (i - VERT_ATTRIB_TEX0);

		RENDERINPUTS_COPY(rmesa->state.render_inputs_bitset, render_inputs_bitset);
	}

	assert(InputsRead);
	assert(OutputsWritten);

	for (i = 0, nr = 0; i < VERT_ATTRIB_MAX; i++) {
		if (InputsRead & (1 << i)) {
			tab[nr++] = i;
		}
	}

	if (nr > R300_MAX_AOS_ARRAYS) {
		return R300_FALLBACK_TCL;
	}

	for (i = 0; i < nr; i++) {
		int ci;

		swizzle[i][0] = SWIZZLE_ZERO;
		swizzle[i][1] = SWIZZLE_ZERO;
		swizzle[i][2] = SWIZZLE_ZERO;
		swizzle[i][3] = SWIZZLE_ONE;

		for (ci = 0; ci < vb->AttribPtr[tab[i]]->size; ci++) {
			swizzle[i][ci] = ci;
		}

		r300EmitVec(ctx, &rmesa->state.aos[i],
				vb->AttribPtr[tab[i]]->data,
				vb->AttribPtr[tab[i]]->size,
				vb->AttribPtr[tab[i]]->stride, count);
	}

	/* Setup INPUT_ROUTE. */
	R300_STATECHANGE(rmesa, vir[0]);
	((drm_r300_cmd_header_t *) rmesa->hw.vir[0].cmd)->packet0.count =
	    r300VAPInputRoute0(&rmesa->hw.vir[0].cmd[R300_VIR_CNTL_0],
			       vb->AttribPtr, inputs, tab, nr);
	R300_STATECHANGE(rmesa, vir[1]);
	((drm_r300_cmd_header_t *) rmesa->hw.vir[1].cmd)->packet0.count =
	    r300VAPInputRoute1(&rmesa->hw.vir[1].cmd[R300_VIR_CNTL_0], swizzle,
			       nr);

	/* Setup INPUT_CNTL. */
	R300_STATECHANGE(rmesa, vic);
	rmesa->hw.vic.cmd[R300_VIC_CNTL_0] = r300VAPInputCntl0(ctx, InputsRead);
	rmesa->hw.vic.cmd[R300_VIC_CNTL_1] = r300VAPInputCntl1(ctx, InputsRead);

	/* Setup OUTPUT_VTX_FMT. */
	R300_STATECHANGE(rmesa, vof);
	rmesa->hw.vof.cmd[R300_VOF_CNTL_0] =
	    r300VAPOutputCntl0(ctx, OutputsWritten);
	rmesa->hw.vof.cmd[R300_VOF_CNTL_1] =
	    r300VAPOutputCntl1(ctx, OutputsWritten);

	rmesa->state.aos_count = nr;

	return R300_FALLBACK_NONE;
}

void r300ReleaseArrays(GLcontext * ctx)
{
	r300ContextPtr rmesa = R300_CONTEXT(ctx);
	int i;

	if (rmesa->state.elt_dma_bo) {
		dri_bo_unreference(rmesa->state.elt_dma_bo);
		rmesa->state.elt_dma_bo = 0;
	}
	for (i = 0; i < rmesa->state.aos_count; i++) {
		if (rmesa->state.aos[i].bo) {
			dri_bo_unreference(rmesa->state.aos[i].bo);
			rmesa->state.aos[i].bo = 0;
		}
	}
}

void r300EmitCacheFlush(r300ContextPtr rmesa)
{
	BATCH_LOCALS(rmesa);

	BEGIN_BATCH(4);
	OUT_BATCH_REGVAL(R300_RB3D_DSTCACHE_CTLSTAT,
		R300_RB3D_DSTCACHE_CTLSTAT_DC_FREE_FREE_3D_TAGS |
		R300_RB3D_DSTCACHE_CTLSTAT_DC_FLUSH_FLUSH_DIRTY_3D);
	OUT_BATCH_REGVAL(R300_ZB_ZCACHE_CTLSTAT,
		R300_ZB_ZCACHE_CTLSTAT_ZC_FLUSH_FLUSH_AND_FREE |
		R300_ZB_ZCACHE_CTLSTAT_ZC_FREE_FREE);
	END_BATCH();
	COMMIT_BATCH();
}

void r300EmitBlit(r300ContextPtr rmesa,
		  GLuint color_fmt,
		  GLuint src_pitch,
		  dri_bo *src_bo, int src_offset,
		  GLuint dst_pitch,
		  GLuint dst_offset,
		  GLint srcx, GLint srcy,
		  GLint dstx, GLint dsty, GLuint w, GLuint h)
{
	BATCH_LOCALS(rmesa);

	if (RADEON_DEBUG & DEBUG_IOCTL)
		fprintf(stderr,
			"%s src %x/%x %d,%d dst: %x/%x %d,%d sz: %dx%d\n",
			__FUNCTION__, src_pitch, src_offset, srcx, srcy,
			dst_pitch, dst_offset, dstx, dsty, w, h);

	assert((src_pitch & 63) == 0);
	assert((dst_pitch & 63) == 0);
	assert((src_offset & 1023) == 0);
	assert((dst_offset & 1023) == 0);
	assert(w < (1 << 16));
	assert(h < (1 << 16));

	BEGIN_BATCH(8);
	OUT_BATCH_PACKET3(R300_CP_CMD_BITBLT_MULTI, 5);
	OUT_BATCH(RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
		  RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		  RADEON_GMC_BRUSH_NONE |
		  (color_fmt << 8) |
		  RADEON_GMC_SRC_DATATYPE_COLOR |
		  RADEON_ROP3_S |
		  RADEON_DP_SRC_SOURCE_MEMORY |
		  RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);
	OUT_BATCH_RELOC((src_pitch / 64) << 22, src_bo, src_offset, DRM_RELOC_BLITTER);
	OUT_BATCH(((dst_pitch / 64) << 22) | (dst_offset >> 10));
	OUT_BATCH((srcx << 16) | srcy);
	OUT_BATCH((dstx << 16) | dsty);
	OUT_BATCH((w << 16) | h);
	END_BATCH();
}
