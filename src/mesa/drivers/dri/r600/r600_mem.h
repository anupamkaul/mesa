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
 
#ifndef __R600_MEM_H__
#define __R600_MEM_H__

#define R600_MEM_UL 1

#define R600_MEM_R 1
#define R600_MEM_W 2
#define R600_MEM_RW (R300_MEM_R | R300_MEM_W)

#define R600_MEM_SCRATCH 2

struct r600_mem_manager
{
    struct 
    {
        void *ptr;
        uint32_t size;
        uint32_t age;
        uint32_t h_pending;
        int pending;
        int mapped;
    } *u_list;
    int u_head, u_size, u_last;
};

extern void  r600MemInit(context_t *context);
extern void  r600MemDestroy(context_t *context);
extern void *r600MemPtr(context_t *context, int id);
extern int   r600MemFind(context_t *context, void *ptr);
extern int   r600MemAlloc(context_t *context, int alignment, int size);
extern void  r600MemFree(context_t *context, int id);
extern void  r600MemUse(context_t *context, int id);
extern unsigned long r600MemOffset(context_t *context, int id);
extern void *r600MemMap(context_t *context, int id, int access);
extern void  r600MemUnmap(context_t *context, int id);

extern void r600AllocDmaRegion(context_t *context, 
                               struct r600_dma_region *region,
                               int bytes, 
                               int alignment);
extern void r600FreeDmaRegion(context_t *context, 
                              struct r600_dma_region *region);

extern GLboolean r600LoadMemSurf(context_t *context,
                               GLuint     dst_offset, /* gpu addr */
                               GLuint     dst_pitch_in_pixel,                               
                               GLuint     src_width_in_pixel,
                               GLuint     height,
                               GLuint     byte_per_pixel,
                               unsigned char* pSrc); /* source data */

extern GLboolean r600AllocMemSurf(context_t   *context,
                           void  **ppmemBlock,
                           void  **ppheap,
                           GLuint      *prefered_heap, 
                           GLuint       totalSize);

#endif
