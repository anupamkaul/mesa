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

#define DRM_BO_MEM_DMA DRM_BO_MEM_PRIV0 /** Use for transient buffers (texture upload, vertex buffers...) */
#define DRM_BO_MEM_CMDBUF DRM_BO_MEM_PRIV1 /** Use for command buffers */

radeon_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa);

#endif
