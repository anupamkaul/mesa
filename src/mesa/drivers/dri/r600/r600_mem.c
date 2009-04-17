/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Richard Li <RichardZ.Li@amd.com>, <richardradeon@gmail.com>
 *   CooperYuan <cooper.yuan@amd.com>, <cooperyuan@gmail.com>
 */

#include "imports.h"
#include "macros.h"

#include "r600_context.h"
#include "r600_cmdbuf.h"
#include "r600_mem.h"
#include "r600_lock.h"

GLboolean r600AllocMemSurf(context_t   *context,
                           void       **ppmemBlock,
                           void       **ppheap,
                           GLuint      *prefered_heap, /* Now used RADEON_LOCAL_TEX_HEAP, return actual heap used. */
                           GLuint       totalSize)
{
    driTextureObject t;
   
    t.heap       = (driTexHeap*)*ppheap;
    t.memBlock   = (struct mem_block*)*ppmemBlock;
    t.totalSize = totalSize;

    *prefered_heap = driAllocateTexture(context->texture_heaps, context->nr_heaps, &t);

    if(-1 == *prefered_heap)
    {
        return GL_FALSE;
    }

    *ppmemBlock = t.memBlock;
    *ppheap     = t.heap;
    return GL_TRUE;
}

GLboolean r600LoadMemSurf(context_t *context,
                               GLuint     dst_offset, /* gpu addr */
                               GLuint     dst_pitch_in_pixel,                               
                               GLuint     src_width_in_pixel,
                               GLuint     height,
                               GLuint     byte_per_pixel,
                               unsigned char* pSrc) /* source data */
{
    /* TEST CODE ONLY */
    
    unsigned char *pDst;
    GLuint i;

    pDst = (unsigned char*)(dst_offset - context->screen->fb.gpu + context->screen->fb.cpu);

    for(i=0; i<height; i++)
    {
        memcpy(pDst, pSrc, src_width_in_pixel*byte_per_pixel);
        pDst += dst_pitch_in_pixel * byte_per_pixel;
        pSrc += src_width_in_pixel * byte_per_pixel;
    }

    return GL_TRUE;
}

static unsigned int r600GetAge(context_t *context)
{
    drm_radeon_getparam_t gp;
    int ret;
    uint32_t age;

    gp.param = RADEON_PARAM_LAST_CLEAR;
    gp.value = (int *)&age;
    ret = drmCommandWriteRead(context->fd,
                              DRM_RADEON_GETPARAM,
                              &gp, 
                              sizeof(gp));
    if (ret) 
    {
        fprintf(stderr, "%s: drmRadeonGetParam: %d\n", __FUNCTION__, ret);
        exit(1);
    }

    return age;
}

static void r600ResizeUList(context_t *context)
{
    void *temp;
    int nsize;

    temp  = context->memManager->u_list;
    nsize = context->memManager->u_size * 2;

    context->memManager->u_list = 
                 _mesa_malloc(nsize * sizeof(*context->memManager->u_list));
    _mesa_memset(context->memManager->u_list, 0,
                 nsize * sizeof(*context->memManager->u_list));

    if (temp) 
    {
        r600Flush(&(context->ctx));
        _mesa_memcpy(context->memManager->u_list, 
                     temp,
                     context->memManager->u_size * 
                     sizeof(*context->memManager->u_list));
        _mesa_free(temp);
    }

    context->memManager->u_size = nsize;
}

void r600MemInit(context_t *context)
{
    context->memManager = malloc(sizeof(struct r600_mem_manager));
    memset(context->memManager, 0, sizeof(struct r600_mem_manager));

    context->memManager->u_size = 128;
    r600ResizeUList(context);
}

void r600MemDestroy(context_t *context)
{
    _mesa_free(context->memManager->u_list);
    context->memManager->u_list = NULL;

    _mesa_free(context->memManager);
    context->memManager = NULL;
}

void *r600MemPtr(context_t *context, int id)
{
    assert(id <= context->memManager->u_last);
    return context->memManager->u_list[id].ptr;
}

int r600MemFind(context_t *context, void *ptr)
{
    int i;

    for (i = 1; i < context->memManager->u_size + 1; i++)
    {
        if (context->memManager->u_list[i].ptr &&
            ptr >= context->memManager->u_list[i].ptr &&
            ptr < context->memManager->u_list[i].ptr + 
                  context->memManager->u_list[i].size)
            break;
    }
    
    if (i < context->memManager->u_size + 1)
        return i;

    fprintf(stderr, "%p failed\n", ptr);
    return 0;
}

int r600MemAlloc(context_t *context, int alignment, int size)
{
    drm_radeon_mem_alloc_t alloc;
    int offset = 0, ret;
    int i, free = -1;
    int done_age;
    drm_radeon_mem_free_t memfree;
    int tries = 0;
    static int bytes_wasted = 0, allocated = 0;

    if (size < 4096)
        bytes_wasted += 4096 - size;

    allocated += size;
    memfree.region = RADEON_MEM_REGION_GART;

    while ((free == -1) && (tries < 100))
    {
        done_age = r600GetAge(context);

        if (context->memManager->u_last + 1 >= context->memManager->u_size)
        {
            r600ResizeUList(context);
        }

        for (i = context->memManager->u_last + 1; i > 0; i--) 
        {
            if (context->memManager->u_list[i].ptr == NULL) 
            {
                free = i;
                continue;
            }

            if (context->memManager->u_list[i].h_pending == 0 &&
                context->memManager->u_list[i].pending && 
                context->memManager->u_list[i].age <= done_age) 
            {
                memfree.region_offset = 
                    (char *)context->memManager->u_list[i].ptr -
                    (char *)context->screen->gartTextures.cpu;

                ret = drmCommandWrite(context->fd, 
                                      DRM_RADEON_FREE,
                                      &memfree, 
                                      sizeof(memfree));

                if (ret) 
                {
                    fprintf(stderr, "Failed to free at %p\n",
                                    context->memManager->u_list[i].ptr);
                    fprintf(stderr, "ret = %s\n", strerror(-ret));
                    exit(1);
                } 
                else 
                {
                    fprintf(stderr, "really freed %d at age %x\n", i, 
                                    r600GetAge(context));
                    if (i == context->memManager->u_last)
                    {
                        context->memManager->u_last--;
                    }

                    if (context->memManager->u_list[i].size < 4096)
                    {
                        bytes_wasted -= 4096 - context->memManager->u_list[i].size;
                    }
                    
                    allocated -= context->memManager->u_list[i].size;
                    context->memManager->u_list[i].pending = 0;
                    context->memManager->u_list[i].ptr = NULL;
                    free = i;
                }
            }
        }

        context->memManager->u_head = i;
        tries ++;
    }
    
    if (free == -1) 
    {
        WARN_ONCE("Ran out of slots!\n");
        r600Flush(&(context->ctx));
        exit(1);
    }

    alloc.region        = RADEON_MEM_REGION_GART;
    alloc.alignment     = alignment;
    alloc.size          = size;
    alloc.region_offset = &offset;

    /* DRM_RADEON_ALLOC will alloc a dma region for us */
    ret = drmCommandWriteRead(context->fd, 
                              DRM_RADEON_ALLOC, 
                              &alloc,
                              sizeof(alloc));
    if (ret) 
    {
        WARN_ONCE ("Ran out of GART memory (for %d)!\n", size);
        return 0;
    }

    i = free;

    if (i > context->memManager->u_last)
        context->memManager->u_last = i;

    context->memManager->u_list[i].ptr =
        ((GLubyte *) context->screen->gartTextures.cpu) + offset;

    context->memManager->u_list[i].size = size;
    context->memManager->u_list[i].age = 0;

    DEBUG_FUNCF ("allocated %d at age %x\n", i, r600GetAge(context));

    return i;
}

void r600MemFree(context_t *context, int id)
{
    fprintf(stderr, "%s: %d at age %x\n", __FUNCTION__, id,r600GetAge(context));
	assert(id <= context->memManager->u_last);

	if (id == 0)
		return;

	if (context->memManager->u_list[id].ptr == NULL) {
		WARN_ONCE("Not allocated!\n");
		return;
	}

	if (context->memManager->u_list[id].pending) {
		WARN_ONCE("%p already pended!\n", context->memManager->u_list[id].ptr);
		return;
	}

	context->memManager->u_list[id].pending = 1;
}

void r600MemUse(context_t *context, int id)
{
    uint64_t ull;
    fprintf(stderr, "%s: %d at age %x\n", __FUNCTION__, id, r600GetAge(context));
    drm_r300_cmd_header_t *cmd;

    assert(id <= context->memManager->u_last);

    if (id == 0)
        return;

    cmd = (drm_r300_cmd_header_t *) r600AllocCmdBuf(context, 2 + sizeof(ull) / 4);
    cmd[0].scratch.cmd_type = R300_CMD_SCRATCH;
    cmd[0].scratch.reg      = R600_MEM_SCRATCH;
    cmd[0].scratch.n_bufs   = 1;
    cmd[0].scratch.flags    = 0;
    cmd++;

    ull = (uint64_t) (intptr_t) & context->memManager->u_list[id].age;
    _mesa_memcpy(cmd, &ull, sizeof(ull));
    cmd += sizeof(ull) / 4;

    cmd[0].u = /*id */ 0;

    R600_LOCK_HARDWARE(context);	/* Protect from DRM. */
    context->memManager->u_list[id].h_pending++;
    R600_UNLOCK_HARDWARE(context);
}

unsigned long r600MemOffset(context_t *context, int id)
{
    unsigned long offset;
    assert(id <= context->memManager->u_last);

    offset = (char *)context->memManager->u_list[id].ptr -
             (char *)context->screen->gartTextures.cpu;
    offset += context->screen->gart_texture_offset;

    return offset;
}

void *r600MemMap(context_t *context, int id, int access)
{
    void *ptr;
    int tries = 0;
    fprintf(stderr, "%s: %d at age %x\n", __FUNCTION__, id, r600GetAge(context));

    assert(id <= context->memManager->u_last);

    if (access == R600_MEM_R) 
    {
        if (context->memManager->u_list[id].mapped == 1)
        {
            WARN_ONCE("buffer %d already mapped\n", id);
        }

        context->memManager->u_list[id].mapped = 1;
        ptr = r600MemPtr(context, id);
        return ptr;
    }

    if (context->memManager->u_list[id].h_pending)
    {
        r600Flush(&(context->ctx));
    }

    if (context->memManager->u_list[id].h_pending) 
    {
        return NULL;
	}

    while (context->memManager->u_list[id].age > r600GetAge(context) && 
           tries++ < 1000)
    {
        usleep(10);
    }

    if (tries >= 1000) 
    {
        fprintf(stderr, "Idling failed (%x vs %x)\n",
                        context->memManager->u_list[id].age,
                        r600GetAge(context));
        return NULL;
    }

    if (context->memManager->u_list[id].mapped == 1)
    {
        WARN_ONCE("buffer %d already mapped\n", id);
    }

    context->memManager->u_list[id].mapped = 1;
    ptr = r600MemPtr(context, id);

    return ptr;
}

void r600MemUnmap(context_t *context, int id)
{
    fprintf(stderr, "%s: %d at age %x\n", __FUNCTION__, id, r600GetAge(context));

    assert(id <= context->memManager->u_last);

    if (context->memManager->u_list[id].mapped == 0)
    {
        WARN_ONCE("buffer %d not mapped\n", id);
    }
    context->memManager->u_list[id].mapped = 0;
}

static void r600RefillCurrentDmaRegion(context_t *context, int size)
{
    struct r600_dma_buffer *dmabuf;
    size = MAX2(size, RADEON_BUFFER_SIZE * 16);

    if (context->dma.flush) 
        context->dma.flush(context);

    if (context->dma.nr_released_bufs > 4)
        r600Flush(&(context->ctx));

    dmabuf              = CALLOC_STRUCT(r600_dma_buffer);
    dmabuf->buf         = (void *)1;    /* hack */
    dmabuf->refcount    = 1;
    dmabuf->id          = r600MemAlloc(context, 4, size);
    
    if (dmabuf->id == 0) 
    {
        R600_LOCK_HARDWARE(context);    /* no need to validate */

        r600Flush(&(context->ctx));
        //radeonWaitForIdleLocked(context);
        dmabuf->id = r600MemAlloc(context, 4, size);

        R600_UNLOCK_HARDWARE(context);

        if (dmabuf->id == 0) 
        {
            fprintf(stderr, "Error: Could not get dma buffer... exiting\n");
            _mesa_exit(-1);
        }
    }

    context->dma.current.buf        = dmabuf;
    context->dma.current.address    = r600MemPtr(context, dmabuf->id);
    context->dma.current.end        = size;
    context->dma.current.start      = 0;
    context->dma.current.ptr        = 0;
}

void r600FreeDmaRegion(context_t *context, struct r600_dma_region *region)
{
    if (!region->buf)
        return;

    if (context->dma.flush)
        context->dma.flush(context);

    if (--region->buf->refcount == 0) 
    {
        r600MemFree(context, region->buf->id);
        FREE(region->buf);
        context->dma.nr_released_bufs ++;
    }

    region->buf     = 0;
    region->start   = 0;
}

/* 
 * Allocates a region from context->dma.current.  If there isn't enough
 * space in current, grab a new buffer (and discard what was left of current)
 */
void r600AllocDmaRegion(context_t *context, struct r600_dma_region *region,
                        int bytes, int alignment)
{
    if (context->dma.flush)
        context->dma.flush(context);

    if (region->buf)
        r600FreeDmaRegion(context, region);

    alignment--;
    context->dma.current.ptr    = (context->dma.current.ptr + alignment) & 
                                  ~alignment;
    context->dma.current.start  = context->dma.current.ptr;

    /*
     * TODO, shader buffer is 256-bytes aligned, we should do it more clear.
     */
    if (bytes < 0x1000) // 4096-bytes aligned
        bytes = 0x1000;
    
    if (context->dma.current.ptr + bytes > context->dma.current.end)
    {
        r600RefillCurrentDmaRegion(context, (bytes + 0x7) & ~0x7);
        //if (context->dma.current.buf) 
        //    r600MemUse(context, context->dma.current.buf->id);
    }

    region->start   = context->dma.current.start;
    region->ptr     = context->dma.current.start;
    region->end     = context->dma.current.start + bytes;
    region->address = context->dma.current.address;
    region->buf     = context->dma.current.buf;
    region->buf->refcount++;

    context->dma.current.ptr   += bytes;	/* bug - if alignment > 7 */
    context->dma.current.ptr    = (context->dma.current.ptr + 0x7) & ~0x7;
    context->dma.current.start  = context->dma.current.ptr;
    
    assert(context->dma.current.ptr <= context->dma.current.end);
}

unsigned long r600GARTOffsetFromVirtual(context_t *context, GLvoid * pointer)
{
    int offset = (char *)pointer -
                 (char *)context->screen->gartTextures.cpu;

    if (offset < 0 || offset > context->screen->gartTextures.size)
        return ~0;
    else
        return context->screen->gart_texture_offset + offset;
}
