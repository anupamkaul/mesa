/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIACONTEXT_H
#define _VIACONTEXT_H

#include "dri_util.h"

#include "main/mtypes.h"
#include "tnl/t_vertex.h"

#include "via_screen.h"
#include "ochr_drm.h"
#include "via_fbo.h"
#include "via_ioctl.h"

struct via_context;

/* Chip tags.  These are used to group the adapters into
 * related families.
 */
enum VIACHIPTAGS
{
    VIA_UNKNOWN = 0,
    VIA_CLE266,
    VIA_KM400,
    VIA_K8M800,
    VIA_PM800,
    VIA_VM800,
    VIA_K8M890,
    VIA_P4M900,
    VIA_CX700,
    VIA_P4M890,
    VIA_LAST
};

#define VIA_FALLBACK_TEXTURE		0x1
#define VIA_FALLBACK_DRAW_BUFFER	0x2
#define VIA_FALLBACK_READ_BUFFER	0x4
#define VIA_FALLBACK_COLORMASK		0x8
#define VIA_FALLBACK_SPECULAR		0x20
#define VIA_FALLBACK_LOGICOP		0x40
#define VIA_FALLBACK_RENDERMODE		0x80
#define VIA_FALLBACK_STENCIL		0x100
#define VIA_FALLBACK_BLEND_EQ		0x200
#define VIA_FALLBACK_BLEND_FUNC		0x400
#define VIA_FALLBACK_USER_DISABLE	0x800
#define VIA_FALLBACK_PROJ_TEXTURE	0x1000
#define VIA_FALLBACK_POLY_STIPPLE	0x2000
#define VIA_FALLBACK_SW_BUFFER          0x4000

/* Use the templated vertex formats:
 */
#define TAG(x) via##x
#include "tnl_dd/t_dd_vertex.h"
#undef TAG

typedef void (*via_tri_func) (struct via_context *, viaVertex *, viaVertex *,
			      viaVertex *);
typedef void (*via_line_func) (struct via_context *, viaVertex *,
			       viaVertex *);
typedef void (*via_point_func) (struct via_context *, viaVertex *);

struct via_reloc_bufinfo;
struct via_reloc_savestate;

#define VIA_META_SRC_ENABLE (1 << 0)
#define VIA_META_DST_ENABLE (1 << 1)
#define VIA_META_OP_ENABLE  (1 << 2)

struct via_meta_state
{
    /*
     * Src data.
     */

    uint32_t regCsat;
    uint32_t regCop;
    uint32_t regAsat;
    uint32_t regCa;
    uint32_t regCbias;
    uint32_t regAa;
    uint32_t regAfog;
    uint32_t regEnable;
    uint32_t regL0Pit;
    uint32_t regL0_5WE;
    uint32_t regL0_5HE;
    uint32_t regL0Os;
    uint32_t regTB;
    uint32_t regMPMD;
    uint32_t regCLODu;
    uint32_t regTexFM;
    uint32_t saveCmdB;

    uint32_t texWidth;
    uint32_t texHeight;
    float maxS;
    float maxT;
    float sToX;
    float tToY;
    float xToS;
    float yToT;
    GLboolean flip_src;
    struct _WsbmBufferObject *src_buf;
    uint32_t src_offset;

    /*
     * Dst data.
     */

    GLboolean flip_dst;
    float xoff;
    float dst_height;
    uint32_t dst_format;
    uint32_t dst_pitch;
    uint32_t dst_offset;
    struct drm_via_clip_rect dst_clip;
    struct _WsbmBufferObject *dst_buf;
};

#define VIA_CLIP_CACHE_SIZE 256

struct via_context
{
    GLcontext *glCtx;
    GLint refcount;
    GLcontext *shareCtx;

    GLfloat polygon_offset_scale;

    GLubyte *dma;
    GLubyte *lostStateDma;

    /* Bit flag to keep 0track of fallbacks.
     */
    GLuint Fallback;

    /* State for via_tris.c.
     */
    GLuint newState;		       /* _NEW_* flags */
    GLuint newEmitState;	       /* _NEW_* flags */
    GLuint newRenderState;	       /* _NEW_* flags */
    int lostState;
    int useLostState;

    GLboolean firstDrawAfterSwap;
    GLboolean lostLock;
    /*
     * IsLocked is an ugly temporary workaround
     * to make it safe to call VIA_FLUSH_DMA() from
     * within a locked region.
     */
    GLboolean isLocked;

    struct tnl_attr_map vertex_attrs[VERT_ATTRIB_MAX];
    GLuint vertex_attr_count;

    GLuint setupIndex;
    GLuint renderIndex;
    GLmatrix ViewportMatrix;
    GLenum renderPrimitive;
    GLenum hwPrimitive;
    GLenum hwShadeModel;
    unsigned char *verts;

    /* drmBufPtr dma_buffer;
     */
    GLuint dmaLow;
    GLuint lostStateDmaLow;

    GLuint dmaCliprectAddr;
    GLuint dmaLastPrim;
    GLboolean useAgp;

    /* Fallback rasterization functions
     */
    via_point_func drawPoint;
    via_line_func drawLine;
    via_tri_func drawTri;

    /* Command submission.
     */

    struct via_reloc_bufinfo *reloc_info;
    struct via_reloc_savestate *save_state;

    struct _WsbmBufferList *validate_list;

    /* Hardware register
     */
    GLuint regCmdA_End;
    GLuint regCmdB;

    GLuint regEnable;
    GLuint regHFBBMSKL;
    GLuint regHROP;

    GLuint regHZWTMD;
    GLuint regHSTREF;
    GLuint regHSTMD;

    GLuint regHATMD;
    GLuint regHABLCsat;
    GLuint regHABLCop;
    GLuint regHABLAsat;
    GLuint regHABLAop;
    GLuint regHABLRCa;
    GLuint regHABLRFCa;
    GLuint regHABLRCbias;
    GLuint regHABLRCb;
    GLuint regHABLRFCb;
    GLuint regHABLRAa;
    GLuint regHABLRAb;
    GLuint regHFogLF;
    GLuint regHFogCL;
    GLuint regHFogCH;

    GLuint regHLP;
    GLuint regHLPRF;

    GLuint regHTXnCLOD[2];
    GLuint regHTXnTB[2];
    GLuint regHTXnMPMD[2];
    GLuint regHTXnTBLCsat[2];
    GLuint regHTXnTBLCop[2];
    GLuint regHTXnTBLMPfog[2];
    GLuint regHTXnTBLAsat[2];
    GLuint regHTXnTBLRCb[2];
    GLuint regHTXnTBLRAa[2];
    GLuint regHTXnTBLRFog[2];
    GLuint regHTXnTBLRCa[2];
    GLuint regHTXnTBLRCc[2];
    GLuint regHTXnTBLRCbias[2];
    GLuint regHTXnTBC[2];
    GLuint regHTXnTRAH[2];

    int vertexSize;
    int hwVertexSize;
    GLboolean ptexHack;
    int coloroffset;
    int specoffset;

    GLint lastStamp;

    GLubyte ClearColor[4];
    GLuint ClearMask;

    /* DRI stuff
     */
    GLboolean doPageFlip;

    /*
     * Current Draw state.
     */

    struct via_renderbuffer *lastDrawBuf;
    struct via_renderbuffer *lastDepthBuf;
    GLuint drawAttIndex;
    GLuint drawXoff;
    GLuint drawXoffBytes;
    GLuint numDrawClipRects;
    drm_clip_rect_t *pDrawClipRects;
    GLuint drawStamp;

    /*
     * Current Read state.
     */

    struct via_renderbuffer *lastReadBuf;
    GLuint readAttIndex;
    GLuint readXoff;
    GLuint readXoffBytes;
    GLuint numReadClipRects;
    drm_clip_rect_t *pReadClipRects;
    GLuint readStamp;

    GLboolean scissor;
    struct drm_via_clip_rect scissorRect;

    drm_context_t hHWContext;
    drm_hw_lock_t *driHwLock;
    int driFd;
    int execIoctlOffset;

   /**
    * DRI drawable bound to this context for drawing.
    */
    __DRIdrawablePrivate *driDrawable;

   /**
    * DRI drawable bound to this context for reading.
    */
    __DRIdrawablePrivate *driReadable;

    __DRIscreenPrivate *driScreen;
    viaScreenPrivate *viaScreen;
    struct drm_via_sarea *sarea;
    GLuint drawType;

    GLuint nDoneFirstFlip;
    GLuint agpFullCount;

    GLboolean clearTexCache;

    GLuint vblank_flags;
    GLuint vbl_seq;

    int64_t swap_ust;
    int64_t swap_missed_ust;

    GLuint swap_count;
    GLuint swap_missed_count;

    GLuint pfCurrentOffset;
    GLboolean allowPageFlip;

    struct _WsbmFenceObject *last_fence;

    uint32_t meta_flags;
    struct via_meta_state meta;

    struct drm_via_clip_rect clip_cache[VIA_CLIP_CACHE_SIZE];

    GLuint drawSyncFlags;
    GLuint depthSyncFlags;
    GLboolean spanError;

};

#define VIA_CONTEXT(ctx)   ((struct via_context *)(ctx->DriverCtx))

/* Lock the hardware and validate our state.
 */
#define LOCK_HARDWARE(vmesa)					\
	do {                                                    \
	    char __ret = 0;                                     \
	    assert(!vmesa->isLocked);				\
	    DRM_CAS(vmesa->driHwLock, vmesa->hHWContext,        \
		(DRM_LOCK_HELD|vmesa->hHWContext), __ret);      \
	    if (__ret)                                          \
		viaGetLock(vmesa, 0);                           \
	    vmesa->isLocked = GL_TRUE;				\
	} while (0);

/* Release the kernel lock.
 */
#define UNLOCK_HARDWARE(vmesa)						\
    do {								\
	assert(vmesa->isLocked);					\
	vmesa->isLocked = GL_FALSE;					\
	DRM_UNLOCK(vmesa->driFd, vmesa->driHwLock, vmesa->hHWContext);	\
    } while (0);

extern GLuint VIA_DEBUG;

#define DEBUG_TEXTURE	0x1
#define DEBUG_STATE	0x2
#define DEBUG_IOCTL	0x4
#define DEBUG_PRIMS	0x8
#define DEBUG_VERTS	0x10
#define DEBUG_FALLBACKS	0x20
#define DEBUG_VERBOSE	0x40
#define DEBUG_DRI       0x80
#define DEBUG_DMA       0x100
#define DEBUG_SANITY    0x200
#define DEBUG_SYNC      0x400
#define DEBUG_SLEEP     0x800
#define DEBUG_PIXEL     0x1000
#define DEBUG_2D        0x2000
#define DEBUG_FBO       0x4000

extern void viaGetLock(struct via_context *vmesa, GLuint flags);
extern void viaLock(struct via_context *vmesa, GLuint flags);
extern void viaUnLock(struct via_context *vmesa, GLuint flags);
extern void viaEmitHwStateLocked(struct via_context *vmesa);
extern void viaEmitScissorValues(struct via_context *vmesa, int box_nr,
				 int emit);
extern void viaXMesaSetBackClipRects(struct via_context *vmesa);
extern void viaXMesaSetFrontClipRects(struct via_context *vmesa);
extern void viaReAllocateBuffers(GLcontext * ctx, GLframebuffer * drawbuffer,
				 GLuint width, GLuint height);
extern void viaXMesaWindowMoved(struct via_context *vmesa);

extern GLboolean viaTexCombineState(struct via_context *vmesa,
				    const struct gl_tex_env_combine_state
				    *combine, unsigned unit);

static __inline__ GLuint *
viaAllocDma(struct via_context *vmesa, int bytes)
{
    if (vmesa->dmaLow + bytes > VIA_DMA_HIGHWATER) {
	viaFlushDma(vmesa);
    }

    {
	GLuint *start = (GLuint *) (vmesa->dma + vmesa->dmaLow);

	vmesa->dmaLow += bytes;
	return start;
    }
}

static __inline__ GLuint *
viaAllocLostStateDma(struct via_context *vmesa, int bytes)
{
    assert(vmesa->lostStateDmaLow + bytes <= VIA_LOSTSTATEDMA_HIGHWATER);
    {
	GLuint *start =
	    (GLuint *) (vmesa->lostStateDma + vmesa->lostStateDmaLow);

	vmesa->lostStateDmaLow += bytes;
	return start;
    }
}

static GLuint __inline__ *
viaExtendPrimitive(struct via_context *vmesa, int bytes)
{
    if (vmesa->dmaLow + bytes > VIA_DMA_HIGHWATER) {
	viaWrapPrimitive(vmesa);
    }

    {
	GLuint *start = (GLuint *) (vmesa->dma + vmesa->dmaLow);

	vmesa->dmaLow += bytes;
	return start;
    }
}

static inline struct via_framebuffer *
viaDrawFrameBuffer(struct via_context *vmesa)
{
    if (!vmesa->glCtx->DrawBuffer)
	return NULL;

    return containerOf(vmesa->glCtx->DrawBuffer,
		       struct via_framebuffer, Base);
}

static inline struct via_framebuffer *
viaReadFrameBuffer(struct via_context *vmesa)
{
    if (!vmesa->glCtx->ReadBuffer)
	return NULL;

    return containerOf(vmesa->glCtx->ReadBuffer,
		       struct via_framebuffer, Base);
}

static inline struct via_renderbuffer *
viaDrawRenderBuffer(struct via_context *vmesa)
{
    struct gl_framebuffer *fb = vmesa->glCtx->DrawBuffer;

    if (!fb)
	return NULL;

    return via_renderbuffer(fb->Attachment[vmesa->drawAttIndex].Renderbuffer);
}

static inline struct via_renderbuffer *
viaDrawDepthRenderBuffer(struct via_context *vmesa)
{
    struct gl_framebuffer *fb = vmesa->glCtx->DrawBuffer;

    if (!fb)
	return NULL;

    return via_renderbuffer(fb->Attachment[BUFFER_DEPTH].Renderbuffer);
}

static inline struct via_renderbuffer *
viaReadRenderBuffer(struct via_context *vmesa)
{
    struct gl_framebuffer *fb = vmesa->glCtx->ReadBuffer;

    if (!fb)
	return NULL;

    return via_renderbuffer(fb->Attachment[vmesa->readAttIndex].Renderbuffer);
}

static inline struct via_renderbuffer *
viaReadDepthRenderBuffer(struct via_context *vmesa)
{
    struct gl_framebuffer *fb = vmesa->glCtx->ReadBuffer;

    if (!fb)
	return NULL;

    return via_renderbuffer(fb->Attachment[BUFFER_DEPTH].Renderbuffer);
}

extern void via_wait_context_idle(struct via_context *vmesa);
extern void viaValidateDrawablesLocked(struct via_context *vmesa);
extern void viaUpdateFrameBufferLocked(struct via_context *vmesa,
				       __DRIdrawablePrivate * dPriv,
				       __DRIdrawablePrivate * rPriv);

/*
 * Meta state.
 */

extern void via_meta_install_dst_bufobj(struct via_context *vmesa,
					uint32_t dst_format,
					uint32_t dst_pitch,
					uint32_t dst_height,
					uint32_t dst_offset,
					uint32_t dst_xoff,
					struct drm_via_clip_rect *dst_clip,
					struct _WsbmBufferObject *dst_buf);
extern void via_meta_install_dst_fb(struct via_context *vmesa);
extern void via_meta_install_src(struct via_context *vmesa,
				 uint32_t scale_rgba, uint32_t bias_rgba,
				 uint32_t srcFmt, float scaleX, float scaleY,
				 uint32_t width, uint32_t height,
				 uint32_t stride, uint32_t src_offset,
				 GLboolean flip_src,
				 struct _WsbmBufferObject *buf);
extern void via_meta_uninstall(struct via_context *vmesa);
extern uint32_t via_src_fmt_to_hw(GLenum format, GLenum type, uint32_t * cpp);
extern uint32_t via_dst_format_to_hw(GLenum format, GLenum type,
				     uint32_t * cpp);
extern void via_meta_emit_src_clip(struct via_context *vmesa, int rx, int ry,
				   int dx, int dy,
				   struct drm_via_clip_rect *clip, float z);

/* Via hw already adjusted for GL pixel centers:
 */
#define SUBPIXEL_X 0
#define SUBPIXEL_Y 0

/* TODO XXX _SOLO temp defines to make code compilable */
#ifndef GLX_PBUFFER_BIT
#define GLX_PBUFFER_BIT        0x00000004
#endif
#ifndef GLX_WINDOW_BIT
#define GLX_WINDOW_BIT 0x00000001
#endif
#ifndef VERT_BIT_CLIP
#define VERT_BIT_CLIP       0x1000000
#endif

#endif
