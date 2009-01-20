/**************************************************************************
 *
 * Copyright 2003-2008 Tungsten Graphics, Inc., Cedar Park, Texas.
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
#include "main/bufferobj.h"
#include "via_buffer_objects.h"
#include "via_context.h"
#include "wsbm_manager.h"

/**
 * There is some duplication between mesa's bufferobjects and our
 * bufmgr buffers.  Both have an integer handle and a hashtable to
 * lookup an opaque structure.  It would be nice if the handles and
 * internal structure where somehow shared.
 */

static void
via_bufferobj_select(struct via_context *vmesa,
		     GLenum target, GLenum usage,
		     struct via_buffer_object *obj)
{
    switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
	/*
	 * All vertex processing done in software. Use the malloc pool
	 * to optimize away TTM maps / unmaps. Otherwise we could've
	 * used the drm pool with cache-coherent memory.
	 */
	obj->pool = vmesa->viaScreen->mallocPool;
	obj->placement = WSBM_PL_FLAG_SYSTEM;
	break;
    default:
	obj->pool = vmesa->viaScreen->bufferPool;
	obj->placement = WSBM_PL_FLAG_VRAM;
	break;
    }
}

static struct gl_buffer_object *
via_bufferobj_alloc(GLcontext * ctx, GLuint name, GLenum target)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_buffer_object *obj = CALLOC_STRUCT(via_buffer_object);
    int ret = 0;

    if (!obj) {
	_mesa_error(ctx, GL_OUT_OF_MEMORY, "glGenBuffers");
	return NULL;
    }

    _mesa_initialize_buffer_object(&obj->Base, name, target);

    via_bufferobj_select(vmesa, target, 0, obj);
    ret = wsbmGenBuffers(obj->pool, 1, &obj->buffer, 64, obj->placement);
    if (ret) {
	_mesa_error(ctx, via_sys_to_gl_err(ret), "glGenBuffers: %s",
		    strerror(-ret));
	free(obj);
	return NULL;
    }

    return &obj->Base;
}

/**
 * Deallocate/free a vertex/pixel buffer object.
 * Called via glDeleteBuffersARB().
 */
static void
via_bufferobj_free(GLcontext * ctx, struct gl_buffer_object *obj)
{
    struct via_buffer_object *via_obj = via_buffer_object(obj);

    assert(via_obj);

    if (via_obj->buffer) {
	wsbmDeleteBuffers(1, &via_obj->buffer);
    }

    _mesa_free(via_obj);
}

/**
 * Allocate space for and store data in a buffer object.  Any data that was
 * previously stored in the buffer object is lost.  If data is NULL,
 * memory will be allocated, but no copy will occur.
 * Called via glBufferDataARB().
 */
static void
via_bufferobj_data(GLcontext * ctx,
		   GLenum target,
		   GLsizeiptrARB size,
		   const GLvoid * data,
		   GLenum usage, struct gl_buffer_object *obj)
{
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    struct via_buffer_object *via_obj = via_buffer_object(obj);
    int ret;

    via_obj->Base.Size = size;
    via_obj->Base.Usage = usage;

    via_bufferobj_select(vmesa, target, usage, via_obj);
    ret = wsbmBOData(via_obj->buffer, size, data, via_obj->pool,
		     via_obj->placement);
    if (ret)
	_mesa_error(ctx, via_sys_to_gl_err(ret), "glBufferDataARB: %s",
		    strerror(-ret));
}

/**
 * Replace data in a subrange of buffer object.  If the data range
 * specified by size + offset extends beyond the end of the buffer or
 * if data is NULL, no copy is performed.
 * Called via glBufferSubDataARB().
 */
static void
via_bufferobj_subdata(GLcontext * ctx,
		      GLenum target,
		      GLintptrARB offset,
		      GLsizeiptrARB size,
		      const GLvoid * data, struct gl_buffer_object *obj)
{
    struct via_buffer_object *via_obj = via_buffer_object(obj);
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    int ret;

    assert(via_obj);

    if (wsbmBOOnList(via_obj->buffer))
	VIA_FLUSH_DMA(vmesa);

    ret = wsbmBOSubData(via_obj->buffer, offset, size, data, NULL);
    if (ret)
	_mesa_error(ctx, via_sys_to_gl_err(ret), "glBufferSubDataARB: %s",
		    strerror(-ret));

}

/**
 * Called via glGetBufferSubDataARB().
 */
static void
via_bufferobj_get_subdata(GLcontext * ctx,
			  GLenum target,
			  GLintptrARB offset,
			  GLsizeiptrARB size,
			  GLvoid * data, struct gl_buffer_object *obj)
{
    struct via_buffer_object *via_obj = via_buffer_object(obj);
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    int ret;

    assert(via_obj);

    if (wsbmBOOnList(via_obj->buffer))
	VIA_FLUSH_DMA(vmesa);

    ret = wsbmBOGetSubData(via_obj->buffer, offset, size, data);
    if (ret)
	_mesa_error(ctx, via_sys_to_gl_err(ret), "glBufferGetSubDataARB: %s",
		    strerror(-ret));
}

/**
 * Called via glMapBufferARB().
 */
static void *
via_bufferobj_map(GLcontext * ctx,
		  GLenum target, GLenum access, struct gl_buffer_object *obj)
{
    struct via_buffer_object *via_obj = via_buffer_object(obj);
    struct via_context *vmesa = VIA_CONTEXT(ctx);
    GLuint flags;
    int ret;

    /* XXX: Translate access to flags arg below:
     */
    assert(via_obj);

    switch (access) {
    case GL_WRITE_ONLY:
	flags = WSBM_SYNCCPU_WRITE;
	break;

    case GL_READ_ONLY:

	flags = WSBM_SYNCCPU_READ;
	break;

    case GL_READ_WRITE:
    default:
	flags = WSBM_SYNCCPU_READ | WSBM_SYNCCPU_WRITE;
	break;
    }

    if (wsbmBOOnList(via_obj->buffer))
	VIA_FLUSH_DMA(vmesa);

    ret = wsbmBOSyncForCpu(via_obj->buffer, flags);

    if (ret)
	goto out_err0;

    obj->Pointer = wsbmBOMap(via_obj->buffer, flags);
    if (!obj->Pointer)
	goto out_err1;

    via_obj->syncFlags = flags;

    return obj->Pointer;

  out_err1:
    wsbmBOReleaseFromCpu(via_obj->buffer, flags);
  out_err0:
    _mesa_error(ctx, GL_OUT_OF_MEMORY, "glBufferMap");
    return NULL;
}

/**
 * Called via glMapBufferARB().
 */
static GLboolean
via_bufferobj_unmap(GLcontext * ctx,
		    GLenum target, struct gl_buffer_object *obj)
{
    struct via_buffer_object *via_obj = via_buffer_object(obj);

    assert(via_obj);
    if (!obj->Pointer)
	return GL_FALSE;

    wsbmBOUnmap(via_obj->buffer);
    wsbmBOReleaseFromCpu(via_obj->buffer, via_obj->syncFlags);

    via_obj->syncFlags = 0;
    obj->Pointer = NULL;
    return GL_TRUE;
}

struct _WsbmBufferObject *
via_bufferobj_buffer(struct via_buffer_object *via_obj)
{
    return via_obj->buffer;
}

void
via_bufferobj_init(struct via_context *vmesa)
{
    GLcontext *ctx = vmesa->glCtx;

    ctx->Driver.NewBufferObject = via_bufferobj_alloc;
    ctx->Driver.DeleteBuffer = via_bufferobj_free;
    ctx->Driver.BufferData = via_bufferobj_data;
    ctx->Driver.BufferSubData = via_bufferobj_subdata;
    ctx->Driver.GetBufferSubData = via_bufferobj_get_subdata;
    ctx->Driver.MapBuffer = via_bufferobj_map;
    ctx->Driver.UnmapBuffer = via_bufferobj_unmap;
}
