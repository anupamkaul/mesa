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

#include "glheader.h"
#include "mtypes.h"

#include "r600_context.h"
#include "r600_mem.h"

static uint32_t r600EmitData(context_t *context, struct r600_dma_region *dmaRegion,
                        GLvoid *data, int size, int stride, int count)
{
    uint32_t addr;
    int *out  = (int *)(dmaRegion->address + dmaRegion->start);
    addr = out;
    
    dmaRegion->aos_offset = GET_START(dmaRegion);
    dmaRegion->aos_stride = size;

    COPY_DWORDS(out, data, count);

    addr = addr - (uint32_t)context->screen->gartTextures.cpu + (uint32_t)context->screen->gartTextures.gpu;
    return addr;
}

static void r600EmitVec4(GLcontext * ctx, 
                         struct r600_dma_region *rvb,
			             GLvoid * data, 
                         int stride, 
                         int count)
{
	int i;
	int *out = (int *)(rvb->address + rvb->start);

	if (stride == 4)
    {
		COPY_DWORDS(out, data, count);
    }
	else
    {
		for (i = 0; i < count; i++) 
        {
			out[0] = *(int *)data;
			out++;
			data += stride;
		}
    }
}

static void r600EmitVec8(GLcontext * ctx, 
                         struct r600_dma_region *rvb,
			             GLvoid * data, 
                         int stride, 
                         int count)
{
	int i;
	int *out = (int *)(rvb->address + rvb->start);

	if (stride == 8)
    {
		COPY_DWORDS(out, data, count * 2);
    }
	else
    {
		for (i = 0; i < count; i++) 
        {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out += 2;
			data += stride;
		}
    }
}

static void r600EmitVec12(GLcontext * ctx, 
                          struct r600_dma_region *rvb,
			              GLvoid * data, 
                          int stride, 
                          int count)
{
	int i;
	int *out = (int *)(rvb->address + rvb->start);

	if (stride == 12)
    {
		COPY_DWORDS(out, data, count * 3);
    }
	else
    {
		for (i = 0; i < count; i++) 
        {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out[2] = *(int *)(data + 8);
			out += 3;
			data += stride;
		}
    }
}

static void r600EmitVec16(GLcontext * ctx, 
                          struct r600_dma_region *rvb,
			              GLvoid * data, 
                          int stride, 
                          int count)
{
	int i;
	int *out = (int *)(rvb->address + rvb->start);

	if (stride == 16)
    {
		COPY_DWORDS(out, data, count * 4);
    }
	else
    {
		for (i = 0; i < count; i++) 
        {
			out[0] = *(int *)data;
			out[1] = *(int *)(data + 4);
			out[2] = *(int *)(data + 8);
			out[3] = *(int *)(data + 12);
			out += 4;
			data += stride;
		}
    }
}

void r600EmitShader(GLcontext * ctx, 
                   struct r600_dma_region *rvb,
			       GLvoid * data, 
                   int sizeinDWORD)
{
    R600_CONTEXT;

    r600AllocDmaRegion(context, rvb, sizeinDWORD * 4, 256);
	rvb->aos_offset = GET_START(rvb);
	rvb->aos_stride = 0;

    r600EmitVec4(ctx, rvb, data, 4, sizeinDWORD);
}

void r600EmitVec(GLcontext * ctx, 
                 struct r600_dma_region *rvb,
			     GLvoid * data, 
                 int size, 
                 int stride, 
                 int count)
{
    R600_CONTEXT;

    if (stride == 0) 
    {
        r600AllocDmaRegion(context, rvb, size * count * 4, 4);
        
        rvb->aos_offset = GET_START(rvb);
        rvb->aos_stride = 0;
    } 
    else 
    {
        r600AllocDmaRegion(context, rvb, size * count * 4, 4);
        rvb->aos_offset = GET_START(rvb);
        rvb->aos_stride = size;
    }

    switch (size) 
    {
	case 1:
		r600EmitVec4(ctx, rvb, data, stride, count);
		break;
	case 2:
		r600EmitVec8(ctx, rvb, data, stride, count);
		break;
	case 3:
		r600EmitVec12(ctx, rvb, data, stride, count);
		break;
	case 4:
		r600EmitVec16(ctx, rvb, data, stride, count);
		break;
	default:
		assert(0);
		break;
    }
}

void r600ReleaseArrays(GLcontext * ctx)
{
	R600_CONTEXT;

	int i;

	for (i = 0; i < context->aos_count; i++) 
    {
        r600FreeDmaRegion(context, &(context->aos[i]));
	}
}

