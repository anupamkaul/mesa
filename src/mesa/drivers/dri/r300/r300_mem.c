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

#include <errno.h>
#include <unistd.h>

#include "main/simple_list.h"

#include "radeon_buffer.h"
#include "radeon_ioctl.h"
#include "r300_cmdbuf.h"

typedef struct _radeon_bufmgr_classic radeon_bufmgr_classic;
typedef struct _radeon_bo_classic radeon_bo_classic;
typedef struct _radeon_bo_functions radeon_bo_functions;
typedef struct _radeon_reloc radeon_reloc;
typedef struct _radeon_bo_vram radeon_bo_vram;

struct _radeon_bufmgr_classic {
	dri_bufmgr base;
	radeonScreenPtr screen;
	r300ContextPtr rmesa;

	radeon_bo_classic *buffers; /** Unsorted linked list of all buffer objects */

	radeon_bo_classic *pending; /** Age-sorted linked list of pending buffer objects */
	radeon_bo_classic **pending_tail;

	/* Texture heap bookkeeping */
	driTexHeap *texture_heap;
	GLuint texture_offset;
	driTextureObject texture_swapped;
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
	 *
	 * May be null for buffer objects that are always valid.
	 * Always called with lock held.
	 */
	void (*validate)(radeon_bo_classic*);

	/**
	 * Map the buffer for CPU access.
	 * Only called when the buffer isn't already mapped.
	 *
	 * May be null.
	 */
	void (*map)(radeon_bo_classic*, GLboolean write);

	/**
	 * Unmap the buffer.
	 * Only called on final unmap.
	 *
	 * May be null.
	 */
	void (*unmap)(radeon_bo_classic*);

	/**
	 * Indicate that the buffer object is now used by the hardware.
	 *
	 * May be null.
	 */
	void (*bind)(radeon_bo_classic*);

	/**
	 * Indicate that the buffer object is no longer used by the hardware.
	 *
	 * May be null.
	 */
	void (*unbind)(radeon_bo_classic*);
};

/**
 * A buffer object. There are three types of buffer objects:
 *  1. cmdbuf: Ordinary malloc()ed memory, used for command buffers
 *  2. dma: GART memory allocated via the DRM_RADEON_ALLOC ioctl.
 *  3. vram: Objects with malloc()ed backing store that will be uploaded
 *     into VRAM on demand; used for textures.
 * There is a @ref functions table for operations that depend on the
 * buffer object type.
 *
 * Fencing is handled the same way all buffer objects. During command buffer
 * submission, the pending flag and corresponding variables are set accordingly.
 */
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

typedef struct _radeon_vram_wrapper radeon_vram_wrapper;

/** Wrapper around heap object */
struct _radeon_vram_wrapper {
	driTextureObject base;
	radeon_bo_vram *bo;
};

struct _radeon_bo_vram {
	radeon_bo_classic base;

	unsigned int backing_store_dirty:1; /** Backing store has changed, block must be reuploaded */

	radeon_vram_wrapper *vram; /** Block in VRAM (if any) */
};

static radeon_bufmgr_classic* get_bufmgr_classic(dri_bufmgr *bufmgr_ctx)
{
	return (radeon_bufmgr_classic*)bufmgr_ctx;
}

static radeon_bo_classic* get_bo_classic(dri_bo *bo_base)
{
	return (radeon_bo_classic*)bo_base;
}

static radeon_bo_vram* get_bo_vram(radeon_bo_classic *bo_base)
{
	return (radeon_bo_vram*)bo_base;
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

		if (bo->functions->unbind)
			(*bo->functions->unbind)(bo);
		if (!bo->refcount)
			bo_free(bo);
	}
}

/**
 * Initialize common buffer object data.
 */
static void init_buffer(radeon_bufmgr_classic *bufmgr, radeon_bo_classic *bo, unsigned long size)
{
	bo->base.bufmgr = &bufmgr->base;
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
	memfree.region_offset -= bufmgr->screen->gart_texture_offset;

	ret = drmCommandWrite(bufmgr->screen->driScreen->fd,
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

	ret = drmCommandWriteRead(bufmgr->screen->driScreen->fd,
			DRM_RADEON_ALLOC, &alloc, sizeof(alloc));
	if (ret) {
		if (RADEON_DEBUG & DEBUG_MEMORY)
			fprintf(stderr, "DRM_RADEON_ALLOC failed: %d\n", ret);
		return 0;
	}

	bo->base.virtual = (char*)bufmgr->screen->gartTextures.map + baseoffset;
	bo->base.offset = bufmgr->screen->gart_texture_offset + baseoffset;

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

/**
 * Free a VRAM-based buffer object.
 */
static void vram_free(radeon_bo_classic *bo_base)
{
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	if (bo->vram) {
		driDestroyTextureObject(&bo->vram->base);
		bo->vram = 0;
	}

	free(bo->base.base.virtual);
	free(bo);
}

/**
 * Allocate/update the copy in vram.
 *
 * Note: Assume we're called with the DRI lock held.
 */
static void vram_validate(radeon_bo_classic *bo_base)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bo_base->base.bufmgr);
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	if (!bo->vram) {
		bo->backing_store_dirty = 1;

		bo->vram = (radeon_vram_wrapper*)calloc(1, sizeof(radeon_vram_wrapper));
		bo->vram->bo = bo;
		make_empty_list(&bo->vram->base);
		bo->vram->base.totalSize = bo->base.base.size;
		if (driAllocateTexture(&bufmgr->texture_heap, 1, &bo->vram->base) < 0) {
			fprintf(stderr, "Ouch! vram_validate failed\n");
			free(bo->vram);
			bo->base.base.offset = 0;
			bo->vram = 0;
			return;
		}
	}

	assert(bo->vram->base.memBlock);

	bo->base.base.offset = bufmgr->texture_offset + bo->vram->base.memBlock->ofs;

	if (bo->backing_store_dirty) {
		/* Copy to VRAM using a blit.
		 * All memory is 4K aligned. We're using 1024 pixels wide blits.
		 */
		drm_radeon_texture_t tex;
		drm_radeon_tex_image_t tmp;
		int ret;

		tex.offset = bo->base.base.offset;
		tex.image = &tmp;

		assert(!(tex.offset & 1023));

		tmp.x = 0;
		tmp.y = 0;
		if (bo->base.base.size < 4096) {
			tmp.width = (bo->base.base.size + 3) / 4;
			tmp.height = 1;
		} else {
			tmp.width = 1024;
			tmp.height = (bo->base.base.size + 4095) / 4096;
		}
		tmp.data = bo->base.base.virtual;

		tex.format = RADEON_TXFORMAT_ARGB8888;
		tex.width = tmp.width;
		tex.height = tmp.height;
		tex.pitch = MAX2(tmp.width / 16, 1);

		do {
			ret = drmCommandWriteRead(bufmgr->screen->driScreen->fd,
						DRM_RADEON_TEXTURE, &tex,
						sizeof(drm_radeon_texture_t));
			if (ret) {
				if (RADEON_DEBUG & DEBUG_IOCTL)
					fprintf(stderr,
						"DRM_RADEON_TEXTURE:  again!\n");
				usleep(1);
			}
		} while (ret == -EAGAIN);

		bo->backing_store_dirty = 0;
	}

	bo->base.validated = 1;
}

/* No need for actual mmap actions since we have backing store,
 * but mark buffer dirty when necessary */
static void vram_map(radeon_bo_classic *bo_base, GLboolean write)
{
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	if (write) {
		bo->base.validated = 0;
		bo->backing_store_dirty = 1;
	}
}

static void vram_bind(radeon_bo_classic *bo_base)
{
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	if (bo->vram) {
		bo->vram->base.bound = 1;
		driUpdateTextureLRU(&bo->vram->base);
	}
}

static void vram_unbind(radeon_bo_classic *bo_base)
{
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	if (bo->vram)
		bo->vram->base.bound = 0;
}

/** Callback function called by the texture heap when a texture is evicted */
static void destroy_vram_wrapper(void *data, driTextureObject *t)
{
	radeon_vram_wrapper *wrapper = (radeon_vram_wrapper*)t;

	if (wrapper->bo && wrapper->bo->vram == wrapper) {
		wrapper->bo->base.validated = 0;
		wrapper->bo->vram = 0;
	}
}

static const radeon_bo_functions vram_bo_functions = {
	.free = vram_free,
	.validate = vram_validate,
	.map = vram_map,
	.bind = vram_bind,
	.unbind = vram_unbind
};

/**
 * Free a VRAM-based buffer object.
 */
static void static_free(radeon_bo_classic *bo_base)
{
	radeon_bo_vram *bo = get_bo_vram(bo_base);

	free(bo);
}

static void static_map(radeon_bo_classic *bo_base, GLboolean write)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bo_base->base.bufmgr);

	bo_base->base.virtual = bufmgr->screen->driScreen->pFB +
		(bo_base->base.offset - bufmgr->screen->fbLocation);

	/* Read the first pixel in the frame buffer.  This should
	 * be a noop, right?  In fact without this conform fails as reading
	 * from the framebuffer sometimes produces old results -- the
	 * on-card read cache gets mixed up and doesn't notice that the
	 * framebuffer has been updated.
	 *
	 * Note that we should probably be reading some otherwise unused
	 * region of VRAM, otherwise we might get incorrect results when
	 * reading pixels from the top left of the screen.
	 *
	 * I found this problem on an R420 with glean's texCube test.
	 * Note that the R200 span code also *writes* the first pixel in the
	 * framebuffer, but I've found this to be unnecessary.
	 *  -- Nicolai Hähnle, June 2008
	 */
	{
		int p;
		volatile int *buf = (int*)bufmgr->screen->driScreen->pFB;
		p = *buf;
	}
}

static void static_unmap(radeon_bo_classic *bo_base)
{
	bo_base->base.virtual = 0;
}

static const radeon_bo_functions static_bo_functions = {
	.free = static_free,
	.map = static_map,
	.unmap = static_unmap
};

/**
 * Allocate a backing store buffer object that is validated into VRAM.
 */
static dri_bo *vram_alloc(radeon_bufmgr_classic *bufmgr, const char *name,
		unsigned long size, unsigned int alignment)
{
	radeon_bo_vram* bo = (radeon_bo_vram*)calloc(1, sizeof(radeon_bo_vram));

	bo->base.functions = &vram_bo_functions;
	bo->base.base.virtual = malloc(size);
	init_buffer(bufmgr, &bo->base, size);
	return &bo->base.base;
}

static dri_bo *bufmgr_classic_bo_alloc(dri_bufmgr *bufmgr_ctx, const char *name,
		unsigned long size, unsigned int alignment,
		uint64_t location_mask)
{
	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bufmgr_ctx);

	if (location_mask & DRM_BO_MEM_CMDBUF) {
		return cmdbuf_alloc(bufmgr, name, size);
	} else if (location_mask & DRM_BO_MEM_DMA) {
		return dma_alloc(bufmgr, name, size, alignment);
	} else {
		return vram_alloc(bufmgr, name, size, alignment);
	}
}

static dri_bo *bufmgr_classic_bo_alloc_static(dri_bufmgr *bufmgr_ctx, const char *name,
					      unsigned long offset, unsigned long size,
					      void *virtual, uint64_t location_mask)
{
  	radeon_bufmgr_classic* bufmgr = get_bufmgr_classic(bufmgr_ctx);
	radeon_bo_vram* bo = (radeon_bo_vram*)calloc(1, sizeof(radeon_bo_vram));

	bo->base.functions = &static_bo_functions;
	bo->base.base.virtual = virtual;
	bo->base.base.offset = offset + bufmgr->screen->fbLocation;
	bo->base.validated = 1; /* Static buffer offsets are always valid */

	init_buffer(bufmgr, &bo->base, size);
	return &bo->base.base;

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
				abort();
			}
		}
	}

	if (!bo->mapcount && bo->functions->map)
		bo->functions->map(bo, write_enable);

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

	if (!bo->mapcount && bo->functions->unmap)
		bo->functions->unmap(bo);

	return 0;
}

/**
 * Mark the given buffer as pending and move it to the tail
 * of the pending list.
 * The caller is responsible for setting up pending_count and pending_age.
 */
static void move_to_pending_tail(radeon_bo_classic *bo)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bo->base.bufmgr);

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

/**
 * Emit commands to the batch buffer that cause the guven buffer's
 * pending_count and pending_age to be updated.
 */
static void emit_age_for_buffer(radeon_bo_classic* bo)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bo->base.bufmgr);
	BATCH_LOCALS(bufmgr->rmesa);
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
	COMMIT_BATCH();

	bo->pending_count++;
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

	// Warning: At this point, we append something to the batch buffer
	// during flush.
	emit_age_for_buffer(batch_bo);

	dri_bo_map(batch_buf, GL_TRUE);
	for(i = 0; i < batch_bo->relocs_used; ++i) {
		radeon_reloc *reloc = &batch_bo->relocs[i];
		uint32_t *dest = (uint32_t*)((char*)batch_buf->virtual + reloc->offset);
		uint32_t offset;

		if (!reloc->target->validated)
			reloc->target->functions->validate(reloc->target);
		reloc->target->used = 1;
		offset = reloc->target->base.offset + reloc->delta;

		if (reloc->flags & DRM_RELOC_BLITTER)
			*dest = (*dest & 0xffc00000) | (offset >> 10);
		else if (reloc->flags & DRM_RELOC_TXOFFSET)
			*dest = (*dest & 31) | (offset & ~31);
		else
			*dest = offset;
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

	assert(!batch_bo->pending_count);

	for(i = 0; i < batch_bo->relocs_used; ++i) {
		radeon_reloc *reloc = &batch_bo->relocs[i];

		if (reloc->target->used) {
			reloc->target->used = 0;
			assert(!reloc->target->pending_count);
			reloc->target->pending_age = batch_bo->pending_age;
			move_to_pending_tail(reloc->target);
			if (reloc->target->functions->bind)
				(*reloc->target->functions->bind)(reloc->target);
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

	driDestroyTextureHeap(bufmgr->texture_heap);
	bufmgr->texture_heap = 0;
	assert(is_empty_list(&bufmgr->texture_swapped));

	free(bufmgr);
}

dri_bufmgr* radeonBufmgrClassicInit(r300ContextPtr rmesa)
{
	radeon_bufmgr_classic* bufmgr = (radeon_bufmgr_classic*)calloc(1, sizeof(radeon_bufmgr_classic));

	bufmgr->screen = rmesa->radeon.radeonScreen;
	bufmgr->rmesa = rmesa;
	bufmgr->base.bo_alloc = &bufmgr_classic_bo_alloc;
	bufmgr->base.bo_alloc_static = bufmgr_classic_bo_alloc_static;
	bufmgr->base.bo_reference = &bufmgr_classic_bo_reference;
	bufmgr->base.bo_unreference = &bufmgr_classic_bo_unreference;
	bufmgr->base.bo_map = &bufmgr_classic_bo_map;
	bufmgr->base.bo_unmap = &bufmgr_classic_bo_unmap;
	bufmgr->base.emit_reloc = &bufmgr_classic_emit_reloc;
	bufmgr->base.process_relocs = &bufmgr_classic_process_relocs;
	bufmgr->base.post_submit = &bufmgr_classic_post_submit;
	bufmgr->base.destroy = &bufmgr_classic_destroy;

	bufmgr->pending_tail = &bufmgr->pending;

	/* Init texture heap */
	make_empty_list(&bufmgr->texture_swapped);
	bufmgr->texture_heap = driCreateTextureHeap(0, bufmgr,
			bufmgr->screen->texSize[0], 12, RADEON_NR_TEX_REGIONS,
			(drmTextureRegionPtr)rmesa->radeon.sarea->tex_list[0],
			&rmesa->radeon.sarea->tex_age[0],
			&bufmgr->texture_swapped, sizeof(radeon_vram_wrapper),
			&destroy_vram_wrapper);
	bufmgr->texture_offset = bufmgr->screen->texOffset[0];

	return &bufmgr->base;
}

void radeonBufmgrContendedLockTake(dri_bufmgr* bufmgr_ctx)
{
	radeon_bufmgr_classic *bufmgr = get_bufmgr_classic(bufmgr_ctx);

	DRI_AGE_TEXTURES(bufmgr->texture_heap);
}
