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
typedef struct _radeon_bo_functions radeon_bo_functions;
typedef struct _radeon_reloc radeon_reloc;

struct _radeon_bufmgr_classic {
	radeon_bufmgr base;
	r300ContextPtr rmesa;

	radeon_bo_classic *buffers; /** Unsorted linked list of all buffer objects */

	radeon_bo_classic *pending; /** Age-sorted linked list of pending buffer objects */
	radeon_bo_classic **pending_tail;
};

struct _radeon_reloc {
	uint64_t flags;
	GLuint offset; /**< Offset (in bytes) into command buffer to relocated dword */
	radeon_bo_classic *target;
	GLuint delta;
};

struct _radeon_bo_functions {
	/**
	 * Free a buffer object. Caller has verified that the object is not
	 * referenced or pending.
	 */
	void (*free)(radeon_bo_classic*);

	/**
	 * Validate the given buffer. Must set the validated flag to 1.
	 */
	void (*validate)(radeon_bo_classic*);
};

struct _radeon_bo_classic {
	dri_bo base;

	const radeon_bo_functions *functions;

	radeon_bo_classic *next; /** Unsorted linked list of all buffer objects */
	radeon_bo_classic **pprev;

	/**
	 * Number of software references to this buffer.
	 * A buffer is freed automatically as soon as its reference count reaches 0
	 * *and* it is no longer pending.
	 */
	unsigned int refcount;
	unsigned int mapcount; /** mmap count; mutually exclusive to being pending */

	unsigned int validated:1; /** whether the buffer is validated for hardware use right now */
	unsigned int used:1; /* only for communication between process_relocs and post_submit */

	unsigned int pending:1;
	radeon_bo_classic *pending_next; /** Age-sorted linked list of pending buffer objects */
	radeon_bo_classic **pending_pprev;

	/* The following two variables are intricately linked to the DRM interface,
	 * and must be in this physical memory order, or else chaos ensues.
	 * See the DRM's implementation of R300_CMD_SCRATCH for details.
	 */
	uint32_t pending_age; /** Buffer object pending until this age is reached, written by the DRM */
	uint32_t pending_count; /** Number of pending R300_CMD_SCRATCH references to this object */

	radeon_reloc *relocs; /** Array of relocations in this buffer */
	GLuint relocs_used; /** # of relocations in relocation array */
	GLuint relocs_size; /** # of reloc records reserved in relocation array */
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
	assert(!bo->refcount);
	assert(!bo->pending);
	assert(!bo->mapcount);

	if (bo->relocs) {
		int i;
		for(i = 0; i < bo->relocs_used; ++i)
			dri_bo_unreference(&bo->relocs[i].target->base);
		free(bo->relocs);
		bo->relocs = 0;
	}

	*bo->pprev = bo->next;
	if (bo->next)
		bo->next->pprev = bo->pprev;

	bo->functions->free(bo);
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
 * Initialize common buffer object data.
 */
static void init_buffer(radeon_bufmgr_classic *bufmgr, radeon_bo_classic *bo, unsigned long size)
{
	bo->base.bufmgr = &bufmgr->base.base;
	bo->base.size = size;
	bo->refcount = 1;

	bo->pprev = &bufmgr->buffers;
	bo->next = bufmgr->buffers;
	if (bo->next)
		bo->next->pprev = &bo->next;
	bufmgr->buffers = bo;
}


/**
 * Free a DMA-based buffer.
 */
static void dma_free(radeon_bo_classic *bo)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bo->base.bufmgr);
	drm_radeon_mem_free_t memfree;
	int ret;

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

static const radeon_bo_functions dma_bo_functions = {
	.free = &dma_free
};

/**
 * Call the DRM to allocate GART memory for the given (incomplete)
 * buffer object.
 */
static int try_dma_alloc(radeon_bufmgr_classic *bufmgr, radeon_bo_classic *bo,
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

/**
 * Allocate a DMA buffer.
 */
static dri_bo *dma_alloc(radeon_bufmgr_classic *bufmgr, const char *name,
		unsigned long size, unsigned int alignment)
{
	radeon_bo_classic* bo = (radeon_bo_classic*)calloc(1, sizeof(radeon_bo_classic));

	bo->functions = &dma_bo_functions;

	track_pending_buffers(bufmgr);
	if (!try_dma_alloc(bufmgr, bo, size, alignment)) {
		if (RADEON_DEBUG & DEBUG_MEMORY)
			fprintf(stderr, "Failed to allocate %ld bytes, finishing command buffer...\n", size);
		radeonFinish(bufmgr->rmesa->radeon.glCtx);
		track_pending_buffers(bufmgr);
		if (!try_dma_alloc(bufmgr, bo, size, alignment)) {
			WARN_ONCE(
				"Ran out of GART memory (for %ld)!\n"
				"Please consider adjusting GARTSize option.\n",
				size);
			free(bo);
			return 0;
		}
	}

	init_buffer(bufmgr, bo, size);
	bo->validated = 1; /* DMA buffer offsets are always valid */

	return &bo->base;
}

/**
 * Free a command buffer
 */
static void cmdbuf_free(radeon_bo_classic *bo)
{
	free(bo->base.virtual);
	free(bo);
}

static const radeon_bo_functions cmdbuf_bo_functions = {
	.free = cmdbuf_free
};

/**
 * Allocate a command buffer.
 *
 * Command buffers are really just malloc'ed buffers. They are managed by
 * the bufmgr to enable relocations.
 */
static dri_bo *cmdbuf_alloc(radeon_bufmgr_classic *bufmgr, const char *name,
		unsigned long size)
{
	radeon_bo_classic* bo = (radeon_bo_classic*)calloc(1, sizeof(radeon_bo_classic));

	bo->functions = &cmdbuf_bo_functions;
	bo->base.virtual = malloc(size);

	init_buffer(bufmgr, bo, size);
	return &bo->base;
}

static dri_bo *bufmgr_classic_bo_alloc(dri_bufmgr *bufmgr_ctx, const char *name,
		unsigned long size, unsigned int alignment,
		uint64_t location_mask)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bufmgr_ctx);

	if (location_mask & DRM_BO_MEM_CMDBUF) {
		return cmdbuf_alloc(bufmgr, name, size);
	} else {
		assert(location_mask & DRM_BO_MEM_DMA);

		return dma_alloc(bufmgr, name, size, alignment);
	}
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
	BATCH_LOCALS(bufmgr->rmesa);
	radeon_bo_classic *bo = get_bo_classic(buf);
	drm_r300_cmd_header_t cmd;
	uint64_t ull;

	cmd.scratch.cmd_type = R300_CMD_SCRATCH;
	cmd.scratch.reg = 2; /* Scratch register 2 corresponds to what radeonGetAge polls */
	cmd.scratch.n_bufs = 1;
	cmd.scratch.flags = 0;
	ull = (uint64_t) (intptr_t) &bo->pending_age;

	BEGIN_BATCH(4);
	OUT_BATCH(cmd.u);
	OUT_BATCH(ull & 0xffffffff);
	OUT_BATCH(ull >> 32);
	OUT_BATCH(0);
	END_BATCH();

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

static int bufmgr_classic_emit_reloc(dri_bo *batch_buf, uint64_t flags, GLuint delta,
			GLuint offset, dri_bo *target)
{
	radeon_bo_classic *bo = get_bo_classic(batch_buf);
	radeon_reloc *reloc;

	if (bo->relocs_used >= bo->relocs_size) {
		bo->relocs_size *= 2;
		if (bo->relocs_size < 32)
			bo->relocs_size = 32;

		bo->relocs = (radeon_reloc*)realloc(bo->relocs, bo->relocs_size*sizeof(radeon_reloc));
	}

	reloc = &bo->relocs[bo->relocs_used++];
	reloc->flags = flags;
	reloc->offset = offset;
	reloc->delta = delta;
	reloc->target = get_bo_classic(target);
	dri_bo_reference(target);
	return 0;
}

/* process_relocs is called just before the given command buffer
 * is executed. It ensures that all referenced buffers are in
 * the right GPU domain.
 */
static void *bufmgr_classic_process_relocs(dri_bo *batch_buf, GLuint *count)
{
	radeon_bo_classic *batch_bo = get_bo_classic(batch_buf);
	int i;

	dri_bo_map(batch_buf, GL_TRUE);
	for(i = 0; i < batch_bo->relocs_used; ++i) {
		radeon_reloc *reloc = &batch_bo->relocs[i];
		uint32_t *dest = (uint32_t*)((char*)batch_buf->virtual + reloc->offset);

		if (!reloc->target->validated)
			reloc->target->functions->validate(reloc->target);
		reloc->target->used = 1;

		*dest = reloc->target->base.offset + reloc->delta;
	}
	dri_bo_unmap(batch_buf);
	return 0;
}

/* post_submit is called just after the given command buffer
 * is executed. It ensures that buffers are properly marked as
 * pending.
 */
static void bufmgr_classic_post_submit(dri_bo *batch_buf, dri_fence **fence)
{
	radeon_bo_classic *batch_bo = get_bo_classic(batch_buf);
	int i;

	for(i = 0; i < batch_bo->relocs_used; ++i) {
		radeon_reloc *reloc = &batch_bo->relocs[i];

		if (reloc->target->used) {
			bufmgr_classic_bo_use(&reloc->target->base);
			reloc->target->used = 0;
		}
	}
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
	bufmgr->base.base.emit_reloc = &bufmgr_classic_emit_reloc;
	bufmgr->base.base.process_relocs = &bufmgr_classic_process_relocs;
	bufmgr->base.base.post_submit = &bufmgr_classic_post_submit;
	bufmgr->base.base.destroy = &bufmgr_classic_destroy;
	bufmgr->base.bo_use = &bufmgr_classic_bo_use;

	bufmgr->pending_tail = &bufmgr->pending;

	return &bufmgr->base;
}
