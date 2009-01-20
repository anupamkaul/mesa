/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Tx. , USA
 * All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include <math.h>

#include "main/image.h"
#include "main/state.h"
#include "swrast/swrast.h"

#include "via_context.h"
#include "via_3d_reg.h"
#include "via_ioctl.h"
#include "via_buffer_objects.h"
#include "via_tris.h"
#include "via_state.h"
#include "via_pixel.h"

#include "wsbm_manager.h"

static int
logbase2(int n)
{
    GLint i = 1;
    GLint log2 = 0;

    while (n > i) {
	i *= 2;
	log2++;
    }

    return log2;
}

void
via_meta_install_dst_bufobj(struct via_context *vmesa,
			    uint32_t dst_format,
			    uint32_t dst_pitch,
			    uint32_t dst_height,
			    uint32_t dst_offset,
			    uint32_t dst_xoff,
			    struct drm_via_clip_rect *dst_clip,
			    struct _WsbmBufferObject *dst_buf)
{
    struct via_meta_state *meta = &vmesa->meta;

    VIA_FLUSH_DMA(vmesa);

    meta->dst_format = dst_format;
    meta->dst_pitch = dst_pitch;
    meta->dst_clip = *dst_clip;
    meta->dst_buf = wsbmBOReference(dst_buf);
    meta->dst_offset = dst_offset;
    meta->xoff = (float)dst_xoff;
    meta->flip_dst = GL_FALSE;
    meta->dst_height = (float)dst_height;
    vmesa->meta_flags |= VIA_META_DST_ENABLE;
    meta->regEnable = (HC_HenCW_MASK | HC_HenTXMP_MASK |
		       HC_HenTXCH_MASK | HC_HenAW_MASK);
    meta->saveCmdB = vmesa->regCmdB;
}

void
via_meta_install_dst_fb(struct via_context *vmesa)
{
    struct via_meta_state *meta = &vmesa->meta;
    struct gl_framebuffer *fb = &viaDrawFrameBuffer(vmesa)->Base;

    if (vmesa->firstDrawAfterSwap) {
	LOCK_HARDWARE(vmesa);
	viaValidateDrawablesLocked(vmesa);
	UNLOCK_HARDWARE(vmesa);
	vmesa->firstDrawAfterSwap = 0;
    }

    meta->xoff = (float)vmesa->drawXoff;
    meta->dst_height = (float)fb->Height;
    meta->flip_dst = (fb->Name == 0);
    meta->regEnable = vmesa->regEnable;
    meta->regEnable &= ~(HC_HenTXEnvMap_MASK |
			 HC_HenFBCull_MASK |
			 HC_HenAA_MASK | HC_HenTXPP_MASK | HC_HenTXTR_MASK);
    meta->regEnable |= (HC_HenCW_MASK |
			HC_HenTXMP_MASK | HC_HenTXCH_MASK | HC_HenAW_MASK);
    meta->saveCmdB = vmesa->regCmdB;
    vmesa->meta_flags &= ~VIA_META_DST_ENABLE;
}

void
via_meta_install_src(struct via_context *vmesa,
		     uint32_t scale_rgba, uint32_t bias_rgba,
		     uint32_t srcFmt, float scaleX, float scaleY,
		     uint32_t width, uint32_t height,
		     uint32_t stride, uint32_t src_offset,
		     GLboolean flip_src, struct _WsbmBufferObject *buf)
{
    struct via_meta_state *meta = &vmesa->meta;
    uint32_t log2Tmp;
    int linearS = ((fabs(scaleX - 1.f)) > 0.001f);
    int linearT = ((fabs(scaleY - 1.f)) > 0.001f);

    /*
     * Set up the following texture blending equation:
     *
     * Ca = scale_rgb
     * Cb = (Rtex, Btex, Ctex)
     * Cc = (0, 0, 0)
     * Cbias = bias_rgb
     * Cshift = 0
     *
     * Aa = scale_a
     * Ab = Atex
     * Ac = 0
     * Abias = bias_a
     * Ashift = 0
     */

    meta->regCsat = (HC_SubA_HTXnTBLCsat << 24) |
	HC_HTXnTBLCsat_MASK |
	HC_HTXnTBLCa_TOPC |
	HC_HTXnTBLCa_HTXnTBLRC |
	HC_HTXnTBLCb_TOPC |
	HC_HTXnTBLCb_Tex | HC_HTXnTBLCc_TOPC | HC_HTXnTBLCc_0;
    meta->regCop = (HC_SubA_HTXnTBLCop << 24) |
	HC_HTXnTBLCop_Add |
	HC_HTXnTBLCbias_Cbias |
	HC_HTXnTBLCbias_HTXnTBLRC |
	HC_HTXnTBLCshift_No |
	HC_HTXnTBLAop_Add |
	HC_HTXnTBLAbias_HTXnTBLRAbias | HC_HTXnTBLAshift_No;
    meta->regAsat = (HC_SubA_HTXnTBLAsat << 24) |
	HC_HTXnTBLAsat_MASK |
	HC_HTXnTBLAa_TOPA |
	HC_HTXnTBLAa_HTXnTBLRA |
	HC_HTXnTBLAb_TOPA |
	HC_HTXnTBLAb_Atex | HC_HTXnTBLAc_TOPA | HC_HTXnTBLAc_HTXnTBLRA;
    meta->regCa = (HC_SubA_HTXnTBLRCa << 24) | (scale_rgba >> 8);
    meta->regCbias = (HC_SubA_HTXnTBLRCbias << 24) | (bias_rgba >> 8);
    meta->regAa = (HC_SubA_HTXnTBLRAa << 24) | ((scale_rgba & 0xFF) << 16);
    meta->regAfog = (HC_SubA_HTXnTBLRFog << 24) | (bias_rgba & 0xFF);
    meta->regL0Pit = (HC_SubA_HTXnL0Pit << 24) |
	HC_HTXnEnPit_MASK | (stride & HC_HTXnLnPit_MASK);
    meta->regL0Os = (HC_SubA_HTXnL0OS << 24) | 0;
    meta->regTB = (HC_SubA_HTXnTB << 24) |
	HC_HTXnTB_NoTB |
	((linearS) ? (HC_HTXnFLSe_Linear | HC_HTXnFLSs_Linear) :
	 (HC_HTXnFLSe_Nearest | HC_HTXnFLSs_Nearest)) |
	((linearT) ? (HC_HTXnFLTe_Linear | HC_HTXnFLTs_Linear) :
	 (HC_HTXnFLTe_Nearest | HC_HTXnFLTs_Nearest)) | HC_HTXnFLDs_Tex0;
    meta->regMPMD = (HC_SubA_HTXnMPMD << 24) |
	HC_HTXnMPMD_Tsingle | HC_HTXnMPMD_Ssingle;
    meta->regCLODu = (HC_SubA_HTXnCLODu << 24);
    meta->regTexFM = (HC_SubA_HTXnFM << 24) | srcFmt;

    log2Tmp = logbase2(width);
    meta->texWidth = 1 << log2Tmp;
    meta->regL0_5WE = (HC_SubA_HTXnL0_5WE << 24) |
	(log2Tmp & HC_HTXnL0WE_MASK);

    log2Tmp = logbase2(height);
    meta->texHeight = 1 << log2Tmp;
    meta->regL0_5HE = (HC_SubA_HTXnL0_5HE << 24) |
	(log2Tmp & HC_HTXnL0HE_MASK);

    meta->maxS = (float)width / (float)meta->texWidth;
    meta->maxT = (float)height / (float)meta->texHeight;

    meta->xToS = meta->maxS / (float)width;
    meta->yToT = meta->maxT / (float)height;
    meta->sToX = (float)width *scaleX / meta->maxS;
    meta->tToY = (float)height *scaleY / meta->maxT;

    meta->src_buf = wsbmBOReference(buf);
    meta->src_offset = src_offset;
    meta->flip_src = flip_src;

    vmesa->meta_flags = VIA_META_SRC_ENABLE;
    vmesa->newState |= _NEW_TEXTURE;
    viaValidateState(vmesa->glCtx);

}

void
via_meta_uninstall(struct via_context *vmesa)
{
    struct via_meta_state *meta = &vmesa->meta;

    /*
     * Must flush before we clear meta_flags if
     * we're rendering to another dest buffer.
     */

    if (vmesa->meta_flags & VIA_META_DST_ENABLE)
	VIA_FLUSH_DMA(vmesa);

    wsbmBOUnreference(&meta->src_buf);
    meta->src_buf = NULL;

    wsbmBOUnreference(&meta->dst_buf);
    meta->dst_buf = NULL;

    vmesa->meta_flags = 0;
    vmesa->newState = _NEW_TEXTURE;
    vmesa->regCmdB = meta->saveCmdB;
    viaValidateState(vmesa->glCtx);
}

uint32_t
via_src_fmt_to_hw(GLenum format, GLenum type, uint32_t * cpp)
{
    *cpp = 2;

    switch (format) {
    case GL_BGRA:
	switch (type) {
	case GL_UNSIGNED_INT_8_8_8_8:
	    *cpp = 4;
	    return HC_HTXnFM_BGRA8888;
	case GL_UNSIGNED_SHORT_5_5_5_1:
	    return HC_HTXnFM_BGRA5551;
	case GL_UNSIGNED_SHORT_4_4_4_4:
	    return HC_HTXnFM_BGRA4444;
	case GL_UNSIGNED_INT_8_8_8_8_REV:
	case GL_UNSIGNED_BYTE:
	    *cpp = 4;
	    return HC_HTXnFM_ARGB8888;
	case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	    return HC_HTXnFM_ARGB1555;
	case GL_UNSIGNED_SHORT_4_4_4_4_REV:
	    return HC_HTXnFM_ARGB4444;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_RGBA:
	switch (type) {
	case GL_UNSIGNED_INT_8_8_8_8:
	    *cpp = 4;
	    return HC_HTXnFM_RGBA8888;
	case GL_UNSIGNED_SHORT_5_5_5_1:
	    return HC_HTXnFM_RGBA5551;
	case GL_UNSIGNED_SHORT_4_4_4_4:
	    return HC_HTXnFM_RGBA4444;
	case GL_UNSIGNED_INT_8_8_8_8_REV:
	case GL_UNSIGNED_BYTE:
	    *cpp = 4;
	    return HC_HTXnFM_ABGR8888;
	case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	    return HC_HTXnFM_ABGR1555;
	case GL_UNSIGNED_SHORT_4_4_4_4_REV:
	    return HC_HTXnFM_ABGR4444;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_RGB:
	switch (type) {
	case GL_UNSIGNED_SHORT_5_6_5:
	    return HC_HTXnFM_RGB565;
	case GL_UNSIGNED_SHORT_5_6_5_REV:
	    return HC_HTXnFM_BGR565;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_BGR:
	switch (type) {
	case GL_UNSIGNED_SHORT_5_6_5:
	    return HC_HTXnFM_BGR565;
	case GL_UNSIGNED_SHORT_5_6_5_REV:
	    return HC_HTXnFM_RGB565;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_ALPHA:
	if (type == GL_UNSIGNED_BYTE) {
	    *cpp = 1;
	    return HC_HTXnFM_A8;
	}
	return VIA_FMT_ERROR;
    case GL_LUMINANCE:
	if (type == GL_UNSIGNED_BYTE) {
	    *cpp = 1;
	    return HC_HTXnFM_L8;
	}
	return VIA_FMT_ERROR;
    case GL_LUMINANCE_ALPHA:
	if (type == GL_UNSIGNED_BYTE)
	    return HC_HTXnFM_AL88;
	return VIA_FMT_ERROR;
    default:
	return VIA_FMT_ERROR;
    }
    return VIA_FMT_ERROR;
}

uint32_t
via_dst_format_to_hw(GLenum format, GLenum type, uint32_t * cpp)
{
    *cpp = 2;

    switch (format) {
    case GL_BGRA:
	switch (type) {
	case GL_UNSIGNED_INT_8_8_8_8_REV:
	case GL_UNSIGNED_BYTE:
	    *cpp = 4;
	    return HC_HDBFM_ARGB8888;
	case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	    return HC_HDBFM_ARGB1555;
	case GL_UNSIGNED_SHORT_4_4_4_4_REV:
	    return HC_HDBFM_ARGB4444;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_RGBA:
	switch (type) {
	case GL_UNSIGNED_INT_8_8_8_8_REV:
	case GL_UNSIGNED_BYTE:
	    *cpp = 4;
	    return HC_HDBFM_ABGR8888;
	case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	    return HC_HDBFM_ABGR1555;
	case GL_UNSIGNED_SHORT_4_4_4_4_REV:
	    return HC_HDBFM_ABGR4444;
	default:
	    return VIA_FMT_ERROR;
	}
    case GL_RGB:
	switch (type) {
	case GL_UNSIGNED_SHORT_5_6_5:
	    return HC_HDBFM_RGB565;
	case GL_UNSIGNED_SHORT_5_6_5_REV:
	    return HC_HDBFM_BGR565;
	default:
	    return VIA_FMT_ERROR;
	}
    default:
	return VIA_FMT_ERROR;
    }
    return VIA_FMT_ERROR;
}

static inline uint32_t
via_fui(float f)
{
    union
    {
	uint32_t u;
	float f;
    } tmp;

    tmp.f = f;

    return tmp.u;
}

static uint32_t
via_meta_compute_fog(GLcontext * ctx)
{
    struct gl_fog_attrib *fog = &ctx->Fog;
    uint32_t tmp;
    GLfloat f;
    GLfloat d;
    GLfloat z = ctx->Current.RasterDistance;

    if (!fog->Enabled)
	return 0;

    switch (fog->Mode) {
    case GL_LINEAR:
	f = (fog->End - z) * fog->_Scale;
	if (f < 0.0F)
	    f = 0.0F;
	else if (f > 1.0F)
	    f = 1.0F;
	break;
    case GL_EXP:
	d = fog->Density;
	f = exp(-d * z);
	break;
    case GL_EXP2:
	d = fog->Density * fog->Density;
	f = exp(-d * z * z);
	break;
    default:
	_mesa_problem(ctx, "Bad fog mode in via_meta_compute_fog");
	return 0;
    }
    tmp = (uint32_t) (f * 255. + 0.5);
    tmp <<= 24;
    return tmp;
}

void
via_meta_emit_src_clip(struct via_context *vmesa,
		       int rx, int ry, int dx, int dy,
		       struct drm_via_clip_rect *clip, float z)
{
    struct via_meta_state *meta = &vmesa->meta;

    float frx = (float)rx;
    float fry = (float)ry;
    float sOrig = frx * meta->xToS;
    float tOrig = fry * meta->yToT;
    float s0 = ((float)clip->x1 + frx) * meta->xToS;
    float s1 = ((float)clip->x2 + frx) * meta->xToS;
    float t0 = ((float)clip->y1 + fry) * meta->yToT;
    float t1 = ((float)clip->y2 + fry) * meta->yToT;
    float x0 = (float)dx + (s0 - sOrig) * meta->sToX + meta->xoff;
    float x1 = (float)dx + (s1 - sOrig) * meta->sToX + meta->xoff;
    float y0 = (float)dy + (t0 - tOrig) * meta->tToY;
    float y1 = (float)dy + (t1 - tOrig) * meta->tToY;
    uint32_t fog = via_meta_compute_fog(vmesa->glCtx);
    uint32_t uiz = via_fui(z);
    uint32_t *vb;

    /*
     * Backface is clockwise?
     */

    VIA_FINISH_PRIM(vmesa);

    vmesa->regCmdB &= ~(HC_HBFace_MASK | HC_HVPMSK_MASK);
    vmesa->regCmdB |= HC_HVPMSK_X | HC_HVPMSK_Y | HC_HVPMSK_Z |
	HC_HVPMSK_Cs | HC_HVPMSK_S | HC_HVPMSK_T;

    viaRasterPrimitive(vmesa->glCtx, GL_TRIANGLE_STRIP, GL_TRIANGLE_STRIP);

    if (meta->flip_dst) {
	y0 = (meta->dst_height - y0);
	y1 = (meta->dst_height - y1);
    }

    if (meta->flip_src) {
	t0 = meta->maxT - t0;
	t1 = meta->maxT - t1;
    }

    vb = viaExtendPrimitive(vmesa, 6 * 4 * sizeof(uint32_t));

    *vb++ = via_fui(x0);
    *vb++ = via_fui(y1);
    *vb++ = uiz;
    *vb++ = fog;
    *vb++ = via_fui(s0);
    *vb++ = via_fui(t1);

    *vb++ = via_fui(x1);
    *vb++ = via_fui(y1);
    *vb++ = uiz;
    *vb++ = fog;
    *vb++ = via_fui(s1);
    *vb++ = via_fui(t1);

    *vb++ = via_fui(x0);
    *vb++ = via_fui(y0);
    *vb++ = uiz;
    *vb++ = fog;
    *vb++ = via_fui(s0);
    *vb++ = via_fui(t0);

    *vb++ = via_fui(x1);
    *vb++ = via_fui(y0);
    *vb++ = uiz;
    *vb++ = fog;
    *vb++ = via_fui(s1);
    *vb++ = via_fui(t0);

    VIA_FINISH_PRIM(vmesa);
}
