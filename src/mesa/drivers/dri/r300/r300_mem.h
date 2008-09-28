#ifndef __R300_MEM_H__
#define __R300_MEM_H__

#include "main/glheader.h"
#include "radeon_dri_bufmgr.h"

#include "r300_context.h"


/* Note: The following flags should probably be ultimately eliminated,
 * or replaced by something else.
 */
#define DRM_BO_MEM_DMA (1 << 27) /** Use for transient buffers (texture upload, vertex buffers...) */
#define DRM_BO_MEM_CMDBUF (1 << 28) /** Use for command buffers */

#define DRM_RELOC_BLITTER (1 << 23) /** Offset overwrites lower 22 bits (used with blit packet3) */
#define DRM_RELOC_TXOFFSET (1 << 24) /** Offset overwrites everything but low bits (used for texture offsets) */

dri_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa);
void radeonBufmgrContendedLockTake(dri_bufmgr* bufmgr_ctx);

#endif
