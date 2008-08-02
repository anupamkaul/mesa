/*
 * Copyright (C) 2005 Aapo Tahkola.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * \file
 * Simulate a real memory manager for R300 in the old-style scheme.
 *
 * NOTE: Right now, this is DMA-only and really only a skeleton of a true bufmgr.
 *
 * \author Aapo Tahkola <aet@rasterburn.org>
 */

#include "r300_mem.h"

#include "radeon_ioctl.h"
#include "r300_cmdbuf.h"

typedef struct _radeon_bufmgr_classic radeon_bufmgr_classic;
typedef struct _radeon_bo_classic radeon_bo_classic;

struct _radeon_bufmgr_classic {
	radeon_bufmgr base;
	r300ContextPtr rmesa;

	radeon_bo_classic *buffers; /** Unsorted linked list of all buffer objects */

	radeon_bo_classic *pending; /** Age-sorted linked list of pending buffer objects */
	radeon_bo_classic **pending_tail;
};

struct _radeon_bo_classic {
	dri_bo base;

	radeon_bo_classic *next; /** Unsorted linked list of all buffer objects */
	radeon_bo_classic **pprev;

	/**
	 * Number of software references to this buffer.
	 * A buffer is freed automatically as soon as its reference count reaches 0
	 * *and* it is no longer pending.
	 */
	unsigned int refcount;
	unsigned int mapcount; /** mmap count; mutually exclusive to being pending */

	unsigned int pending:1;
	radeon_bo_classic *pending_next; /** Age-sorted linked list of pending buffer objects */
	radeon_bo_classic **pending_pprev;

	/* The following two variables are intricately linked to the DRM interface,
	 * and must be in this physical memory order, or else chaos ensues.
	 * See the DRM's implementation of R300_CMD_SCRATCH for details.
	 */
	uint32_t pending_age; /** Buffer object pending until this age is reached, written by the DRM */
	uint32_t pending_count; /** Number of pending R300_CMD_SCRATCH references to this object */
};

static radeon_bufmgr_classic* get_bufmgr_classic(dri_bufmgr *bufmgr_ctx)
{
	return (radeon_bufmgr_classic*)bufmgr_ctx;
}

static radeon_bo_classic* get_bo_classic(dri_bo *bo_base)
{
	return (radeon_bo_classic*)bo_base;
}

/**
 * Really free a given buffer object.
 */
static void bo_free(radeon_bo_classic *bo)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bo->base.bufmgr);
	drm_radeon_mem_free_t memfree;
	int ret;

	assert(!bo->refcount);
	assert(!bo->pending);
	assert(!bo->mapcount);

	*bo->pprev = bo->next;
	if (bo->next)
		bo->next->pprev = bo->pprev;

	memfree.region = RADEON_MEM_REGION_GART;
	memfree.region_offset = bo->base.offset;
	memfree.region_offset -= bufmgr->rmesa->radeon.radeonScreen->gart_texture_offset;

	ret = drmCommandWrite(bufmgr->rmesa->radeon.radeonScreen->driScreen->fd,
		DRM_RADEON_FREE, &memfree, sizeof(memfree));
	if (ret) {
		fprintf(stderr, "Failed to free bo[%p] at %08x\n", bo, memfree.region_offset);
		fprintf(stderr, "ret = %s\n", strerror(-ret));
		exit(1);
	}

	free(bo);
}


/**
 * Keep track of which buffer objects are still pending, i.e. waiting for
 * some hardware operation to complete.
 */
static void track_pending_buffers(radeon_bufmgr_classic *bufmgr)
{
	uint32_t currentage = radeonGetAge((radeonContextPtr)bufmgr->rmesa);

	while(bufmgr->pending) {
		radeon_bo_classic *bo = bufmgr->pending;

		assert(bo->pending);

		if (bo->pending_count ||
		    bo->pending_age > currentage) // TODO: Age counter wraparound!
			break;

		bo->pending = 0;
		bufmgr->pending = bo->pending_next;
		if (bufmgr->pending)
			bufmgr->pending->pending_pprev = &bufmgr->pending;
		else
			bufmgr->pending_tail = &bufmgr->pending;

		if (!bo->refcount)
			bo_free(bo);
	}
}


/**
 * Call the DRM to allocate GART memory for the given (incomplete)
 * buffer object.
 */
static int try_alloc(radeon_bufmgr_classic *bufmgr, radeon_bo_classic *bo,
		unsigned long size, unsigned int alignment)
{
	drm_radeon_mem_alloc_t alloc;
	int baseoffset;
	int ret;

	alloc.region = RADEON_MEM_REGION_GART;
	alloc.alignment = alignment;
	alloc.size = size;
	alloc.region_offset = &baseoffset;

	ret = drmCommandWriteRead(bufmgr->rmesa->radeon.dri.fd,
			DRM_RADEON_ALLOC, &alloc, sizeof(alloc));
	if (ret) {
		if (RADEON_DEBUG & DEBUG_MEMORY)
			fprintf(stderr, "DRM_RADEON_ALLOC failed: %d\n", ret);
		return 0;
	}

	bo->base.virtual = (char*)bufmgr->rmesa->radeon.radeonScreen->gartTextures.map + baseoffset;
	bo->base.offset = bufmgr->rmesa->radeon.radeonScreen->gart_texture_offset + baseoffset;

	return 1;
}

static dri_bo *bufmgr_classic_bo_alloc(dri_bufmgr *bufmgr_ctx, const char *name,
		unsigned long size, unsigned int alignment,
		uint64_t location_mask)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bufmgr_ctx);
	radeon_bo_classic* bo = (radeon_bo_classic*)calloc(1, sizeof(radeon_bo_classic));

	bo->base.bufmgr = bufmgr_ctx;
	bo->base.size = size;
	bo->refcount = 1;

	track_pending_buffers(bufmgr);

	if (!try_alloc(bufmgr, bo, size, alignment)) {
		if (RADEON_DEBUG & DEBUG_MEMORY)
			fprintf(stderr, "Failed to allocate %ld bytes, finishing command buffer...\n", size);
		radeonFinish(bufmgr->rmesa->radeon.glCtx);
		track_pending_buffers(bufmgr);
		if (!try_alloc(bufmgr, bo, size, alignment)) {
			WARN_ONCE(
				"Ran out of GART memory (for %ld)!\n"
				"Please consider adjusting GARTSize option.\n",
				size);
			goto fail;
		}
	}

	bo->pprev = &bufmgr->buffers;
	bo->next = bufmgr->buffers;
	if (bo->next)
		bo->next->pprev = &bo->next;
	bufmgr->buffers = bo;

	return &bo->base;
fail:
	free(bo);
	return 0;
}


static void bufmgr_classic_bo_reference(dri_bo *bo_base)
{
	radeon_bo_classic *bo = get_bo_classic(bo_base);
	bo->refcount++;
	assert(bo->refcount > 0);
}

static void bufmgr_classic_bo_unreference(dri_bo *bo_base)
{
	radeon_bo_classic *bo = get_bo_classic(bo_base);

	if (!bo_base)
		return;

	assert(bo->refcount > 0);
	bo->refcount--;
	if (!bo->refcount) {
		// Ugly HACK - figure out whether this is really necessary
		get_bufmgr_classic(bo_base->bufmgr)->rmesa->dma.nr_released_bufs++;

		assert(!bo->mapcount);
		if (!bo->pending)
			bo_free(bo);
	}
}

static int bufmgr_classic_bo_map(dri_bo *bo_base, GLboolean write_enable)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bo_base->bufmgr);
	radeon_bo_classic *bo = get_bo_classic(bo_base);
	assert(bo->refcount > 0);

	if (bo->pending) {
		track_pending_buffers(bufmgr);
		if (bo->pending) {
			// TODO: Better fence waiting
			if (RADEON_DEBUG & DEBUG_MEMORY)
				fprintf(stderr, "bo_map: buffer is pending. Flushing...\n");
			radeonFinish(bufmgr->rmesa->radeon.glCtx);
			track_pending_buffers(bufmgr);
			if (bo->pending) {
				fprintf(stderr, "Internal error or hardware lockup: bo_map: buffer is still pending.\n");
				exit(-1);
			}
		}
	}

	bo->mapcount++;
	assert(bo->mapcount > 0);
	return 0;
}

static int bufmgr_classic_bo_unmap(dri_bo *buf)
{
	radeon_bo_classic *bo = get_bo_classic(buf);
	assert(bo->refcount > 0);
	assert(bo->mapcount > 0);
	bo->mapcount--;
	return 0;
}

static void bufmgr_classic_bo_use(dri_bo* buf)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(buf->bufmgr);
	radeon_bo_classic *bo = get_bo_classic(buf);
	drm_r300_cmd_header_t *cmd;
	uint64_t ull;

	cmd = (drm_r300_cmd_header_t *) r300AllocCmdBuf(bufmgr->rmesa,
			2 + sizeof(ull) / 4,  __FUNCTION__);
	cmd[0].scratch.cmd_type = R300_CMD_SCRATCH;
	cmd[0].scratch.reg = 2; /* Scratch register 2 corresponds to what radeonGetAge polls */
	cmd[0].scratch.n_bufs = 1;
	cmd[0].scratch.flags = 0;
	cmd++;

	ull = (uint64_t) (intptr_t) &bo->pending_age;
	_mesa_memcpy(cmd, &ull, sizeof(ull));
	cmd += sizeof(ull) / 4;

	cmd[0].u = 0;

	bo->pending_count++;

	if (bo->pending) {
		*bo->pending_pprev = bo->pending_next;
		if (bo->pending_next)
			bo->pending_next->pending_pprev = bo->pending_pprev;
		else
			bufmgr->pending_tail = bo->pending_pprev;
	}

	bo->pending = 1;
	bo->pending_pprev = bufmgr->pending_tail;
	bo->pending_next = 0;
	*bufmgr->pending_tail = bo;
	bufmgr->pending_tail = &bo->pending_next;
}

static void bufmgr_classic_destroy(dri_bufmgr *bufmgr_ctx)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bufmgr_ctx);

	track_pending_buffers(bufmgr);
	if (bufmgr->pending)
		radeonFinish(bufmgr->rmesa->radeon.glCtx);
	track_pending_buffers(bufmgr);

	if (bufmgr->buffers) {
		fprintf(stderr, "Warning: Buffer objects have leaked\n");
		while(bufmgr->buffers) {
			fprintf(stderr, "  Leak of size %ld\n", bufmgr->buffers->base.size);
			bufmgr->buffers->refcount = 0;
			bufmgr->buffers->mapcount = 0;
			bufmgr->buffers->pending = 0;
			bo_free(bufmgr->buffers);
		}
	}

	free(bufmgr);
}

radeon_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa)
{
	radeon_bufmgr_classic* bufmgr = (radeon_bufmgr_classic*)calloc(1, sizeof(radeon_bufmgr_classic));

	bufmgr->rmesa = rmesa;
	bufmgr->base.base.bo_alloc = &bufmgr_classic_bo_alloc;
	bufmgr->base.base.bo_reference = &bufmgr_classic_bo_reference;
	bufmgr->base.base.bo_unreference = &bufmgr_classic_bo_unreference;
	bufmgr->base.base.bo_map = &bufmgr_classic_bo_map;
	bufmgr->base.base.bo_unmap = &bufmgr_classic_bo_unmap;
	bufmgr->base.base.destroy = &bufmgr_classic_destroy;
	bufmgr->base.bo_use = &bufmgr_classic_bo_use;

	bufmgr->pending_tail = &bufmgr->pending;

	return &bufmgr->base;
}
