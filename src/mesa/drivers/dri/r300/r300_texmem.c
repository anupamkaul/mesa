/**************************************************************************

Copyright (C) Tungsten Graphics 2002.  All Rights Reserved.
The Weather Channel, Inc. funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86
license. This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation on the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NON-INFRINGEMENT. IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR THEIR
SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

**************************************************************************/

/**
 * \file
 *
 * \author Gareth Hughes <gareth@valinux.com>
 *
 * \author Kevin E. Martin <martin@valinux.com>
 */

#include <errno.h>

#include "glheader.h"
#include "imports.h"
#include "context.h"
#include "colormac.h"
#include "macros.h"
#include "simple_list.h"
#include "radeon_reg.h"		/* gets definition for usleep */
#include "r300_context.h"
#include "r300_state.h"
#include "r300_cmdbuf.h"
#include "r300_emit.h"
#include "r300_mipmap_tree.h"
#include "radeon_ioctl.h"
#include "r300_tex.h"
#include "r300_ioctl.h"
#include <unistd.h>		/* for usleep() */

#include "r300_mem.h"


/**
 * Upload the texture images associated with texture \a t.  This might
 * require the allocation of texture memory.
 *
 * \param rmesa Context pointer
 * \param t Texture to be uploaded
 * \param face Cube map face to be uploaded.  Zero for non-cube maps.
 */

int r300UploadTexImages(r300ContextPtr rmesa, r300TexObjPtr t, GLuint face)
{
	if (t->image_override)
		return 0;
	if (!t->mt)
		return 0;

	if (RADEON_DEBUG & (DEBUG_TEXTURE | DEBUG_IOCTL)) {
		fprintf(stderr, "%s( %p, %p ) lvls=%d-%d\n", __FUNCTION__,
			(void *)rmesa->radeon.glCtx, t,
			t->mt->firstLevel, t->mt->lastLevel);
	}

	if (RADEON_DEBUG & DEBUG_SYNC) {
		fprintf(stderr, "%s: Syncing\n", __FUNCTION__);
		radeonFinish(rmesa->radeon.glCtx);
	}

	/* Upload any images that are new */
	if (t->dirty_images[face]) {
		int i, numLevels = t->mt->lastLevel - t->mt->firstLevel + 1;
		for (i = 0; i < numLevels; i++) {
			if (t->dirty_images[face] & (1 << (i + t->mt->firstLevel))) {
				r300_miptree_upload_image(t->mt, face, t->mt->firstLevel + i,
					t->base.Image[face][t->mt->firstLevel + i]);
			}
		}
		t->dirty_images[face] = 0;
	}

	if (RADEON_DEBUG & DEBUG_SYNC) {
		fprintf(stderr, "%s: Syncing\n", __FUNCTION__);
		radeonFinish(rmesa->radeon.glCtx);
	}

	return 0;
}
