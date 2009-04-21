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

#ifndef _R600_COMMON_H
#define _R600_COMMON_H

#include "glheader.h"

#include <xf86drm.h>


extern int debug;

#define DEBUG_FUNC  do { if (debug >= 1) fprintf (stderr, "[r600] %s   (%s:%d)\n", __func__, __FILE__, __LINE__); } while (0)
#define DEBUG_FUNCF(x...)  do { if (debug >= 1) { fprintf (stderr, "[r600] %s   (%s:%d)   ", __func__, __FILE__, __LINE__); fprintf (stderr, x); } } while (0)
#define DEBUGF(x...)  do { if (debug >= 1) { fprintf (stderr, "[r600]    " x); } } while (0)
#define DEBUGP(x...)  do { if (debug >= 2) { fprintf (stderr, "" x); } } while (0)


#if __SIZEOF_LONG__ == 8
#  define PRINTF_INT64          "%ld"
#  define PRINTF_UINT64         "%lu"
#  define PRINTF_UINT64_HEX     "%010lx"		/* Yes, 64bit need 16 characters, but typically 10 are enough (mostly addresses) */
#else
#  define PRINTF_INT64          "%lld"
#  define PRINTF_UINT64         "%llu"
#  define PRINTF_UINT64_HEX     "%010llx"
#endif

typedef int16_t bool_t;

typedef struct screen_s  screen_t;
typedef struct context_s context_t;


/* Preambles */
#define R600_CONTEXT    context_t *context = (context_t *) ctx->DriverCtx

#define R700_CONTEXT(ctx) ((context_t *)(ctx->DriverCtx))

/* Reference to GPU address only (struct for easy upgrade to target_t/mem_t) */
typedef struct {
    uint64_t     gpu;
} gpu_t;

/* Memory area, addressable on both CPU and GPU */
typedef struct {
    uint64_t     gpu;
    void        *cpu;
    uint32_t     size;
} mem_t;

/* Render target */
typedef struct {
    uint64_t     gpu;
    void        *cpu;
    uint32_t     size;					/* total, in bytes */
    uint32_t     pitch;					/* in pixels */
} target_t;

/* Memory area, addressable on both CPU and GPU, mapped by DRM */
typedef struct {
    uint64_t     gpu;
    void        *cpu;
    drmSize      size;
    drm_handle_t handle;
} memmap_t;

/* Buffer to be sent to DRM */
typedef struct {
    uint32_t    *data;
    int          pos;					/* Current writing pos in # of uint32_t */
    int          size;					/* Total size in # of uint32_t */
    uint32_t     fence;					/* !=0: wait for fence before reuse */
} buffer_t;

/* Color formats */
typedef struct {
    float        a, r, g, b;
} color_f_t;

typedef union {
    struct {
	uint8_t  a, r, g, b;				/* TODO: probably not endian aware */
    }            u8;
    uint32_t     u32;
} color_i_t;


/* Emit commands to set according to state. Only if state is active, though.
 * May verify for (partially) changed state unless force == 1 (context lost). */
typedef void (*emit_t) (context_t *context, void *state, int force);


#endif	/* _R600_COMMON_H */
