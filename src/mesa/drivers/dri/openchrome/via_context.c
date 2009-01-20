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

/**
 * \file via_context.c
 *
 * \author John Sheng (presumably of either VIA Technologies or S3 Graphics)
 * \author Others at VIA Technologies?
 * \author Others at S3 Graphics?
 */

#include "main/glheader.h"
#include "main/context.h"
#include "main/matrix.h"
#include "main/state.h"
#include "main/extensions.h"
#include "main/framebuffer.h"
#include "main/renderbuffer.h"
#include "main/macros.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "tnl/tnl.h"
#include "vbo/vbo.h"

#include "tnl/t_pipeline.h"

#include "drivers/common/driverfuncs.h"

#include "via_screen.h"
#include "via_dri.h"

#include "via_state.h"
#include "via_tex.h"
#include "via_span.h"
#include "via_tris.h"
#include "via_ioctl.h"
#include "via_buffer_objects.h"
#include "via_pixel.h"

#include <stdio.h>
#include "drirenderbuffer.h"
#include "wsbm_fencemgr.h"
#include "wsbm_manager.h"

#define need_GL_ARB_texture_compression
#define need_GL_ARB_multisample
#define need_GL_ARB_point_parameters
#define need_GL_ARB_vertex_buffer_object
#define need_GL_EXT_fog_coord
#define need_GL_EXT_framebuffer_object
#define need_GL_EXT_secondary_color
#include "extension_helper.h"

#define DRIVER_DATE	"20080822"

#include "vblank.h"
#include "utils.h"

GLuint VIA_DEBUG = 0;

static GLboolean
via_alloc_dma_buffer(struct via_context *vmesa)
{
#if 0
    drm_via_dma_init_t init;
#endif

    vmesa->dma = (GLubyte *) malloc(VIA_DMA_BUFSIZ);
    if (!vmesa->dma)
	return GL_FALSE;

    vmesa->lostStateDma = (GLubyte *) malloc(VIA_LOSTSTATEDMA_BUFSIZ);
    if (!vmesa->lostStateDma) {
	free(vmesa->dma);
	return GL_FALSE;
    }

    /*
     * Check whether AGP DMA has been initialized.
     */
#if 1
    vmesa->useAgp = 1;
#else
    memset(&init, 0, sizeof(init));
    init.func = VIA_DMA_INITIALIZED;

    vmesa->useAgp =
	(0 == drmCommandWrite(vmesa->driFd, DRM_VIA_DMA_INIT,
			      &init, sizeof(init)));
    if (VIA_DEBUG & DEBUG_DMA) {
	if (vmesa->useAgp)
	    fprintf(stderr, "unichrome_dri.so: Using AGP.\n");
	else
	    fprintf(stderr, "unichrome_dri.so: Using PCI.\n");
    }
#endif
    return ((vmesa->dma) ? GL_TRUE : GL_FALSE);
}

static void
via_free_dma_buffer(struct via_context *vmesa)
{
    if (!vmesa)
	return;
    free(vmesa->dma);
    vmesa->dma = 0;
}

/**
 * Return various strings for \c glGetString.
 *
 * \sa glGetString
 */
static const GLubyte *
viaGetString(GLcontext * ctx, GLenum name)
{
    static char buffer[128];
    unsigned offset;

    switch (name) {
    case GL_VENDOR:
	return (GLubyte *) "Tungsten Graphics Inc.";

    case GL_RENDERER:{
	    static const char *const chipset_names[] = {
		"UniChrome (Unknown model)",
		"Unichrome (CLE266)",
		"UniChrome (KM400)",
		"UniChrome Pro (K8M800)",
		"UniChrome Pro (PM8x0/CN400)",
		"UniChrome Pro (VM800)",
		"UniChrome Pro (K8M890)",
		"UniChrome Pro (P4M900)",
		"UniChrome Pro II (CX700)",
		"Unichrome Pro (P4M890)"
	    };
	    struct via_context *vmesa = VIA_CONTEXT(ctx);
	    unsigned id = vmesa->viaScreen->deviceID;

	    offset = driGetRendererString(buffer,
					  chipset_names[(id > 8) ? 0 : id],
					  DRIVER_DATE, 0);
	    return (GLubyte *) buffer;
	}

    default:
	return NULL;
    }
}

/**
 * Calculate a width that satisfies the hardware's alignment requirements.
 * On the Unichrome hardware, each scanline must be aligned to a multiple of
 * 16 pixels.
 *
 * \param width  Minimum buffer width, in pixels.
 *
 * \returns A pixel width that meets the alignment requirements.
 */
static __inline__ unsigned
buffer_align(unsigned width)
{
    return (width + 0x0f) & ~0x0f;
}

void
viaReAllocateBuffers(GLcontext * ctx, GLframebuffer * drawbuffer,
		     GLuint width, GLuint height)
{
    if (ctx != NULL) {
	VIA_FLUSH_DMA(VIA_CONTEXT(ctx));
    }
    _mesa_resize_framebuffer(ctx, drawbuffer, width, height);
}

/* Extension strings exported by the Unichrome driver.
 */
const struct dri_extension card_extensions[] = {
    {"GL_ARB_multisample", GL_ARB_multisample_functions},
    {"GL_ARB_multitexture", NULL},
    {"GL_ARB_point_parameters", GL_ARB_point_parameters_functions},
    {"GL_ARB_texture_border_clamp", NULL},
    {"GL_ARB_texture_compression", GL_ARB_texture_compression_functions},
    {"GL_ARB_texture_env_add", NULL},
    {"GL_ARB_texture_env_combine", NULL},
    {"GL_ARB_texture_env_dot3", NULL},
    {"GL_ARB_texture_mirrored_repeat", NULL},
    {"GL_ARB_vertex_buffer_object", GL_ARB_vertex_buffer_object_functions},
    {"GL_ARB_pixel_buffer_object", NULL},
    {"GL_EXT_fog_coord", GL_EXT_fog_coord_functions},
    {"GL_EXT_framebuffer_object", GL_EXT_framebuffer_object_functions},
    {"GL_EXT_secondary_color", GL_EXT_secondary_color_functions},
    {"GL_EXT_stencil_wrap", NULL},
    {"GL_EXT_texture_compression_s3tc", NULL},
    {"GL_EXT_texture_env_combine", NULL},
    {"GL_EXT_texture_lod_bias", NULL},
    {"GL_NV_blend_square", NULL},
    {NULL, NULL}
};

extern const struct tnl_pipeline_stage _via_fastrender_stage;
extern const struct tnl_pipeline_stage _via_render_stage;

static const struct tnl_pipeline_stage *via_pipeline[] = {
    &_tnl_vertex_transform_stage,
    &_tnl_normal_transform_stage,
    &_tnl_lighting_stage,
    &_tnl_fog_coordinate_stage,
    &_tnl_texgen_stage,
    &_tnl_texture_transform_stage,
    /* REMOVE: point attenuation stage */
#if 1
    &_via_fastrender_stage,	       /* ADD: unclipped rastersetup-to-dma */
#endif
    &_tnl_render_stage,
    0,
};

static const struct dri_debug_control debug_control[] = {
    {"fall", DEBUG_FALLBACKS},
    {"tex", DEBUG_TEXTURE},
    {"ioctl", DEBUG_IOCTL},
    {"prim", DEBUG_PRIMS},
    {"vert", DEBUG_VERTS},
    {"state", DEBUG_STATE},
    {"verb", DEBUG_VERBOSE},
    {"dri", DEBUG_DRI},
    {"dma", DEBUG_DMA},
    {"san", DEBUG_SANITY},
    {"sync", DEBUG_SYNC},
    {"sleep", DEBUG_SLEEP},
    {"pix", DEBUG_PIXEL},
    {"2d", DEBUG_2D},
    {"fbo", DEBUG_FBO},
    {NULL, 0}
};

static GLboolean
AllocateDmaBuffer(struct via_context *vmesa)
{
    if (vmesa->dma)
	via_free_dma_buffer(vmesa);

    if (!via_alloc_dma_buffer(vmesa))
	return GL_FALSE;

    vmesa->dmaLow = 0;
    vmesa->dmaCliprectAddr = ~0;
    return GL_TRUE;
}

static void
FreeBuffer(struct via_context *vmesa)
{
    if (vmesa->dma)
	via_free_dma_buffer(vmesa);
}

GLboolean
viaCreateContext(const __GLcontextModes * visual,
		 __DRIcontextPrivate * driContextPriv,
		 void *sharedContextPrivate)
{
    GLcontext *ctx, *shareCtx;
    struct via_context *vmesa;
    __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
    viaScreenPrivate *viaScreen = (viaScreenPrivate *) sPriv->private;
    struct drm_via_sarea *saPriv = (struct drm_via_sarea *)
	(((GLubyte *) sPriv->pSAREA) + viaScreen->sareaPrivOffset);
    struct dd_function_table functions;

    /* Allocate via context */
    vmesa = (struct via_context *)CALLOC_STRUCT(via_context);
    if (!vmesa) {
	return GL_FALSE;
    }

    vmesa->drawAttIndex = BUFFER_BACK_LEFT;
    vmesa->readAttIndex = BUFFER_BACK_LEFT;

    vmesa->reloc_info = via_create_reloc_buffer();
    if (!vmesa->reloc_info)
	goto out_err0;

    vmesa->validate_list = wsbmBOCreateList(64, 1);
    if (!vmesa->validate_list)
	goto out_err1;

    vmesa->save_state = via_reloc_save_state_alloc();
    if (!vmesa->save_state)
	goto out_err2;

    _mesa_init_driver_functions(&functions);
    viaInitTextureFuncs(&functions);
    viaInitPixelFuncs(&functions);

    /* Allocate the Mesa context */
    if (sharedContextPrivate)
	shareCtx = ((struct via_context *)sharedContextPrivate)->glCtx;
    else
	shareCtx = NULL;

    vmesa->glCtx = _mesa_create_context(visual, shareCtx, &functions,
					(void *)vmesa);

    vmesa->shareCtx = shareCtx;

    if (!vmesa->glCtx) {
	FREE(vmesa);
	return GL_FALSE;
    }
    driContextPriv->driverPrivate = vmesa;

    ctx = vmesa->glCtx;

    /*
     * FIXME: Add more devices here when we've checked the maximum
     * texture size.
     */

    switch (viaScreen->deviceID) {
    case VIA_CX700:
	ctx->Const.MaxTextureLevels = 12;
	break;
    case VIA_CLE266:
    case VIA_K8M800:
    default:
	if (driQueryOptionb(&viaScreen->parsedCache, "excess_mipmap"))
	    ctx->Const.MaxTextureLevels = 11;
	else
	    ctx->Const.MaxTextureLevels = 10;
	break;
    }

    ctx->Const.MaxTextureUnits = 2;
    ctx->Const.MaxTextureImageUnits = ctx->Const.MaxTextureUnits;
    ctx->Const.MaxTextureCoordUnits = ctx->Const.MaxTextureUnits;

    ctx->Const.MinLineWidth = 1.0;
    ctx->Const.MinLineWidthAA = 1.0;
    ctx->Const.MaxLineWidth = 1.0;
    ctx->Const.MaxLineWidthAA = 1.0;
    ctx->Const.LineWidthGranularity = 1.0;

    ctx->Const.MinPointSize = 1.0;
    ctx->Const.MinPointSizeAA = 1.0;
    ctx->Const.MaxPointSize = 1.0;
    ctx->Const.MaxPointSizeAA = 1.0;
    ctx->Const.PointSizeGranularity = 1.0;

    ctx->Driver.GetString = viaGetString;

    ctx->DriverCtx = (void *)vmesa;
    vmesa->glCtx = ctx;
    vmesa->newEmitState = ~0;
    vmesa->lostState = 1;

    /* Initialize the software rasterizer and helper modules.
     */
    _swrast_CreateContext(ctx);
    _vbo_CreateContext(ctx);
    _tnl_CreateContext(ctx);
    _swsetup_CreateContext(ctx);

    /* Install the customized pipeline:
     */
    _tnl_destroy_pipeline(ctx);
    _tnl_install_pipeline(ctx, via_pipeline);

    /* Configure swrast and T&L to match hardware characteristics:
     */
    _swrast_allow_pixel_fog(ctx, GL_FALSE);
    _swrast_allow_vertex_fog(ctx, GL_TRUE);
    _tnl_allow_pixel_fog(ctx, GL_FALSE);
    _tnl_allow_vertex_fog(ctx, GL_TRUE);

    vmesa->hHWContext = driContextPriv->hHWContext;
    vmesa->driFd = sPriv->fd;
    vmesa->driHwLock = &sPriv->pSAREA->lock;

    vmesa->viaScreen = viaScreen;
    vmesa->execIoctlOffset = viaScreen->execIoctlOffset;
    vmesa->driScreen = sPriv;
    vmesa->sarea = saPriv;

    vmesa->renderIndex = ~0;
    vmesa->setupIndex = ~0;
    vmesa->hwPrimitive = GL_POLYGON + 1;

    /* KW: Hardwire this.  Was previously set bogusly in
     * viaCreateBuffer.  Needs work before PBUFFER can be used:
     */
    vmesa->drawType = GLX_WINDOW_BIT;

    _math_matrix_ctr(&vmesa->ViewportMatrix);

    /* Do this early, before VIA_FLUSH_DMA can be called:
     */
    if (!AllocateDmaBuffer(vmesa)) {
	fprintf(stderr, "AllocateDmaBuffer fail\n");
	FreeBuffer(vmesa);
	FREE(vmesa);
	return GL_FALSE;
    }

    driInitExtensions(ctx, card_extensions, GL_TRUE);
    viaInitStateFuncs(ctx);
    viaInitTriFuncs(ctx);
    viaInitSpanFuncs(ctx);
    viaInitIoctlFuncs(ctx);
    viaInitState(ctx);

    //    if (driQueryOptionb(&viaScreen->parsedCache, "fake_ext")) {
    _mesa_enable_1_3_extensions(ctx);
    //    }

    if (getenv("VIA_DEBUG"))
	VIA_DEBUG = driParseDebugString(getenv("VIA_DEBUG"), debug_control);

    if (getenv("VIA_NO_RAST") ||
	driQueryOptionb(&viaScreen->parsedCache, "no_rast"))
	FALLBACK(vmesa, VIA_FALLBACK_USER_DISABLE, 1);

    vmesa->vblank_flags =
	vmesa->viaScreen->irqEnabled ?
	driGetDefaultVBlankFlags(&viaScreen->
				 parsedCache) : VBLANK_FLAG_NO_IRQ;

    (*sPriv->systemTime->getUST) (&vmesa->swap_ust);

    via_bufferobj_init(vmesa);
    via_fbo_init(vmesa);

    return GL_TRUE;
  out_err2:
    wsbmBOFreeList(vmesa->validate_list);
  out_err1:
    via_free_reloc_buffer(vmesa->reloc_info);
  out_err0:
    free(vmesa);
    return GL_FALSE;
}

void
viaDestroyContext(__DRIcontextPrivate * driContextPriv)
{
    GET_CURRENT_CONTEXT(ctx);
    struct via_context *vmesa =
	(struct via_context *)driContextPriv->driverPrivate;
    struct via_context *current = ctx ? VIA_CONTEXT(ctx) : NULL;

    assert(vmesa);		       /* should never be null */

    /* check if we're deleting the currently bound context */
    if (vmesa == current) {
	VIA_FLUSH_DMA(vmesa);
	_mesa_make_current(NULL, NULL, NULL);
    }

    if (vmesa) {
	if (vmesa->last_fence) {
	    wsbmFenceFinish(vmesa->last_fence, 0x1, 0);
	    wsbmFenceUnreference(&vmesa->last_fence);
	}
	if (vmesa->doPageFlip) {
	    LOCK_HARDWARE(vmesa);
	    if (vmesa->pfCurrentOffset != 0) {
		fprintf(stderr, "%s - reset pf\n", __FUNCTION__);
		viaResetPageFlippingLocked(vmesa);
	    }
	    UNLOCK_HARDWARE(vmesa);
	}

	_swsetup_DestroyContext(vmesa->glCtx);
	_tnl_DestroyContext(vmesa->glCtx);
	_vbo_DestroyContext(vmesa->glCtx);
	_swrast_DestroyContext(vmesa->glCtx);
	/* free the Mesa context */
	_mesa_destroy_context(vmesa->glCtx);

	/* release our data */
	FreeBuffer(vmesa);
	free(vmesa->save_state);
	wsbmBOFreeList(vmesa->validate_list);
	via_free_reloc_buffer(vmesa->reloc_info);
	FREE(vmesa);
    }
}

/*
 * Update framebuffer info based on the backing driDrawable state.
 * The context pointer is only used for command submission and
 * should be the context currently holding the hardware lock.
 * Note that this function should never be called for user-created
 * fbos, so we use __DRIdrawablePrivates as arguments.
 */

extern void
viaUpdateFrameBufferLocked(struct via_context *vmesa,
			   __DRIdrawablePrivate * dPriv,
			   __DRIdrawablePrivate * rPriv)
{
    struct via_framebuffer *vfb;
    __DRIscreenPrivate *sPriv = vmesa->driScreen;
    GLuint bytePerPixel;

    if (dPriv == rPriv)
	rPriv = NULL;

    if (rPriv == NULL) {
	DRI_VALIDATE_DRAWABLE_INFO(sPriv, dPriv);
    } else {
	DRI_VALIDATE_TWO_DRAWABLES_INFO(sPriv, dPriv, rPriv);
    }

    /*
     * Update cached drawable state.
     */

    while (dPriv != NULL) {

	vfb = containerOf(dPriv->driverPrivate, struct via_framebuffer, Base);

	if (vfb->lastStamp != dPriv->lastStamp) {
	    drm_clip_rect_t *cur_clip;
	    struct via_renderbuffer *front =
		via_get_renderbuffer(&vfb->Base, BUFFER_FRONT_LEFT);
	    int i;

	    vfb->lastStamp = dPriv->lastStamp;
	    vfb->numFrontClipRects = dPriv->numClipRects;
	    vfb->pFrontClipRects = dPriv->pClipRects;
	    vfb->xoff = 0;
	    vfb->drawX = dPriv->x;
	    vfb->drawY = dPriv->y;

	    /*
	     * Make cliprects independent of drawable position.
	     * If the drawable position is needed (shared front buffers),
	     * we'll need to add it back.
	     */

	    cur_clip = vfb->pFrontClipRects;
	    for (i = 0; i < vfb->numFrontClipRects; ++i) {
		cur_clip->x1 -= vfb->drawX;
		cur_clip->x2 -= vfb->drawX;
		cur_clip->y1 -= vfb->drawY;
		cur_clip->y2 -= vfb->drawY;
		cur_clip++;
	    }

	    if (front->isSharedFrontBuffer) {
		struct gl_renderbuffer *rb = &front->Base;

		rb->AllocStorage(vmesa->glCtx, rb, rb->InternalFormat,
				 dPriv->w, dPriv->h);

		bytePerPixel = (front->bpp + 7) >> 3;

		vfb->xoff =
		    dPriv->x & (VIA_OFFSET_ALIGN_BYTES / bytePerPixel - 1);
		front->origAdd =
		    vfb->drawY * front->pitch + (vfb->drawX -
						 vfb->xoff) * bytePerPixel;

		front->origMapAdd =
		    vfb->drawY * front->pitch + vfb->drawX * bytePerPixel;

		vfb->allClipRect.x1 = 0;
		vfb->allClipRect.x2 = vfb->Base.Width;
		vfb->allClipRect.y1 = 0;
		vfb->allClipRect.y2 = vfb->Base.Height;
	    }
	}
	dPriv = rPriv;
	rPriv = NULL;
    }
}

/*
 * Update cached readable and drawable state.
 */

static void
viaValidateReadDrawState(struct via_context *vmesa)
{
    struct via_framebuffer *vfb = viaDrawFrameBuffer(vmesa);
    GLcontext *ctx = vmesa->glCtx;

    if (vfb && (vmesa->drawStamp != vfb->lastStamp))
	ctx->Driver.DrawBuffer(ctx, vfb->Base.ColorDrawBuffer[0]);

    vfb = viaReadFrameBuffer(vmesa);
    if (vfb && (vmesa->readStamp != vfb->lastStamp))
	ctx->Driver.ReadBuffer(vmesa->glCtx, vfb->Base.ColorReadBuffer);
}

/*
 * If the draw- and read- framebuffers attached to this context
 * are window system framebuffers, update their size to match the
 * window systems size, and also update all info we've cached in the
 * context about these drawables.
 *
 * Note that it should be OK to do this after every lock, since no
 * expensive functions will be called without a stamp check. We're using
 * the lostlock variable to determine when it's really necessary, though.
 */

void
viaValidateDrawablesLocked(struct via_context *vmesa)
{
    struct gl_framebuffer *fb = vmesa->glCtx->DrawBuffer;
    __DRIdrawablePrivate *dPriv = NULL;
    __DRIdrawablePrivate *rPriv = NULL;

    if (vmesa->lostLock == 0)
	return;

    if (fb->Name == 0)
	dPriv = via_framebuffer(fb)->dPriv;

    fb = vmesa->glCtx->ReadBuffer;
    if (fb->Name == 0)
	rPriv = via_framebuffer(fb)->dPriv;

    if (dPriv == NULL)
	dPriv = rPriv;
    if (dPriv == NULL)
	goto out;

    viaUpdateFrameBufferLocked(vmesa, dPriv, rPriv);
    driUpdateFramebufferSize(vmesa->glCtx, dPriv);
    if (rPriv && rPriv != dPriv)
	driUpdateFramebufferSize(vmesa->glCtx, rPriv);
    viaValidateReadDrawState(vmesa);
  out:
    vmesa->lostLock = 0;
}

GLboolean
viaUnbindContext(__DRIcontextPrivate * driContextPriv)
{
    return GL_TRUE;
}

GLboolean
viaMakeCurrent(__DRIcontextPrivate * driContextPriv,
	       __DRIdrawablePrivate * driDrawPriv,
	       __DRIdrawablePrivate * driReadPriv)
{
    GLboolean newBuffers = GL_FALSE;

    if (VIA_DEBUG & DEBUG_DRI) {
	fprintf(stderr, "driContextPriv = %016lx\n",
		(unsigned long)driContextPriv);
	fprintf(stderr, "driDrawPriv = %016lx\n", (unsigned long)driDrawPriv);
	fprintf(stderr, "driReadPriv = %016lx\n", (unsigned long)driReadPriv);
    }

    if (driContextPriv) {
	struct via_context *vmesa =
	    (struct via_context *)driContextPriv->driverPrivate;
	GLcontext *ctx = vmesa->glCtx;
	struct gl_framebuffer *drawBuffer, *readBuffer;

	GET_CURRENT_CONTEXT(oldctx);

	if (ctx != oldctx && oldctx != NULL) {
	    VIA_FLUSH_DMA(VIA_CONTEXT(oldctx));
	}

	drawBuffer = (GLframebuffer *) driDrawPriv->driverPrivate;
	readBuffer = (GLframebuffer *) driReadPriv->driverPrivate;
	assert(drawBuffer->Name == 0);
	assert(readBuffer->Name == 0);

	if (vmesa->driDrawable != driDrawPriv) {
	    driDrawPriv->vblFlags = vmesa->vblank_flags;
	    driDrawableInitVBlank(driDrawPriv);
	}

	if ((vmesa->driDrawable != driDrawPriv)
	    || (vmesa->driReadable != driReadPriv)) {
	    newBuffers = GL_TRUE;

	    vmesa->driDrawable = driDrawPriv;
	    vmesa->driReadable = driReadPriv;

	    if (driDrawPriv == NULL)
		driDrawPriv = driReadPriv;

	    /*
	     * Get the latest dimensions for the window system drawables.
	     */

	    if (driDrawPriv != NULL) {
		LOCK_HARDWARE(vmesa);
		viaUpdateFrameBufferLocked(vmesa, driDrawPriv, driReadPriv);
		driUpdateFramebufferSize(vmesa->glCtx, driDrawPriv);
		if (driReadPriv)
		    driUpdateFramebufferSize(vmesa->glCtx, driReadPriv);
		vmesa->lostLock = 0;
		UNLOCK_HARDWARE(vmesa);
	    }
	    drawBuffer->Initialized = GL_TRUE;
	    readBuffer->Initialized = GL_TRUE;
	}

	_mesa_make_current(ctx, drawBuffer, readBuffer);

	/*
	 * Note that the context Draw- and Readbuffers may differ
	 * from "drawBuffer" and "readBuffer" if an fbo is bound.
	 * Otherwise go on updating all cached read and draw state.
	 */

	if (newBuffers) {
	    if (drawBuffer == ctx->DrawBuffer)
		ctx->Driver.DrawBuffer(ctx, drawBuffer->ColorDrawBuffer[0]);
	    if (readBuffer == ctx->ReadBuffer)
		ctx->Driver.ReadBuffer(ctx, readBuffer->ColorReadBuffer);
	}
    } else {
	_mesa_make_current(NULL, NULL, NULL);
    }

    return GL_TRUE;
}

void
viaGetLock(struct via_context *vmesa, GLuint flags)
{
    drmGetLock(vmesa->driFd, vmesa->hHWContext, flags);
    vmesa->lostLock = 1;
}

void
viaSwapBuffers(__DRIdrawablePrivate * drawablePrivate)
{
    GET_CURRENT_CONTEXT(ctx);
    __DRIdrawablePrivate *dPriv = drawablePrivate;

    if (ctx && dPriv->driverPrivate == ctx->DrawBuffer) {
	struct via_context *vmesa = VIA_CONTEXT(ctx);

	_mesa_notifySwapBuffers(ctx);
	vmesa->firstDrawAfterSwap = GL_TRUE;
    }

    /*
     * FIXME: Re-enable pageflipping.
     */

    viaCopyBuffer(dPriv);
}

void
via_wait_context_idle(struct via_context *vmesa)
{
    VIA_FLUSH_DMA(vmesa);
    if (vmesa->last_fence) {
	wsbmFenceFinish(vmesa->last_fence, 0x1, 0);
	wsbmFenceUnreference(&vmesa->last_fence);
    }
}
