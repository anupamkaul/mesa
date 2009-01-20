 /**************************************************************************
 *
 * Copyright 2005 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#ifndef VIA_BUFFEROBJ_H
#define VIA_BUFFEROBJ_H

#include "main/mtypes.h"
#include "xf86drm.h"
#include "via_context.h"

struct gl_buffer_object;

/**
 * Via vertex/pixel buffer object, derived from Mesa's gl_buffer_object.
 */
struct via_buffer_object
{
    struct gl_buffer_object Base;
    struct _WsbmBufferObject *buffer;  /* the low-level buffer manager's buffer handle */

    uint32_t placement;
    uint32_t syncFlags;
    struct _WsbmBufferPool *pool;
};

/* Hook the bufferobject implementation into mesa:
 */
void via_bufferobj_init(struct via_context *vmesa);

/* Are the obj->Name tests necessary?  Unfortunately yes, mesa
 * allocates a couple of gl_buffer_object structs statically, and
 * the Name == 0 test is the only way to identify them and avoid
 * casting them erroneously to our structs.
 */
static INLINE struct via_buffer_object *
via_buffer_object(struct gl_buffer_object *obj)
{
    if (obj->Name)
	return containerOf(obj, struct via_buffer_object, Base);
    else
	return NULL;
}

struct _WsbmBufferObject *via_bufferobj_buffer(struct via_buffer_object
					       *via_obj);
#endif
