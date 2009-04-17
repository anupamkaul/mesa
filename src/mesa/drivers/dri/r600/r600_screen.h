/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 * Copyright (C) 2008-2009  Matthias Hopf
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

#ifndef _R600_SCREEN_H
#define _R600_SCREEN_H

#include "r600_common.h"
#include "r600_id.h"

#include "radeon_drm.h"
#include "dri_util.h"


struct screen_s {
    __DRIscreenPrivate *driScreen;
    uint32_t     sareaPrivOffset;
    chip_t       chip;

    /* DRM mapped memory by ourself */
    memmap_t     regs;
    memmap_t     status;				/* memory copy of ring read ptr, scratch regs */
    memmap_t     gartTextures;
    drmBufMapPtr buffers;

    /* Known pointers and externally mapped */
    mem_t        fb;
    mem_t        bufs;
    gpu_t        gart;
    __volatile__ uint32_t *scratch;

    /* Buffers */
    buffer_t     vertBuffer;				/* buffer for sending vertices */
    buffer_t     ioBuffer;				/* buffer for up/downloading data (textures)  */

    /* Targets */
    int          cpp;
    target_t     frontBuffer;
    target_t     backBuffer;
    target_t     depthBuffer;

    /* Dimension */
    int           width;	 
    int           height;

    /* Textures */
    mem_t        textures;				/* .gpu and .size only */

    /* Shared texture data */
    int numTexHeaps;
    int texOffset[RADEON_NR_TEX_HEAPS];
    int texSize[RADEON_NR_TEX_HEAPS];
    int logTexGranularity[RADEON_NR_TEX_HEAPS];

   unsigned int gart_buffer_offset;	    /* offset in card memory space */
   unsigned int gart_texture_offset;    /* offset in card memory space */
    
} ;


#endif	/* _R600_SCREEN_H */
