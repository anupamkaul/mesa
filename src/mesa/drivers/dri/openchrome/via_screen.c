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

#include <stdio.h>

#include "main/glheader.h"
#include "main/context.h"
#include "main/framebuffer.h"
#include "main/renderbuffer.h"
#include "main/matrix.h"
#include "main/macros.h"

#include "utils.h"
#include "dri_util.h"
#include "vblank.h"

#include "via_state.h"
#include "via_tex.h"
#include "via_span.h"
#include "via_tris.h"
#include "via_ioctl.h"
#include "via_screen.h"
#include "via_dri.h"
#include "GL/internal/dri_interface.h"
#include "drirenderbuffer.h"
#include "xmlpool.h"
#include "wsbm_manager.h"

const char __driConfigOptions[] = DRI_CONF_BEGIN DRI_CONF_SECTION_PERFORMANCE
    DRI_CONF_FTHROTTLE_MODE(DRI_CONF_FTHROTTLE_BUSY)
    DRI_CONF_VBLANK_MODE(DRI_CONF_VBLANK_DEF_INTERVAL_0)
    DRI_CONF_SECTION_END DRI_CONF_SECTION_QUALITY
    DRI_CONF_EXCESS_MIPMAP(false)
    DRI_CONF_SECTION_END DRI_CONF_SECTION_DEBUG
    DRI_CONF_NO_RAST(false)
    DRI_CONF_SECTION_END DRI_CONF_END;
    static const GLuint __driNConfigOptions = 4;

    extern const struct dri_extension card_extensions[];

#if 0
    static GLboolean
	viaCreateDummyHWContext(__DRIscreenPrivate * sPriv,
				__DRIid * pContextID,
				drm_context_t * hw_context)
{
    /*
     * Use the first server supported GLX visual.
     * This isn't really used by the
     * via DDX anyway.
     */

    GLint configID = sPriv->modes->visualID;

    free(sPriv->modes);
    sPriv->modes = NULL;

    return dri_interface->createContext(sPriv->display, sPriv->myNum,
					configID, pContextID, hw_context);
}

static void
viaDestroyDummyHWContext(__DRIscreenPrivate * sPriv, __DRIid contextID)
{
    /*
     * The connection already appears to be down at this point.
     */

    //    (void) dri_interface->destroyContext(sPriv->display, sPriv->myNum,
    //                                   contextID);
    return;
}
#endif

static __inline__ unsigned
buffer_align(unsigned width)
{
    return (width + 0x0f) & ~0x0f;
}

static GLboolean
viaInitDriver(__DRIscreenPrivate * sPriv)
{
    viaScreenPrivate *viaScreen;
    VIADRIPtr gDRIPriv = (VIADRIPtr) sPriv->pDevPriv;
    int ret;
    struct drm_via_getparam_arg gp_arg;
    union drm_via_extension_arg ext_arg;
    const char ttm_ext[] = "via_ttm_placement_081121";
    const char fence_ext[] = "via_ttm_fence_081121";
    const char exec_ext[] = "via_ttm_execbuf";
    int i;

    if (sPriv->devPrivSize != sizeof(VIADRIRec)) {
	fprintf(stderr,
		"\nERROR!  sizeof(VIADRIRec) does not match passed size from device driver\n");
	return GL_FALSE;
    }

    ret = wsbmInit(wsbmPThreadFuncs(), viaVNodeFuncs());
    if (ret != 0)
	return GL_FALSE;

    /* Allocate the private area */
    viaScreen = (viaScreenPrivate *) CALLOC(sizeof(viaScreenPrivate));
    if (!viaScreen) {
	__driUtilMessage
	    ("viaInitDriver: alloc viaScreenPrivate struct failed");
	goto out_err0;
    }

    /* parse information in __driConfigOptions */
    driParseOptionInfo(&viaScreen->optionCache,
		       __driConfigOptions, __driNConfigOptions);
    /* Parse configuration files.
     */

    driParseConfigFiles(&viaScreen->parsedCache, &viaScreen->optionCache,
			sPriv->myNum, "unichrome");

    viaScreen->driScrnPriv = sPriv;
    sPriv->private = (void *)viaScreen;

    viaScreen->deviceID = gDRIPriv->deviceID;
    viaScreen->mallocPool = wsbmMallocPoolInit();
    if (!viaScreen->mallocPool) {
	_mesa_printf("Initialization of buffer pools failed.\n");
	goto out_err1;
    }
    strncpy(ext_arg.extension, ttm_ext, sizeof(ext_arg.extension));
    ret = drmCommandWriteRead(sPriv->fd, DRM_VIA_EXTENSION, &ext_arg,
			      sizeof(ext_arg));
    if (ret != 0 || !ext_arg.rep.exists) {
	_mesa_printf("Could not detect DRM extension \"%s\".\n", ttm_ext);
	goto out_err2;
    }

    viaScreen->bufferPool = wsbmTTMPoolInit(sPriv->fd,
					    ext_arg.rep.driver_ioctl_offset);
    if (!viaScreen->bufferPool) {
	_mesa_printf("VIA buffer manager creation failed.\n");
	goto out_err2;
    }

    strncpy(ext_arg.extension, exec_ext, sizeof(ext_arg.extension));
    ret = drmCommandWriteRead(sPriv->fd, DRM_VIA_EXTENSION, &ext_arg,
			      sizeof(ext_arg));
    if (ret != 0 || !ext_arg.rep.exists) {
	_mesa_printf("Could not detect DRM extension \"%s\".\n", exec_ext);
	goto out_err3;
    }
    viaScreen->execIoctlOffset = ext_arg.rep.driver_ioctl_offset;

    /*
     * We cannot rely on a dummy hw context from the X server,
     * since it only creates a single dummy hw context. We need
     * one per client.
     */
#if 0
    if (!viaCreateDummyHWContext(sPriv, &viaScreen->dummyContextID,
				 &viaScreen->dummyContext)) {
	_mesa_printf("Could not create dummy screen context.\n");
	goto out_err3;
    }
#endif
    if (VIA_DEBUG & DEBUG_DRI) {
	fprintf(stderr, "deviceID = %08x\n", viaScreen->deviceID);
	fprintf(stderr, "bpp = %08x\n", viaScreen->bitsPerPixel);
    }

    viaScreen->sareaPrivOffset = gDRIPriv->sarea_priv_offset;
    gp_arg.param = DRM_VIA_PARAM_HAS_IRQ;
    ret = drmCommandWriteRead(sPriv->fd, DRM_VIA_GET_PARAM, &gp_arg,
			      sizeof(gp_arg));

    if (ret == 0 || gp_arg.value != 0)
	viaScreen->irqEnabled = 1;
    else
	viaScreen->irqEnabled = 0;
    viaScreen->bitsPerPixel = gDRIPriv->bpp;

    strncpy(ext_arg.extension, fence_ext, sizeof(ext_arg.extension));
    ret = drmCommandWriteRead(sPriv->fd, DRM_VIA_EXTENSION, &ext_arg,
			      sizeof(ext_arg));
    if (ret != 0 || !ext_arg.rep.exists) {
	_mesa_printf("Could not detect DRM extension \"%s\".\n", fence_ext);
	goto out_err4;
    }

    viaScreen->fence_mgr = wsbmFenceMgrTTMInit(sPriv->fd, 5,
					       ext_arg.rep.
					       driver_ioctl_offset);

    if (!viaScreen->fence_mgr) {
	_mesa_printf("VIA fence manager creation failed.\n");
	goto out_err4;
    }

    i = 0;
    viaScreen->extensions[i++] = &driFrameTrackingExtension.base;
    viaScreen->extensions[i++] = &driReadDrawableExtension;
    if (viaScreen->irqEnabled) {
	viaScreen->extensions[i++] = &driSwapControlExtension.base;
	viaScreen->extensions[i++] = &driMediaStreamCounterExtension.base;
    }

    viaScreen->extensions[i++] = NULL;
    sPriv->extensions = viaScreen->extensions;

    return GL_TRUE;
  out_err4:
    //    viaDestroyDummyHWContext(sPriv, viaScreen->dummyContextID);
  out_err3:
    viaScreen->bufferPool->takeDown(viaScreen->bufferPool);
  out_err2:
    viaScreen->mallocPool->takeDown(viaScreen->mallocPool);
  out_err1:
    driDestroyOptionCache(&viaScreen->parsedCache);
    FREE(viaScreen);
    sPriv->private = NULL;
  out_err0:
    wsbmTakedown();
    return GL_FALSE;
}

static void
viaDestroyScreen(__DRIscreenPrivate * sPriv)
{
    viaScreenPrivate *viaScreen = (viaScreenPrivate *) sPriv->private;

    wsbmFenceMgrTTMTakedown(viaScreen->fence_mgr);
    //    viaDestroyDummyHWContext(sPriv, viaScreen->dummyContextID);
    viaScreen->bufferPool->takeDown(viaScreen->bufferPool);
    viaScreen->mallocPool->takeDown(viaScreen->mallocPool);
    driDestroyOptionCache(&viaScreen->parsedCache);
    driDestroyOptionInfo(&viaScreen->optionCache);
    FREE(viaScreen);
    sPriv->private = NULL;
    wsbmTakedown();
}

static GLboolean
viaCreateBuffer(__DRIscreenPrivate * driScrnPriv,
		__DRIdrawablePrivate * driDrawPriv,
		const __GLcontextModes * mesaVis, GLboolean isPixmap)
{
    GLboolean swAccum = mesaVis->accumRedBits > 0;
    GLenum rgbFormat = (mesaVis->redBits == 5 ? GL_RGB5 : GL_RGBA8);
    viaScreenPrivate *viaScreen = driScrnPriv->private;

    if (isPixmap) {
	return GL_FALSE;
    } else {
	struct via_framebuffer *viafb = (struct via_framebuffer *)
	    CALLOC_STRUCT(via_framebuffer);
	struct via_renderbuffer *front;
	struct gl_framebuffer *fb;

	if (!viafb)
	    return GL_FALSE;

	fb = &viafb->Base;
	_mesa_initialize_framebuffer(fb, mesaVis);
	/* Init the front framebuffer */

	viafb->fthrottle_mode = driQueryOptioni(&viaScreen->parsedCache,
						"fthrottle_mode");
	viafb->used_swap_fences = 3;

	front = via_create_renderbuffer(rgbFormat, driScrnPriv, GL_TRUE);
	if (!front)
	    goto out_err;
	_mesa_add_renderbuffer(fb, BUFFER_FRONT_LEFT, &front->Base);

	if (fb->Visual.doubleBufferMode) {
	    struct via_renderbuffer *back =
		via_create_renderbuffer(rgbFormat, driScrnPriv, GL_FALSE);
	    if (!back)
		goto out_err;
	    _mesa_add_renderbuffer(fb, BUFFER_BACK_LEFT, &back->Base);

	}

	if (mesaVis->depthBits == 24 && mesaVis->stencilBits == 8) {
	    struct via_renderbuffer *depth =
		via_create_renderbuffer(GL_DEPTH24_STENCIL8_EXT, driScrnPriv,
					GL_FALSE);

	    if (!depth)
		goto out_err;
	    _mesa_add_renderbuffer(fb, BUFFER_DEPTH, &depth->Base);
	    _mesa_add_renderbuffer(fb, BUFFER_STENCIL, &depth->Base);

	} else if (mesaVis->depthBits == 16) {
	    struct via_renderbuffer *depth =
		via_create_renderbuffer(GL_DEPTH_COMPONENT16, driScrnPriv,
					GL_FALSE);

	    if (!depth)
		goto out_err;
	    _mesa_add_renderbuffer(fb, BUFFER_DEPTH, &depth->Base);

	} else if (mesaVis->depthBits == 32) {
	    struct via_renderbuffer *depth =
		via_create_renderbuffer(GL_DEPTH_COMPONENT32, driScrnPriv,
					GL_FALSE);

	    if (!depth)
		goto out_err;
	    _mesa_add_renderbuffer(fb, BUFFER_DEPTH, &depth->Base);

	}

	_mesa_add_soft_renderbuffers(fb, GL_FALSE,	/* color */
				     GL_FALSE,	/* depth */
				     GL_FALSE, swAccum, GL_FALSE,	/* alpha */
				     GL_FALSE /* aux */ );

	driDrawPriv->driverPrivate = (void *)&viafb->Base;
	viafb->dPriv = driDrawPriv;

	return GL_TRUE;
      out_err:
	_mesa_unreference_framebuffer(&fb);
	return GL_FALSE;
    }
}

static void
viaDestroyBuffer(__DRIdrawablePrivate * driDrawPriv)
{
    struct via_framebuffer *viafb =
	(struct via_framebuffer *)driDrawPriv->driverPrivate;
    struct gl_framebuffer *fb = &viafb->Base;
    int i;

    driDrawPriv->driverPrivate = NULL;
    for (i = 0; i < VIA_MAX_SWAP_FENCES; ++i) {
	if (viafb->swap_fences[i] != 0)
	    wsbmFenceUnreference(&viafb->swap_fences[i]);
    }
    if (viafb->pFrontClipRects &&
	(viafb->pFrontClipRects != &viafb->allClipRect)) {
	free(viafb->pFrontClipRects);
    }

    _mesa_unreference_framebuffer(&fb);
}

static const __DRIconfig **
viaFillInModes(__DRIscreenPrivate * psp,
	       unsigned pixel_bits, GLboolean have_back_buffer)
{
    __DRIconfig **configs;
    const unsigned back_buffer_factor = (have_back_buffer) ? 2 : 1;
    GLenum fb_format;
    GLenum fb_type;

    /* Right now GLX_SWAP_COPY_OML isn't supported, but it would be easy
     * enough to add support.  Basically, if a context is created with an
     * fbconfig where the swap method is GLX_SWAP_COPY_OML, pageflipping
     * will never be used.
     */
    static const GLenum back_buffer_modes[] = {
	GLX_NONE, GLX_SWAP_UNDEFINED_OML	/*, GLX_SWAP_COPY_OML */
    };

    /* The 32-bit depth-buffer mode isn't supported yet, so don't actually
     * enable it.
     */
    static const uint8_t depth_bits_array[4] = { 0, 16, 24, 32 };
    static const uint8_t stencil_bits_array[4] = { 0, 0, 8, 0 };
    const unsigned depth_buffer_factor = 3;

    if (pixel_bits == 16) {
	fb_format = GL_RGB;
	fb_type = GL_UNSIGNED_SHORT_5_6_5;
    } else {
	fb_format = GL_BGRA;
	fb_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

    configs = driCreateConfigs(fb_format, fb_type,
			       depth_bits_array, stencil_bits_array,
			       depth_buffer_factor, back_buffer_modes,
			       back_buffer_factor);
    if (configs == NULL) {
	fprintf(stderr, "[%s:%u] Error creating FBConfig!\n", __func__,
		__LINE__);
	return NULL;
    }

    return (const __DRIconfig **)configs;
}

/**
 * This is the driver specific part of the createNewScreen entry point.
 *
 * \todo maybe fold this into intelInitDriver
 *
 * \return the __GLcontextModes supported by this driver
 */
static const __DRIconfig **
viaInitScreen(__DRIscreenPrivate * psp)
{
    static const __DRIversion ddx_expected = { VIA_DRIDDX_VERSION_MAJOR,
	VIA_DRIDDX_VERSION_MINOR,
	VIA_DRIDDX_VERSION_PATCH
    };
    static const __DRIversion dri_expected = { 4, 0, 0 };
    static const __DRIversion drm_expected = { 0, 1, 0 };
    static const char *driver_name = "Openchrome";
    VIADRIPtr dri_priv = (VIADRIPtr) psp->pDevPriv;

    if (!driCheckDriDdxDrmVersions2(driver_name,
				    &psp->dri_version, &dri_expected,
				    &psp->ddx_version, &ddx_expected,
				    &psp->drm_version, &drm_expected))
	return NULL;

    /* Calling driInitExtensions here, with a NULL context pointer,
     * does not actually enable the extensions.  It just makes sure
     * that all the dispatch offsets for all the extensions that
     * *might* be enables are known.  This is needed because the
     * dispatch offsets need to be known when _mesa_context_create is
     * called, but we can't enable the extensions until we have a
     * context pointer.
     *
     * Hello chicken.  Hello egg.  How are you two today?
     */
    driInitExtensions(NULL, card_extensions, GL_FALSE);

    if (!viaInitDriver(psp))
	return NULL;

    return viaFillInModes(psp, dri_priv->bpp, GL_TRUE);

}

/**
 * Get information about previous buffer swaps.
 */
static int
getSwapInfo(__DRIdrawablePrivate * dPriv, __DRIswapInfo * sInfo)
{
    struct via_context *vmesa;

    if ((dPriv == NULL) || (dPriv->driContextPriv == NULL)
	|| (dPriv->driContextPriv->driverPrivate == NULL)
	|| (sInfo == NULL)) {
	return -1;
    }

    vmesa = (struct via_context *)dPriv->driContextPriv->driverPrivate;
    sInfo->swap_count = vmesa->swap_count;
    sInfo->swap_ust = vmesa->swap_ust;
    sInfo->swap_missed_count = vmesa->swap_missed_count;

    sInfo->swap_missed_usage = (sInfo->swap_missed_count != 0)
	? driCalculateSwapUsage(dPriv, 0, vmesa->swap_missed_ust)
	: 0.0;

    return 0;
}

const struct __DriverAPIRec driDriverAPI = {
    .InitScreen = viaInitScreen,
    .DestroyScreen = viaDestroyScreen,
    .CreateContext = viaCreateContext,
    .DestroyContext = viaDestroyContext,
    .CreateBuffer = viaCreateBuffer,
    .DestroyBuffer = viaDestroyBuffer,
    .SwapBuffers = viaSwapBuffers,
    .MakeCurrent = viaMakeCurrent,
    .UnbindContext = viaUnbindContext,
    .GetSwapInfo = getSwapInfo,
    .GetDrawableMSC = driDrawableGetMSC32,
    .WaitForMSC = driWaitForMSC32,
    .WaitForSBC = NULL,
    .SwapBuffersMSC = NULL
};
