#ifndef __R300_MEM_H__
#define __R300_MEM_H__

#include "glheader.h"
#include "dri_bufmgr.h"

#include "r300_context.h"

struct _radeon_bufmgr {
	dri_bufmgr base;

	/**
	 * Call this after writing command buffer instructions that use
	 * the given buffer. Marks the buffer as pending on hardware.
	 */
	void (*bo_use)(dri_bo* buf);
};

/* Note: The following flags should probably be ultimately eliminated,
 * or replaced by something else.
 */
#define DRM_BO_MEM_DMA (1 << 27) /** Use for transient buffers (texture upload, vertex buffers...) */
#define DRM_BO_MEM_CMDBUF (1 << 28) /** Use for command buffers */

#define DRM_RELOC_BLITTER (1 << 23) /** Offset overwrites lower 22 bits (used with blit packet3) */

radeon_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa);

#endif
