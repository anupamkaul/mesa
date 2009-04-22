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

#include <errno.h>

#include "mtypes.h"
#include "r600_common.h"
#include "r600_context.h"
#include "r600_lock.h"
#include "radeon_drm.h"
#include "imports.h"

/*
 * Allocate system memory for the command buffer and initialized the state
 * this system memory will be filled with command and in r600FlushCmdBuffer
 * it will be IOCTL to kernel module where kernel memory will be allocated
 * and copy_from_user will be involked.
 */
void r600InitCmdBuf(context_t *context)
{
    int size;
    /* Initialize command buffer */
    
    size = 65536; /* hard code as (64*4)k, temp */
    context->cmdbuf.size            = size;
    context->cmdbuf.cmd_buf         = (uint32_t *)CALLOC(size * 4);
    context->cmdbuf.count_used      = 0;
    context->cmdbuf.count_reemit    = 0;
}

/*
 * Make sure that enough space is available in the command buffer
 * by flushing if necessary.
 *
 * \param dwords The number of dwords we need to be free on the command buffer
 */
static void r600EnsureCmdBufSpace(context_t *context, int dwords)
{
    assert(dwords < context->cmdbuf.size);

    if (context->cmdbuf.count_used + dwords > context->cmdbuf.size)
    {
        if(GL_TRUE == context->cmdbuf.bUseDMAasIndirect)
        {
            r600FlushIndirectBuffer(context);
        }
        else
        {
            r600FlushCmdBuffer(context);
        }
    }
}

uint32_t *r600AllocCmdBuf(context_t *context, int dwords)
{
    uint32_t *ptr;

    r600EnsureCmdBufSpace(context, dwords);

    if (!context->cmdbuf.count_used) 
    {
        fprintf(stderr, "Reemit state after flush %s\n", __FUNCTION__);

        /* TODO, if cmdbuf is not NULL, first emit old states */
        //r600EmitState(context);
    }

    ptr = &context->cmdbuf.cmd_buf[context->cmdbuf.count_used];
    context->cmdbuf.count_used += dwords;
    return ptr;
}

void r600DestroyCmdBuf(context_t *context)
{
    FREE(context->cmdbuf.cmd_buf);
}

/*
 * flush command buffer to hw
 */
int r600FlushCmdBuffer(context_t *context)
{
    int ret;
    drm_radeon_cmd_buffer_t cmd;
    int start = 0;
    
    drmGetLock(context->fd, context->hwContext, DRM_LOCK_HELD);

    cmd.buf     = (char *)(context->cmdbuf.cmd_buf + start);
    cmd.bufsz   = (context->cmdbuf.count_used - start) * 4;
    cmd.nbox    = context->numClipRects;
    cmd.boxes   = context->pClipRects;
    
    ret = drmCommandWrite(context->fd, DRM_RADEON_CMDBUF, &cmd, sizeof(cmd));
    
    drmUnlock(context->fd, context->hwContext);

    context->cmdbuf.count_used      = 0;
    context->cmdbuf.count_reemit    = 0;

    return ret;
}

int r600InitIndirectBuffer(context_t *context)
{
    int       i;
    int       iRet = 0;
    drmDMAReq dma;
    int       nIndex;
    int       nSize;

    dma.context         = context->hwContext;
    dma.send_count      = 0;
    dma.request_count   = 1; /* now we need one dma for indirect buffer */
    dma.request_size    = 0x10000 * 4; /* 64k DWORDS*/
    dma.request_list    = &nIndex;
    dma.request_sizes   = &nSize;
    dma.flags           = DRM_DMA_WAIT;

    for (i=0; i<2000000; i++) 
    {
	    drmGetLock(context->fd, context->hwContext, DRM_LOCK_READY);

        iRet = drmDMA(context->fd, &dma);

	    if( iRet != -EBUSY )
        {
	        drmUnlock(context->fd,context);

	        if(!iRet)break;

	        return iRet;
	    }
	    
	    drmUnlock(context->fd, context->hwContext);
    }

    if(2000000 == i)
    {
        /* drm always busy to map a buf entry for us */
        return -EBUSY;
    }

    context->cmdbuf.size         = (nSize & 0xFFFFFFC0)/4;
    context->cmdbuf.cmd_buf      = (unsigned int*)(context->screen->buffers->list[nIndex].address);
    context->cmdbuf.count_used   = 0;
    context->cmdbuf.count_reemit = 0;
    context->cmdbuf.nDMAindex    = nIndex;

    return iRet;
}

int r600FlushIndirectBuffer(context_t *context)
{
    int                   iRet;
    unsigned int         *dest;
    drm_radeon_indirect_t ind;

    if(0 == context->cmdbuf.count_used)
    {
        return 0;
    }

    if(context->cmdbuf.count_used < context->cmdbuf.size)
    {   /* not full cmd buf, need to check 16 DWORDs alignment. */
        dest = context->cmdbuf.cmd_buf + context->cmdbuf.count_used;

        while( (context->cmdbuf.count_used & 0xF) != 0 )
        {
            *dest = 0x80000000; /* NOP */
            context->cmdbuf.count_used++;
        };
    }

    ind.idx     = context->cmdbuf.nDMAindex;
    ind.start   = 0;
    ind.end     = context->cmdbuf.count_used * 4;
    ind.discard = 0;

    drmGetLock(context->fd, context->hwContext, DRM_LOCK_READY);

    iRet = drmCommandWriteRead(context->fd, 
                               DRM_RADEON_INDIRECT,
			                   &ind, 
                               sizeof(drm_radeon_indirect_t));

    drmUnlock(context->fd, context->hwContext);

    context->cmdbuf.count_used   = 0;
    context->cmdbuf.count_reemit = 0;

    return iRet;
}

void r600Flush(GLcontext * ctx)
{
    int ret;

    R600_CONTEXT;

    if(GL_TRUE == context->cmdbuf.bUseDMAasIndirect)
    {
        ret = r600FlushIndirectBuffer(context);
    }
    else
    {
        ret = r600FlushCmdBuffer(context);
    }

    if (ret)
    {
        fprintf(stderr, "drmRadeonCmdBuffer: %d\n", ret);
        _mesa_exit(ret);
    }
}

void r600InitCmdBufferFuncs(struct dd_function_table *functions)
{
	functions->Flush = r600Flush;
}
