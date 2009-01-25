/**************************************************************************
 *
 * Copyright 2006, 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "main/imports.h"
#include "main/mtypes.h"
#include "main/fbobject.h"
#include "main/framebuffer.h"
#include "main/renderbuffer.h"
#include "main/texformat.h"
#include "main/texrender.h"

#include "wsbm_manager.h"

#include "via_fbo.h"
#include "via_screen.h"
#include "via_span.h"
#include "via_3d_reg.h"
#include "via_context.h"
#include "via_depthstencil.h"
#include "via_ioctl.h"
#include "via_state.h"
#include "via_tex.h"

static __inline__ unsigned
buffer_pitch(unsigned width)
{
    return (width + 0x0f) & ~0x0f;
}

static void
via_set_depth_24_8(struct gl_renderbuffer *rb)
{
    rb->_ActualFormat = GL_DEPTH24_STENCIL8_EXT;
    rb->DataType = GL_UNSIGNED_INT_24_8_EXT;
    rb->DepthBits = 24;
}

/**
 * Called via glRenderbufferStorageEXT() to set the format and allocate
 * storage for a user-created renderbuffer.
 */

static GLboolean
via_alloc_renderbuffer_storage(GLcontext * ctx, struct gl_renderbuffer *rb,
			       GLenum internalFormat,
			       GLuint width, GLuint height)
{
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    GLboolean softwareBuffer = GL_FALSE;
    GLuint cpp, hwformat = 0;
    struct _WsbmBufferPool *pool;
    uint64_t flags;
    int ret;

    viarb->texFormat = VIA_FMT_ERROR;

    /*
     * User-created depth renderbuffers must use the
     * depth 24 stencil 8 format since we might pair
     * depth and stencil.
     */

    switch (internalFormat) {
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
	rb->_ActualFormat = GL_RGB5;
	rb->DataType = GL_UNSIGNED_BYTE;
	rb->RedBits = 5;
	rb->GreenBits = 6;
	rb->BlueBits = 5;
	hwformat = HC_HDBFM_RGB565;
	viarb->texFormat = HC_HTXnFM_RGB565;
	cpp = 2;
	break;
    case GL_RGB:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
	rb->_ActualFormat = GL_RGBA8;
	rb->DataType = GL_UNSIGNED_BYTE;
	rb->RedBits = 8;
	rb->GreenBits = 8;
	rb->BlueBits = 8;
	rb->AlphaBits = 8;
	hwformat = HC_HDBFM_ARGB8888;
	viarb->texFormat = HC_HTXnFM_ARGB8888;
	cpp = 4;
	break;
    case GL_STENCIL_INDEX:
    case GL_STENCIL_INDEX1_EXT:
    case GL_STENCIL_INDEX4_EXT:
    case GL_STENCIL_INDEX8_EXT:
    case GL_STENCIL_INDEX16_EXT:
	/* alloc a depth+stencil buffer */
	rb->_ActualFormat = GL_DEPTH24_STENCIL8_EXT;
	rb->DataType = GL_UNSIGNED_INT_24_8_EXT;
	rb->StencilBits = 8;
	cpp = 4;
	break;
    case GL_DEPTH_COMPONENT16:
	if (rb->Name == 0) {
	    rb->_ActualFormat = GL_DEPTH_COMPONENT16;
	    rb->DataType = GL_UNSIGNED_SHORT;
	    rb->DepthBits = 16;
	    cpp = 2;
	} else {
	    via_set_depth_24_8(rb);
	    cpp = 4;
	}
	break;
    case GL_DEPTH_COMPONENT32:
	if (rb->Name == 0) {
	    rb->_ActualFormat = GL_DEPTH_COMPONENT32;
	    rb->DataType = GL_UNSIGNED_INT;
	    rb->DepthBits = 32;
	} else {
	    via_set_depth_24_8(rb);
	}
	cpp = 4;
	break;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT24:
	via_set_depth_24_8(rb);
	cpp = 4;
	break;
    case GL_DEPTH_STENCIL_EXT:
    case GL_DEPTH24_STENCIL8_EXT:
	via_set_depth_24_8(rb);
	rb->StencilBits = 8;
	cpp = 4;
	break;
    default:
	_mesa_problem(ctx,
		      "Unexpected format in via_alloc_renderbuffer_storage");
	return GL_FALSE;
    }

    /*
     * Fixme, flush might be needed here, but this function
     * is usually called from within a locked region.
     */

    rb->Width = width;
    rb->Height = height;
    viarb->pitch = (buffer_pitch(width) + VIA_MAX_DRAWXOFF) * cpp;

    viarb->hwformat = hwformat;
    viarb->bpp = cpp << 3;
    viarb->size = viarb->pitch * height;

    pool = (softwareBuffer) ? viarb->viaScreen->mallocPool :
	viarb->viaScreen->bufferPool;
    flags = WSBM_PL_FLAG_VRAM;

    /*
     * WaitIdle needed to avoid too much fragmentation of the VRAM heap.
     */

    wsbmBOWaitIdle(viarb->buf, 0);
    ret = wsbmBOData(viarb->buf, viarb->size, NULL, pool, flags);

    viarb->origAdd = 0;
    viarb->origMapAdd = 0;

    via_set_span_functions(rb);

    return (ret) ? GL_FALSE : GL_TRUE;
}

static GLboolean
via_alloc_window_storage(GLcontext * ctx, struct gl_renderbuffer *rb,
			 GLenum internalFormat, GLuint width, GLuint height)
{
    struct drm_via_sarea *saPriv;
    struct via_renderbuffer *viarb = via_renderbuffer(rb);
    int ret;
    int wasLocked = 0;

    struct _viaScreenPrivate *viaScreen = viarb->viaScreen;
    __DRIscreenPrivate *psp = viaScreen->driScrnPriv;
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    volatile struct drm_via_scanout *front;

    ASSERT(rb->Name == 0);

    viarb->texFormat = VIA_FMT_ERROR;
    switch (internalFormat) {
    case GL_RGB5:
	rb->_ActualFormat = GL_RGB5;
	rb->DataType = GL_UNSIGNED_BYTE;
	rb->RedBits = 5;
	rb->GreenBits = 6;
	rb->BlueBits = 5;
	viarb->bpp = 16;
	viarb->texFormat = HC_HTXnFM_RGB565;
	break;
    case GL_RGBA8:
	rb->_ActualFormat = GL_RGBA8;
	rb->DataType = GL_UNSIGNED_BYTE;
	rb->RedBits = 8;
	rb->GreenBits = 8;
	rb->BlueBits = 8;
	rb->AlphaBits = 8;
	viarb->bpp = 32;
	viarb->texFormat = HC_HTXnFM_ARGB8888;
	break;
    default:
	_mesa_problem(ctx, "Unexpected format in via_alloc_window_storage");
	return GL_FALSE;
    }

    wasLocked = vmesa->isLocked;
    if (!wasLocked) {
	LOCK_HARDWARE(vmesa);
    }

    saPriv = (struct drm_via_sarea *)
	(((GLubyte *) psp->pSAREA) + viaScreen->sareaPrivOffset);

    front = &saPriv->scanouts[0];

    rb->Width = width;
    rb->Height = height;
    viarb->pitch = front->stride;
    viarb->size = front->stride * front->height;

    if (!wasLocked) {
	UNLOCK_HARDWARE(vmesa);
    }

    viarb->map = NULL;
    via_set_span_functions(rb);

    ret = wsbmBOSetReferenced(viarb->buf, front->handle);

    return (ret) ? GL_FALSE : GL_TRUE;
}

static void
via_delete_renderbuffer(struct gl_renderbuffer *rb)
{
    GET_CURRENT_CONTEXT(ctx);
    struct via_renderbuffer *viarb = (struct via_renderbuffer *)rb;
    struct gl_texture_image *texImage;

    if (viarb->PairedStencil || viarb->PairedDepth) {
	via_unpair_depth_stencil(ctx, viarb);
    }

    texImage = &viarb->viaImage.image;

    if (texImage->ImageOffsets) {
	free(texImage->ImageOffsets);
	texImage->ImageOffsets = NULL;
    }

    wsbmBOUnreference(&viarb->buf);
    FREE(viarb);
}

struct via_renderbuffer *
via_create_renderbuffer(GLenum intFormat, __DRIscreenPrivate * sPriv,
			int common)
{
    struct _viaScreenPrivate *viaScreen = sPriv->private;
    struct via_renderbuffer *vrb;
    struct gl_renderbuffer *rb;
    GLenum baseFormat;
    GLuint name = 0, hwformat = 0;
    int ret;

    switch (intFormat) {
    case GL_RGB5:
	baseFormat = GL_RGBA;
	hwformat = HC_HDBFM_RGB565;
	break;
    case GL_RGBA8:
	baseFormat = GL_RGBA;
	hwformat = HC_HDBFM_ARGB8888;
	break;
    case GL_DEPTH_COMPONENT16:
	baseFormat = GL_DEPTH_COMPONENT;
	break;
    case GL_DEPTH_COMPONENT24:
	baseFormat = GL_DEPTH_COMPONENT;
	break;
    case GL_DEPTH24_STENCIL8_EXT:
	baseFormat = GL_DEPTH_STENCIL_EXT;
	break;
    default:
	_mesa_problem(NULL,
		      "Unexpected intFormat in via_create_renderbuffer.");
	return NULL;
    }

    vrb = CALLOC_STRUCT(via_renderbuffer);
    if (!vrb) {
	_mesa_error(NULL, GL_OUT_OF_MEMORY, "Creating renderbuffer.");
	return NULL;
    }

    rb = &vrb->Base;
    _mesa_init_renderbuffer(rb, name);

    vrb->viaScreen = viaScreen;
    vrb->map = NULL;
    vrb->size = 0;

    ret = wsbmGenBuffers(viaScreen->bufferPool, 1,
			 &vrb->buf, 32, WSBM_PL_FLAG_VRAM);
    if (ret)
	goto out_err;

    rb->ClassID = VIA_RB_CLASS;
    rb->InternalFormat = intFormat;
    rb->_BaseFormat = baseFormat;
    vrb->hwformat = hwformat;
    rb->Delete = via_delete_renderbuffer;
    rb->AllocStorage = (common) ?
	via_alloc_window_storage : via_alloc_renderbuffer_storage;
    vrb->isSharedFrontBuffer = (common) ? GL_TRUE : GL_FALSE;

    return vrb;

  out_err:
    free(vrb);
    return NULL;
}

static GLboolean
via_nop_alloc_storage(GLcontext * ctx, struct gl_renderbuffer *rb,
		      GLenum internalFormat, GLuint width, GLuint height)
{
    _mesa_problem(ctx, "via_nop_alloc_storage should never be called.");
    return GL_FALSE;
}

struct via_renderbuffer *
via_get_renderbuffer(struct gl_framebuffer *fb, GLuint attIndex)
{
    return via_renderbuffer(fb->Attachment[attIndex].Renderbuffer);
}

static void
via_destroy_framebuffer(struct gl_framebuffer *buffer)
{
    struct via_framebuffer *viafb = via_framebuffer(buffer);

    if (!viafb)
	return;

    if (viafb->pFrontClipRects &&
	(viafb->pFrontClipRects != &viafb->allClipRect))
	free(viafb->pFrontClipRects);

    _mesa_destroy_framebuffer(buffer);
}

static struct gl_framebuffer *
via_new_framebuffer(GLcontext * ctx, GLuint name)
{
    struct via_framebuffer *viafb;

    (void)ctx;
    assert(name != 0);
    viafb = CALLOC_STRUCT(via_framebuffer);
    if (viafb) {
	viafb->Base.Name = name;
	viafb->Base.RefCount = 1;
	viafb->Base._NumColorDrawBuffers = 1;
	viafb->Base.ColorDrawBuffer[0] = GL_COLOR_ATTACHMENT0_EXT;
	viafb->Base._ColorDrawBufferIndexes[0] = BUFFER_COLOR0;
	viafb->Base.ColorReadBuffer = GL_COLOR_ATTACHMENT0_EXT;
	viafb->Base._ColorReadBufferIndex = BUFFER_COLOR0;
	viafb->Base.Delete = via_destroy_framebuffer;
    }
    return &viafb->Base;
}

/**
 * Called via glBindFramebufferEXT().
 */
static void
via_bind_framebuffer(GLcontext * ctx, GLenum target,
		     struct gl_framebuffer *fb, struct gl_framebuffer *fbread)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);

    if (target == GL_FRAMEBUFFER_EXT || target == GL_DRAW_FRAMEBUFFER_EXT) {
	if (fb->Name == 0) {
	    vmesa->lostLock = 1;
	    vmesa->firstDrawAfterSwap = GL_TRUE;
	}
	ctx->Driver.DrawBuffer(ctx, fb->ColorDrawBuffer[0]);
	ctx->Driver.ReadBuffer(ctx, fbread->ColorReadBuffer);
    }
}

/**
 * Called via glFramebufferRenderbufferEXT().
 */
static void
via_framebuffer_renderbuffer(GLcontext * ctx,
			     struct gl_framebuffer *fb,
			     GLenum attachment, struct gl_renderbuffer *rb)
{
    if (VIA_DEBUG & DEBUG_FBO)
	fprintf(stderr, "VIA FramebufferRenderbuffer %u %u\n", fb->Name,
		rb ? rb->Name : 0);

    _mesa_framebuffer_renderbuffer(ctx, fb, attachment, rb);

    ctx->Driver.DrawBuffer(ctx, fb->ColorDrawBuffer[0]);
    ctx->Driver.ReadBuffer(ctx, fb->ColorReadBuffer);
}

/**
 * Create a new renderbuffer object.
 * Typically called via glBindRenderbufferEXT().
 */
static struct gl_renderbuffer *
via_new_renderbuffer(GLcontext * ctx, GLuint name)
{
    struct via_renderbuffer *viarb;
    struct gl_renderbuffer *rb;

    /*
     * Note: According to BrianP, we should always have a valid
     * ctx pointer here.
     */

    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct _viaScreenPrivate *viaScreen = vmesa->viaScreen;
    int ret;

    viarb = CALLOC_STRUCT(via_renderbuffer);
    if (!viarb) {
	_mesa_error(ctx, GL_OUT_OF_MEMORY, "creating renderbuffer");
	return NULL;
    }
    viarb->viaScreen = viaScreen;

    rb = &viarb->Base;
    _mesa_init_renderbuffer(rb, name);

    ret = wsbmGenBuffers(viaScreen->bufferPool, 1,
			 &viarb->buf, 32, WSBM_PL_FLAG_VRAM);
    if (ret)
	goto out_err;

    rb->ClassID = VIA_RB_CLASS;
    rb->Delete = via_delete_renderbuffer;
    rb->AllocStorage = via_alloc_renderbuffer_storage;

    return &viarb->Base;

  out_err:
    free(viarb);
    return NULL;
}

static struct via_renderbuffer *
via_alloc_texture_wrapper(void)
{
    struct gl_renderbuffer *rb;
    struct via_renderbuffer *viarb;
    const GLuint name = ~0;

    ASSERT(att->Type == GL_TEXTURE);
    ASSERT(att->Renderbuffer == NULL);

    viarb = CALLOC_STRUCT(via_renderbuffer);
    if (!viarb) {
	return NULL;
    }

    rb = &viarb->Base;

    /* init base gl_renderbuffer fields */
    _mesa_init_renderbuffer(rb, name);

    rb->ClassID = VIA_RB_CLASS;
    rb->Delete = via_delete_renderbuffer;
    rb->AllocStorage = via_nop_alloc_storage;
    via_set_texture_span_functions(&viarb->Base);

    viarb->Base.AllocStorage = via_nop_alloc_storage;
    return viarb;
}

/**
 * When glFramebufferTexture[123]D is called this function sets up the
 * gl_renderbuffer wrapper around the texture image.
 * This will have the region info needed for hardware rendering.
 */
static void
via_update_texture_wrapper(GLcontext * ctx,
			   struct via_renderbuffer *viarb,
			   struct gl_texture_image *texImage)
{
    struct via_texture_image *viaImage = (struct via_texture_image *)texImage;
    struct gl_texture_image *localImage;
    struct via_texture_object *viaObj =
	(struct via_texture_object *)texImage->TexObject;
    GLuint *offsetCopy;

    if (texImage->DriverData == NULL) {
	/*
	 * FIXME: need to fallback to software here.
	 */
    }

    viaObj->imagesInVRAM = GL_TRUE;

    /*
     * Need to copy the teximage since it's not refcounted.
     */

    localImage = &viarb->viaImage.image;

    if (localImage->ImageOffsets) {
	free(localImage->ImageOffsets);
	localImage->ImageOffsets = NULL;
    }

    viarb->viaImage = *viaImage;
    viarb->viaImage.buf = NULL;

    localImage->TexObject = NULL;
    localImage->Data = NULL;
    localImage->DriverData = NULL;

    offsetCopy = malloc(sizeof(*offsetCopy) * localImage->Depth);

    if (!offsetCopy) {
	_mesa_error(ctx, GL_OUT_OF_MEMORY, "wrap_texture");
	return;
    }

    memcpy(offsetCopy, localImage->ImageOffsets,
	   sizeof(*offsetCopy) * localImage->Depth);

    localImage->ImageOffsets = offsetCopy;

    viarb->hwformat = viaImage->dstHwFormat;
    viarb->texFormat = viaImage->texHwFormat;

    if (VIA_DEBUG & DEBUG_FBO) {
	if (viarb->hwformat != VIA_FMT_ERROR)
	    fprintf(stderr, "Render to texture OK.\n");
	else
	    fprintf(stderr, "Render to texture Unsupported format.\n");
    }

    viarb->Base.InternalFormat = texImage->InternalFormat;
    viarb->Base.DataType = GL_UNSIGNED_BYTE;	/* FBO XXX fix */

    /* FIXME: may need more special cases here */
    if (texImage->TexFormat->MesaFormat == MESA_FORMAT_Z24_S8) {
	viarb->Base._ActualFormat = GL_DEPTH24_STENCIL8_EXT;
	viarb->Base.DataType = GL_UNSIGNED_INT_24_8_EXT;
    } else if (texImage->TexFormat->MesaFormat == MESA_FORMAT_Z16) {
	viarb->Base._ActualFormat = GL_DEPTH_COMPONENT;
	viarb->Base.DataType = GL_UNSIGNED_SHORT;
    } else if (texImage->TexFormat->MesaFormat == MESA_FORMAT_Z32) {
	viarb->Base._ActualFormat = GL_DEPTH_COMPONENT;
	viarb->Base.DataType = GL_UNSIGNED_INT;
    } else {
	viarb->Base._ActualFormat = texImage->InternalFormat;
	viarb->Base.DataType = CHAN_TYPE;
    }
    viarb->Base._BaseFormat = texImage->TexFormat->BaseFormat;

    viarb->Base.Width = texImage->Width;
    viarb->Base.Height = texImage->Height;
    viarb->Base.RedBits = texImage->TexFormat->RedBits;
    viarb->Base.GreenBits = texImage->TexFormat->GreenBits;
    viarb->Base.BlueBits = texImage->TexFormat->BlueBits;
    viarb->Base.AlphaBits = texImage->TexFormat->AlphaBits;
    viarb->Base.DepthBits = texImage->TexFormat->DepthBits;

    viarb->bpp = 8 * texImage->TexFormat->TexelBytes;
    viarb->pitch = via_teximage_stride(viaImage);
    viarb->size = texImage->IsCompressed ? texImage->CompressedSize :
	viarb->pitch * texImage->Height;
    viarb->Store = texImage->TexFormat->StoreTexel;

    if (viarb->buf != viaImage->buf) {
	wsbmBOUnreference(&viarb->buf);
	viarb->buf = wsbmBOReference(viaImage->buf);
    }
}

/**
 * Called by glFramebufferTexture[123]DEXT() (and other places) to
 * prepare for rendering into texture memory.  This might be called
 * many times to choose different texture levels, cube faces, etc
 * before via_finish_render_texture() is ever called.
 */
static void
via_render_texture(GLcontext * ctx,
		   struct gl_framebuffer *fb,
		   struct gl_renderbuffer_attachment *att)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct gl_texture_image *newImage
	= att->Texture->Image[att->CubeMapFace][att->TextureLevel];
    struct via_renderbuffer *viarb = via_renderbuffer(att->Renderbuffer);
    struct via_texture_image *via_image;

    (void)fb;
    ASSERT(newImage);

    if (!viarb) {
	viarb = via_alloc_texture_wrapper();

	if (!viarb)
	    return;
    }

    /* bind the wrapper to the attachment point */
    _mesa_reference_renderbuffer(&att->Renderbuffer, &viarb->Base);
    via_update_texture_wrapper(ctx, viarb, newImage);

    if (VIA_DEBUG & DEBUG_FBO)
	fprintf(stderr,
		"Begin render texture tid %lx tex=%u w=%d h=%d refcount=%d\n",
		_glthread_GetID(), att->Texture->Name, newImage->Width,
		newImage->Height, viarb->Base.RefCount);

    /* point the renderbufer's region to the texture image region */
    via_image = (struct via_texture_image *)newImage;

    ctx->Driver.DrawBuffer(ctx, fb->ColorDrawBuffer[0]);
    ctx->Driver.ReadBuffer(ctx, fb->ColorReadBuffer);

    /* Re-calculate viewport etc */
    vmesa->newState |= _NEW_BUFFERS;
}

/**
 * Called by Mesa when rendering to a texture is done.
 */
static void
via_finish_render_texture(GLcontext * ctx,
			  struct gl_renderbuffer_attachment *att)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);

    struct via_renderbuffer *viarb = via_renderbuffer(att->Renderbuffer);

    if (VIA_DEBUG & DEBUG_FBO)
	fprintf(stderr, "End render texture (tid %lx) tex %u\n",
		_glthread_GetID(), att->Texture->Name);

    if (viarb) {
	/* just release the region */
	wsbmBOUnreference(&viarb->buf);
	viarb->buf = NULL;
    } else if (att->Renderbuffer) {
	/* software fallback */
	_mesa_finish_render_texture(ctx, att);
    }

    /* Re-calculate viewport etc */
    vmesa->newState |= _NEW_BUFFERS;
}

void
via_fbo_init(struct via_context *vmesa)
{
    GLcontext *ctx = vmesa->glCtx;

    ctx->Driver.NewFramebuffer = via_new_framebuffer;
    ctx->Driver.NewRenderbuffer = via_new_renderbuffer;
    ctx->Driver.BindFramebuffer = via_bind_framebuffer;
    ctx->Driver.FramebufferRenderbuffer = via_framebuffer_renderbuffer;
    ctx->Driver.RenderTexture = via_render_texture;
    ctx->Driver.FinishRenderTexture = via_finish_render_texture;
}
