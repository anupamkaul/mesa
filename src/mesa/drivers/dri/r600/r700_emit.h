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
 
#ifndef __R700_EMIT_H__
#define __R700_EMIT_H__

#include "r700_chip.h"

/* R6xx knows 2 ways to set registers: 
 * 1. Packet0, 
 * 2. A set of Packet3 commands,
 * In current file, register setting use Packet3 commands.
 */
 
/* R700_PM4_PACKET0 uses register index rather than offset. */
#define R700_PM4_PACKET0(reg, n)    (R700_PM4_PACKET0_NOP | ((n)<<16) | (reg))
#define R700_PM4_PACKET3(cmd, n)    (R700_PM4_PACKET3_NOP | ((n)<<16) | ((cmd)<<8))

#define R700_CMDBUF_BEGIN                                               \
        uint32_t *dest = context->cmdbuf.cmd_buf + context->cmdbuf.count_used;

#define R700_CMDBUF_CHECK_SPACE(n)                                      \
    do                                                                  \
    {                                                                   \
        if (context->cmdbuf.count_used + (n) > context->cmdbuf.size)    \
        {                                                               \
            fprintf (stderr, "Insufficient cmdbuffer size-aborting\n"); \
            _mesa_exit(-1);                                             \
        }                                                               \
    } while (0)

#define R700_CMDBUF_END(n)                                              \
    do                                                                  \
    {                                                                   \
        context->cmdbuf.count_used += (n);                              \
    } while (0)

/* Emit a single uint32_t value */
#define R700E32(context, dword)                                         \
    do                                                                  \
    {                                                                   \
        R700_CMDBUF_BEGIN;                                              \
        R700_CMDBUF_CHECK_SPACE(1);                                     \
        *dest = (dword);                                                \
        R700_CMDBUF_END(1);                                             \
    } while(0)

/* Emit packet 3 header */
#define R700EP3(context, cmd, num)                                      \
    do                                                                  \
    {                                                                   \
        R700E32 (context, R700_PM4_PACKET3 ((cmd), (num)));             \
    }while(0)

/* Emit packet 0 header */
#define R700EP0(context, cmd, num)                                      \
    do                                                                  \
    {                                                                   \
        R700E32 (context, R700_PM4_PACKET0 ((cmd), (num)));             \
    }while(0)

#endif /* __R700_EMIT_H__ */
