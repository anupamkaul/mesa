/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 * Copyright (C) 2008-2009  Matthias Hopf
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
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Matthias Hopf
 *   Richard Li <RichardZ.Li@amd.com>, <richardradeon@gmail.com>
 *   CooperYuan <cooper.yuan@amd.com>, <cooperyuan@gmail.com>
 */

#include "r600_screen.h"
#include "r600_context.h"
#include "r600_id.h"
#include "r600_span.h"

#include "server/radeon_dri.h"

#include "utils.h"
#include "drirenderbuffer.h"
#include "vblank.h"

#include "framebuffer.h"
#include "renderbuffer.h"

int debug = 1;


/* Get 64bit param (currently drm interface is only 32bit, though). Returns -1 on error. */
static uint64_t
r600GetParam (__DRIscreenPrivate *sPriv, int param, char *what)
{
    int r;
    drm_radeon_getparam_t gp;
    uint32_t val;
    
    gp.param = param;
    gp.value = &val;
    
    if ( (r = drmCommandWriteRead (sPriv->fd, DRM_RADEON_GETPARAM, &gp, sizeof(gp))) >= 0)
	return val;
    
    if (what)
	drmError (r, what);
    return -1;
}

/* Map memory */
static int
r600DrmMap (__DRIscreenPrivate *sPriv, memmap_t *map,
	    drm_handle_t handle, drmSize size, uint64_t offset, char *what)
{
    int r;
    map->handle = handle;
    map->size   = size;
    map->gpu    = offset;
    if ( (r = drmMap (sPriv->fd, handle, size, &map->cpu)) < 0)
	__driUtilMessage ("%s: drmMap(%s) failed\n", __FUNCTION__, what);
    return r;
}

/* UnMap memory */
static int
r600DrmUnMap (drmAddress address, drmSize size)
{
    int r;
    if ( (r = drmUnmap(address, size)) < 0)
    {
        __driUtilMessage ("%s: drmMap failed\n", __FUNCTION__);
    }
    return r;
}


/*
 * Free the device specific screen private data struct and close everything associated.
 */
static void *
r600FreeScreen (screen_t *screen)
{
    if (! screen)
	return NULL;
    
    if (screen->regs.cpu)
	drmUnmap (screen->regs.cpu, screen->regs.size);
    if (screen->status.cpu)
	drmUnmap (screen->status.cpu, screen->status.size);
    if (screen->textures.cpu)
	drmUnmap (screen->status.cpu, screen->status.size);
    if (screen->buffers)
	drmUnmapBufs (screen->buffers);
    FREE (screen);
    
    return NULL;
}

    
/*
 * Create the device specific screen private data struct.
 */
static screen_t *
r600CreateScreen (__DRIscreenPrivate *sPriv)
{
    screen_t     *screen;
    RADEONDRIPtr  driPriv = (RADEONDRIPtr)sPriv->pDevPriv;
    int i;

    if (sPriv->devPrivSize != sizeof(RADEONDRIRec)) {
	fprintf (stderr,"\nERROR!  sizeof(RADEONDRIRec) does not match passed size from device driver\n");
	return NULL;
    }

    /* Allocate the private area */
    if (! (screen = (screen_t *) CALLOC (sizeof(*screen))) ) {
	__driUtilMessage ("%s: Could not allocate memory for screen structure",
			  __FUNCTION__);
	return NULL;
    }
    screen->driScreen       = sPriv;
    screen->sareaPrivOffset = driPriv->sarea_priv_offset;

    /* Verify chipset */
    for (i = 0; r600_chips[i].type; i++)
	if (r600_chips[i].id == driPriv->deviceID) {
	    memcpy (&screen->chip, &r600_chips[i], sizeof (chip_t));
	    break;
	}
    if (! r600_chips[i].type) {
	fprintf (stderr, "unknown chip id 0x%x.\n", driPriv->deviceID);
	return r600FreeScreen (screen);
    }

    /* Framebuffer location */
    if ( (screen->fb.gpu = r600GetParam (sPriv, RADEON_PARAM_FB_LOCATION,
					 "RADEON_PARAM_FB_LOCATION")) == -1)
	return r600FreeScreen (screen);
    screen->fb.gpu  = (screen->fb.gpu & 0xffff) << 24;
    screen->fb.cpu  = sPriv->pFB;
    screen->fb.size = sPriv->fbSize;
   
    /* GART location */
    if ( (screen->gart.gpu = r600GetParam (sPriv, RADEON_PARAM_GART_BASE,
					   "RADEON_PARAM_GART_BASE")) == -1)
	return r600FreeScreen (screen);

    /* Register map */
    if (r600DrmMap (sPriv, &screen->regs,
		    driPriv->registerHandle, driPriv->registerSize,
		    0, "regs") < 0)
	return r600FreeScreen (screen);

    /* Memory copies of ring read ptr, scratch regs */
    if (r600DrmMap (sPriv, &screen->status,
		    driPriv->statusHandle, driPriv->statusSize,
		    0, "status") < 0)
	return r600FreeScreen (screen);
    screen->scratch = (__volatile__ u_int32_t *)
	((GLubyte *)screen->status.cpu + R600_SCRATCH_REG_OFFSET);

    /* Textures */
    /* BTW - RADEON_PARAM_GART_TEX_HANDLE does not work yet */
    if (r600DrmMap (sPriv, &screen->gartTextures,
		    driPriv->gartTexHandle, driPriv->gartTexMapSize,
		    driPriv->gartTexOffset + screen->gart.gpu, "textures") < 0)
	return r600FreeScreen (screen);
    screen->textures.gpu  = driPriv->textureOffset + screen->fb.gpu;
    screen->textures.size = driPriv->textureSize;
   
    /* DRM buffers */
    if (! (screen->buffers = drmMapBufs (sPriv->fd)) ) {
	__driUtilMessage ("%s: drmMapBufs failed\n", __FUNCTION__);
	return r600FreeScreen (screen);
    }
    if ( (screen->bufs.gpu = r600GetParam (sPriv, RADEON_PARAM_GART_BUFFER_OFFSET,
					   "RADEON_PARAM_GART_BUFFER_OFFSET")) == -1)
	return r600FreeScreen (screen);
    screen->bufs.cpu  = screen->buffers->list[0].address;
    screen->bufs.size = screen->buffers->count * screen->buffers->list[0].total;

    /* Interrupt */
    /* TODO (RADEON_PARAM_IRQ_NR) */

    /* Targets */
    screen->cpp = driPriv->bpp / 8; /* still bpp 15 there? */
    screen->frontBuffer.gpu   = driPriv->frontOffset + screen->fb.gpu;
    screen->frontBuffer.cpu   = driPriv->frontOffset + screen->fb.cpu;
    screen->frontBuffer.size  = driPriv->frontPitch  * screen->cpp    * driPriv->height;
    screen->frontBuffer.pitch = driPriv->frontPitch;
    screen->backBuffer.gpu    = driPriv->backOffset  + screen->fb.gpu;
    screen->backBuffer.cpu    = driPriv->backOffset  + screen->fb.cpu;
    screen->backBuffer.size   = driPriv->backPitch   * screen->cpp    * driPriv->height;
    screen->backBuffer.pitch  = driPriv->backPitch;
    /* TODO: currently same cpp as for front/back buffer */
    screen->depthBuffer.gpu   = driPriv->depthOffset + screen->fb.gpu;
    screen->depthBuffer.cpu   = driPriv->depthOffset + screen->fb.cpu;
    screen->depthBuffer.size  = driPriv->depthPitch  * screen->cpp    * driPriv->height;
    screen->depthBuffer.pitch = driPriv->depthPitch;

    screen->width  = driPriv->width;
    screen->height = driPriv->height;

    if (debug >= 1) {
	fprintf (stderr, "[r600]  Mappings:\n"
		 "\tRegisters                         cpu %p   size 0x%08x   handle 0x%08x\n"
		 "\tStatus                            cpu %p   size 0x%08x   handle 0x%08x\n"
		 "\tGART           gpu 0x" PRINTF_UINT64_HEX "\n"
		 "\tDRM buffers    gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x\n"
		 "\tGART Textures  gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x   handle 0x%08x\n"
		 "\tFramebuffer    gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x\n"
		 "\tFront Target   gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x   pitch 0x%04x\n"
		 "\tBack  Target   gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x   pitch 0x%04x\n"
		 "\tDepth Target   gpu 0x" PRINTF_UINT64_HEX "   cpu %p   size 0x%08x   pitch 0x%04x\n\n",
		 screen->regs.cpu,   screen->regs.size,   screen->regs.handle,
		 screen->status.cpu, screen->status.size, screen->status.handle,
		 screen->gart.gpu,
		 screen->bufs.gpu,   screen->bufs.cpu,   screen->bufs.size,
		 screen->gartTextures.gpu, screen->gartTextures.cpu, screen->gartTextures.size, screen->gartTextures.handle,
		 screen->fb.gpu,     screen->fb.cpu,     screen->fb.size,
		 screen->frontBuffer.gpu,  screen->frontBuffer.cpu,  screen->frontBuffer.size,  screen->frontBuffer.pitch,
		 screen->backBuffer.gpu,   screen->backBuffer.cpu,   screen->backBuffer.size,   screen->backBuffer.pitch,
		 screen->depthBuffer.gpu,  screen->depthBuffer.cpu,  screen->depthBuffer.size,  screen->depthBuffer.pitch);
    }
    
    screen->gart_texture_offset = driPriv->gartTexOffset + screen->gart.gpu;

    if ( driPriv->textureSize == 0 ) 
    {
        screen->texOffset[RADEON_LOCAL_TEX_HEAP] = screen->gart_texture_offset;
        screen->texSize[RADEON_LOCAL_TEX_HEAP] = driPriv->gartTexMapSize;
        screen->logTexGranularity[RADEON_LOCAL_TEX_HEAP] = driPriv->log2GARTTexGran;
    } 
    else 
    {
        screen->texOffset[RADEON_LOCAL_TEX_HEAP] = driPriv->textureOffset + screen->fb.gpu;
        screen->texSize[RADEON_LOCAL_TEX_HEAP] = driPriv->textureSize;
        screen->logTexGranularity[RADEON_LOCAL_TEX_HEAP] = driPriv->log2TexGran;
    }
    if ( !screen->gartTextures.cpu || driPriv->textureSize == 0
                                   || getenv( "RADEON_GARTTEXTURING_FORCE_DISABLE" ) ) 
    {
        screen->numTexHeaps = RADEON_NR_TEX_HEAPS - 1;
        screen->texOffset[RADEON_GART_TEX_HEAP] = 0;
        screen->texSize[RADEON_GART_TEX_HEAP] = 0;
        screen->logTexGranularity[RADEON_GART_TEX_HEAP] = 0;
    } 
    else 
    {
        screen->numTexHeaps = RADEON_NR_TEX_HEAPS;
        screen->texOffset[RADEON_GART_TEX_HEAP] = screen->gart_texture_offset;
        screen->texSize[RADEON_GART_TEX_HEAP] = driPriv->gartTexMapSize;
        screen->logTexGranularity[RADEON_GART_TEX_HEAP] = driPriv->log2GARTTexGran;
    }
    
    return screen;
}


/*
 * Create visual list
 */
static const __DRIconfig **
r600FillInModes (__DRIscreenPrivate *psp,
		 unsigned pixel_bits, unsigned depth_bits,
		 unsigned stencil_bits)
{
    __DRIconfig **configs;
    GLenum fb_format;
    GLenum fb_type;

    uint8_t depth_bits_array[1];
    uint8_t stencil_bits_array[1];

    static const GLenum back_buffer_modes[] = {
        GLX_NONE, GLX_SWAP_UNDEFINED_OML
    };

    /* Only support a single buffered and a double buffered mode, with
     * everything enabled including depth and stencil buffer. Features
     * not used won't use computation time, and we have a shared depth
     * buffer anyway. */

    depth_bits_array[0] = depth_bits;
    stencil_bits_array[0] = (stencil_bits == 0) ? 8 : stencil_bits;

    if ( pixel_bits == 16 ) {
        fb_format = GL_RGB;
        fb_type = GL_UNSIGNED_SHORT_5_6_5;
    }
    else {
        fb_format = GL_BGRA;
        fb_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

    configs = driCreateConfigs (fb_format, fb_type,
				depth_bits_array, stencil_bits_array,
				1, back_buffer_modes, 2);
    if (configs == NULL) {
        fprintf( stderr, "[%s:%u] Error creating FBConfig!\n",
                 __func__, __LINE__ );
        return NULL;
    }

    return (const __DRIconfig **) configs;
}

/*
 * API
 */

static void
r600DestroyScreen (__DRIscreenPrivate *sPriv)
{
    screen_t *screen = (screen_t *)sPriv->private;
    if (screen->gartTextures.cpu)
    {
        r600DrmUnMap(screen->gartTextures.cpu, screen->gartTextures.size);
    }

    r600FreeScreen (sPriv->private);
    sPriv->private = NULL;
}

static const __DRIconfig **
r600InitScreen (__DRIscreenPrivate *sPriv)
{
    RADEONDRIPtr driPriv = (RADEONDRIPtr)sPriv->pDevPriv;
    static const char *driver_name = "R600";
    static const __DRIutilversion2 ddx_expected = { 4, 5, 0, 0 };
    static const __DRIversion dri_expected = { 4, 0, 0 };
    static const __DRIversion drm_expected = { 1, 29, 0 };
    static const struct dri_extension no_extensions[] = { { NULL, NULL } };
    
    if (! driCheckDriDdxDrmVersions3 (driver_name,
				      &sPriv->dri_version, &dri_expected,
				      &sPriv->ddx_version, &ddx_expected,
				      &sPriv->drm_version, &drm_expected) ) {
	return NULL;
    }
    /* Even if we don't support any extensions, this has to be called
     * in order to init driDispatchRemapTable
     * (otherwise glNewList dispatch vector gets nuked) */
    driInitExtensions (NULL, no_extensions, GL_FALSE);
    
    if (! (sPriv->private = r600CreateScreen (sPriv)) )
	return NULL;

#if 0
    if (! r600InitDriver (sPriv)) {
	r600DestroyScreen (sPriv);
	return NULL;
    }
#endif
    
    return r600FillInModes (sPriv,
			    driPriv->bpp,
			    (driPriv->bpp == 16) ? 16 : 24,
			    (driPriv->bpp == 16) ? 0  : 8);
}


static GLboolean
r600CreateBuffer (__DRIscreenPrivate *sPriv,
		  __DRIdrawablePrivate *dPriv,
		  const __GLcontextModes *mesaVis,
		  GLboolean isPixmap)
{
    screen_t       *screen    = (screen_t *) sPriv->private;
    const GLboolean swAccum   = mesaVis->accumRedBits > 0;
    const GLboolean swStencil = mesaVis->stencilBits > 0 && mesaVis->depthBits != 24;
    const int       mesaDepth = mesaVis->depthBits == 24 ?
	GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16;
    struct gl_framebuffer *fb;


    if (isPixmap)
	return GL_FALSE;				/* not implemented */

    fb = _mesa_create_framebuffer(mesaVis);
    
    /* front color renderbuffer */
    {
	driRenderbuffer *frontRb
	    = driNewRenderbuffer (GL_RGBA,
				  screen->frontBuffer.cpu, screen->cpp,
				  screen->frontBuffer.cpu - screen->fb.cpu,
				  screen->frontBuffer.pitch, dPriv);
	r600SetSpanFunctions (frontRb, mesaVis);
	_mesa_add_renderbuffer (fb, BUFFER_FRONT_LEFT, &frontRb->Base);
    }
    
    /* back color renderbuffer */
    if (mesaVis->doubleBufferMode) {
	driRenderbuffer *backRb
	    = driNewRenderbuffer (GL_RGBA,
				  screen->backBuffer.cpu, screen->cpp,
				  screen->backBuffer.cpu - screen->fb.cpu,
				  screen->backBuffer.pitch, dPriv);
	r600SetSpanFunctions (backRb, mesaVis);
	_mesa_add_renderbuffer (fb, BUFFER_BACK_LEFT, &backRb->Base);
    }

    /* depth renderbuffer */
    if (mesaVis->depthBits == 16 || mesaVis->depthBits == 24) {
	driRenderbuffer *depthRb
	    = driNewRenderbuffer (mesaDepth,
				  screen->depthBuffer.cpu, screen->cpp,
				  screen->depthBuffer.cpu - screen->fb.cpu,
				  screen->depthBuffer.pitch, dPriv);
	r600SetSpanFunctions (depthRb, mesaVis);
	_mesa_add_renderbuffer (fb, BUFFER_DEPTH, &depthRb->Base);
	depthRb->depthHasSurface = 1;
    }

    /* stencil renderbuffer */
    if (mesaVis->stencilBits > 0 && !swStencil) {
	driRenderbuffer *stencilRb
	    = driNewRenderbuffer (GL_STENCIL_INDEX8_EXT,
				  screen->depthBuffer.cpu, screen->cpp,
				  screen->depthBuffer.cpu - screen->fb.cpu,
				  screen->depthBuffer.pitch, dPriv);
	r600SetSpanFunctions (stencilRb, mesaVis);
	_mesa_add_renderbuffer (fb, BUFFER_STENCIL, &stencilRb->Base);
	stencilRb->depthHasSurface = 1;
    }

    _mesa_add_soft_renderbuffers (fb,
				  GL_FALSE,		/* color */
				  GL_FALSE,		/* depth */
				  swStencil,
				  swAccum,
				  GL_FALSE,		/* alpha */
				  GL_FALSE);		/* aux */

    _mesa_resize_framebuffer (NULL, fb, screen->driScreen->fbWidth, screen->driScreen->fbHeight);

    dPriv->driverPrivate = (void *) fb;
    
    return (fb != NULL);
}


static void
r600DestroyBuffer (__DRIdrawablePrivate *dPriv)
{
    _mesa_unreference_framebuffer ((GLframebuffer **) (&(dPriv->driverPrivate)));
}


/* TODO: temporary in software */
static void
r600CopyBuffer (__DRIdrawablePrivate *dPriv)
{
    context_t *context = (context_t *) dPriv->driContextPriv->driverPrivate;
    screen_t  *screen  = context->screen;
    int i, y;
    unsigned int frontPitch = screen->frontBuffer.pitch * screen->cpp;
    unsigned int backPitch  = screen->backBuffer.pitch  * screen->cpp;

    GLcontext *ctx = ((context_t *) dPriv->driContextPriv->driverPrivate)->ctx;

    DEBUG_FUNC;
    
/*     LOCK_HARDWARE (context); */
    for (i = 0; i < dPriv->numClipRects; i++) 
    {
        unsigned int size  = (dPriv->pClipRects[i].x2 - dPriv->pClipRects[i].x1) * screen->cpp;
        void        *front = screen->frontBuffer.cpu + dPriv->pClipRects[i].x1   * screen->cpp;
        void        *back  = screen->backBuffer.cpu  + dPriv->pClipRects[i].x1   * screen->cpp;
        DEBUGF ("ClipRect %d:%d-%d:%d\n",
                dPriv->pClipRects[i].x1, dPriv->pClipRects[i].y1,
                dPriv->pClipRects[i].x2, dPriv->pClipRects[i].y2);
        for (y = dPriv->pClipRects[i].y1; y < dPriv->pClipRects[i].y2; y++)
        {   
            memcpy (front + frontPitch * y, back + backPitch  * y, size); 
        }
    }
/*     UNLOCK_HARDWARE(radeon); */
}

static void
r600SwapBuffers (__DRIdrawablePrivate *dPriv)
{
    GLcontext *ctx;;
    
    if (! dPriv->driContextPriv || !dPriv->driContextPriv->driverPrivate) {
	_mesa_problem (NULL, "%s: drawable has no context!", __FUNCTION__);
	return;
    }

    ctx = ((context_t *) dPriv->driContextPriv->driverPrivate) ->ctx;

    if (ctx->Visual.doubleBufferMode) {
	_mesa_notifySwapBuffers (ctx);			/* flush pending rendering comands */
	r600CopyBuffer (dPriv);
    }
}


const struct __DriverAPIRec driDriverAPI = {
    .InitScreen      = r600InitScreen,
    .DestroyScreen   = r600DestroyScreen,
    .CreateContext   = r600CreateContext,
    .DestroyContext  = r600DestroyContext,
    .CreateBuffer    = r600CreateBuffer,
    .DestroyBuffer   = r600DestroyBuffer,
    .SwapBuffers     = r600SwapBuffers,
    .MakeCurrent     = r600MakeCurrent,
    .UnbindContext   = r600UnbindContext,
//    .GetSwapInfo     = getSwapInfo,
    .GetDrawableMSC  = driDrawableGetMSC32,
    .WaitForMSC      = driWaitForMSC32,
//    .WaitForSBC      = NULL,
//    .SwapBuffersMSC  = NULL,
//    .CopySubBuffer   = NULL,		/* driCopySubBufferExtension.base */
};

