/**************************************************************************
 *
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

#include "wsbm_driver.h"
#include "wsbm_util.h"
#include "via_ioctl.h"

/*
 * Allocate a validate list node. On the list of drm buffers, which is
 * identified by type_id == 0, we allocate a derived item which
 * also contains a drm validate arg, which means we can start to fill
 * this in immediately.
 */

static struct _ValidateNode *
vn_alloc(struct _WsbmVNodeFuncs *func, int type_id)
{
    if (type_id == 0) {
	struct _ViaDrmValidateNode *vNode = malloc(sizeof(*vNode));

	vNode->base.func = func;
	vNode->base.type_id = 0;
	return &vNode->base;
    } else {
	struct _ValidateNode *node = malloc(sizeof(*node));

	node->func = func;
	node->type_id = 1;
	return node;
    }
}

/*
 * Free an allocated validate list node.
 */

static void
vn_free(struct _ValidateNode *node)
{
    if (node->type_id == 0)
	free(containerOf(node, struct _ViaDrmValidateNode, base));

    else
	free(node);

}

/*
 * Clear the private part of the validate list node. This happens when
 * the list node is newly allocated or is being reused. Since we only have
 * a private part when node->type_id == 0 we only care to clear in that
 * case. We want to clear the drm ioctl argument.
 */

static void
vn_clear(struct _ValidateNode *node)
{
    if (node->type_id == 0) {
	struct _ViaDrmValidateNode *vNode =
	    containerOf(node, struct _ViaDrmValidateNode, base);

	memset(&vNode->val_arg.d.req, 0, sizeof(vNode->val_arg.d.req));
    }
}

static struct _WsbmVNodeFuncs viaVNode = {
    .alloc = vn_alloc,
    .free = vn_free,
    .clear = vn_clear,
};

struct _WsbmVNodeFuncs *
viaVNodeFuncs(void)
{
    return &viaVNode;
}
