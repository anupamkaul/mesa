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

radeon_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa);

#endif
