#ifndef NOUVEAU_WINSYS_H
#define NOUVEAU_WINSYS_H

#include <stdint.h>
#include "pipe/p_defines.h"

#include "nouveau/nouveau_bo.h"
#include "nouveau/nouveau_channel.h"
#include "nouveau/nouveau_class.h"
#include "nouveau/nouveau_device.h"
#include "nouveau/nouveau_grobj.h"
#include "nouveau/nouveau_notifier.h"
#include "nouveau/nouveau_resource.h"
#include "nouveau/nouveau_pushbuf.h"

#define NOUVEAU_CAP_HW_VTXBUF (0xbeef0000)
#define NOUVEAU_CAP_HW_IDXBUF (0xbeef0001)

/* These were poorly defined flags at the pipe interface previously.
 * They have been removed, but nouveau can continue to use its own
 * versions internally:
 */
#define NOUVEAU_BUFFER_USAGE_PIXEL           0x1
#define NOUVEAU_BUFFER_USAGE_VERTEX          0x2
#define NOUVEAU_BUFFER_USAGE_CPU_READ_WRITE  0x4
#define NOUVEAU_BUFFER_USAGE_GPU_WRITE       0x8
#define NOUVEAU_BUFFER_USAGE_DISCARD         0x10
#define NOUVEAU_BUFFER_USAGE_TEXTURE         0x20
#define NOUVEAU_BUFFER_USAGE_ZETA            0x40
#define NOUVEAU_BUFFER_USAGE_TRANSFER        0x80

static inline uint32_t
nouveau_screen_transfer_flags(unsigned pipe)
{
	uint32_t flags = 0;

	if (pipe & PIPE_TRANSFER_READ)
		flags |= NOUVEAU_BO_RD;
	if (pipe & PIPE_TRANSFER_WRITE)
		flags |= NOUVEAU_BO_WR;
	if (pipe & PIPE_TRANSFER_DISCARD)
		flags |= NOUVEAU_BO_INVAL;
	if (pipe & PIPE_TRANSFER_DONTBLOCK)
		flags |= NOUVEAU_BO_NOWAIT;
	else
	if (pipe & PIPE_TRANSFER_UNSYNCHRONIZED)
		flags |= NOUVEAU_BO_NOSYNC;

	return flags;
}

static INLINE unsigned 
nouveau_screen_bind_flags( unsigned bind )
{
	unsigned buf_usage = 0;

	if (bind & PIPE_BIND_VERTEX_BUFFER)
		buf_usage |= NOUVEAU_BUFFER_USAGE_VERTEX;

	if (bind & PIPE_BIND_INDEX_BUFFER)
		buf_usage |= PIPE_BIND_INDEX_BUFFER;

	if (bind & (PIPE_BIND_RENDER_TARGET |
		    PIPE_BIND_DEPTH_STENCIL |
		    PIPE_BIND_BLIT_SOURCE |
		    PIPE_BIND_BLIT_DESTINATION |
		    PIPE_BIND_SCANOUT |
		    PIPE_BIND_DISPLAY_TARGET |
		    PIPE_BIND_SAMPLER_VIEW))
		buf_usage |= NOUVEAU_BUFFER_USAGE_PIXEL;

	if (bind & (PIPE_BIND_TRANSFER_WRITE |
		    PIPE_BIND_TRANSFER_READ))
		buf_usage |= NOUVEAU_BUFFER_USAGE_CPU_READ_WRITE;

	/* Not sure where these two came from:
	 */
	if (0)
		buf_usage |= NOUVEAU_BUFFER_USAGE_TRANSFER;

	if (0)
		buf_usage |= NOUVEAU_BUFFER_USAGE_ZETA;

	return buf_usage;
}


extern struct pipe_screen *
nvfx_screen_create(struct pipe_winsys *ws, struct nouveau_device *);

extern struct pipe_screen *
nv50_screen_create(struct pipe_winsys *ws, struct nouveau_device *);

#endif
