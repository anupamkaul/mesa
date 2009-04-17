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

/*
 * Authors: Matthias Hopf
 */

#include "r600_span.h"

#include "swrast/swrast.h"

/* #include "r600_state.h" */
/* #include "r600_ioctl.h" */

#define DBG 0

/*
 * Note that all information needed to access pixels in a renderbuffer
 * should be obtained through the gl_renderbuffer parameter, not per-context
 * information.
 */
#define LOCAL_VARS						\
   driRenderbuffer *drb = (driRenderbuffer *) rb;		\
   const __DRIdrawablePrivate *dPriv = drb->dPriv;		\
   const GLuint bottom = dPriv->h - 1;				\
   GLubyte *buf = (GLubyte *) drb->flippedData			\
      + (dPriv->y * drb->flippedPitch + dPriv->x) * drb->cpp;	\
   GLuint p;							\
   (void) p;

#define LOCAL_DEPTH_VARS				\
   driRenderbuffer *drb = (driRenderbuffer *) rb;	\
   const __DRIdrawablePrivate *dPriv = drb->dPriv;	\
   const GLuint bottom = dPriv->h - 1;			\
   GLuint xo = dPriv->x;				\
   GLuint yo = dPriv->y;				\
   GLubyte *buf = (GLubyte *) drb->Base.Data;

#define LOCAL_STENCIL_VARS LOCAL_DEPTH_VARS

#define Y_FLIP(Y) (bottom - (Y))

#define HW_LOCK()

#define HW_UNLOCK()

/* ================================================================
 * Color buffer
 */

/* 16 bit, RGB565 color spanline and pixel functions */
#define SPANTMP_PIXEL_FMT GL_RGB
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_SHORT_5_6_5

#define TAG(x)    r600##x##_RGB565
#define TAG2(x,y) r600##x##_RGB565##y
#define GET_PTR(X,Y) (buf + ((Y) * drb->flippedPitch + (X)) * 2)
#include "spantmp2.h"

/* 32 bit, ARGB8888 color spanline and pixel functions */
#define SPANTMP_PIXEL_FMT GL_BGRA
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV

#define TAG(x)    r600##x##_ARGB8888
#define TAG2(x,y) r600##x##_ARGB8888##y
#define GET_PTR(X,Y) (buf + ((Y) * drb->flippedPitch + (X)) * 4)
#include "spantmp2.h"

/* ================================================================
 * Depth buffer
 */

/* 16-bit depth buffer functions */
#define VALUE_TYPE GLushort

#define GET_PTR(X,Y) ((GLushort *)(buf + ((Y) * drb->pitch + (X)) * 2))

#define WRITE_DEPTH( _x, _y, d )					\
    *(GET_PTR ((_x)+xo, (_y)+yo)) = d;

#define READ_DEPTH( d, _x, _y )						\
    d = *(GET_PTR ((_x)+xo, (_y)+yo));

#define TAG(x) r600##x##_z16
#include "depthtmp.h"
#undef GET_PTR

/* 24 bit depth, 8 bit stencil depthbuffer functions */
#define VALUE_TYPE GLuint

#define GET_PTR(X,Y) ((GLuint *)(buf + ((Y) * drb->pitch + (X)) * 4))
#define WRITE_DEPTH( _x, _y, d )					\
    do {								\
	GLuint *ptr = GET_PTR ((_x)+xo, (_y)+yo);			\
	*ptr = (*ptr & 0xff) | ((d) << 8);				\
    } while (0)

#define READ_DEPTH( d, _x, _y )		\
    d = (*(GET_PTR ((_x)+xo, (_y)+yo))) >> 8

#define TAG(x) r600##x##_z24_s8
#include "depthtmp.h"
#undef GET_PTR

/* ================================================================
 * Stencil buffer
 */

/* 24 bit depth, 8 bit stencil depthbuffer functions */
#define GET_PTR(X,Y) ((GLuint *)(buf + ((Y) * drb->pitch + (X)) * 4))
#define WRITE_STENCIL( _x, _y, d )					\
    do {								\
	GLuint *ptr = GET_PTR ((_x)+xo, (_y)+yo);			\
	*ptr = (*ptr & 0xffffff00) | ((d) & 0xff);			\
    } while (0)

#define READ_STENCIL( d, _x, _y )		\
    d = *(GET_PTR ((_x)+xo, (_y)+yo)) & 0xff

#define TAG(x) r600##x##_z24_s8
#include "stenciltmp.h"
#undef GET_PTR

/* Move locking out to get reasonable span performance (10x better
 * than doing this in HW_LOCK above).  WaitForIdle() is the main
 * culprit. */

static void
r600SpanRenderStart (GLcontext * ctx)
{
    R600_CONTEXT;
#if 0
    r600WaitForIdle  (context);				/* emit flush cache, emit fence, flush buffer, wait  */
    LOCK_HARDWARE    (context);
#endif
}

static void
r600SpanRenderFinish(GLcontext * ctx)
{
    R600_CONTEXT;

//    _swrast_flush   (ctx);  // implicit
#if 0
    r600EmitFlushInputCache (context, context->target.rt.gpu, context->target.rt.size);
    UNLOCK_HARDWARE (context);
#endif
}

void
r600InitSpanFuncs (GLcontext * ctx)
{
    struct swrast_device_driver *swdd =
	_swrast_GetDeviceDriverReference (ctx);
    swdd->SpanRenderStart  = r600SpanRenderStart;
    swdd->SpanRenderFinish = r600SpanRenderFinish;
}

/**
 * Plug in the Get/Put routines for the given driRenderbuffer.
 */
void
r600SetSpanFunctions (driRenderbuffer * drb, const GLvisual * vis)
{
    if (drb->Base.InternalFormat == GL_RGBA) {
	if (vis->redBits == 5 && vis->greenBits == 6
	    && vis->blueBits == 5) {
	    r600InitPointers_RGB565(&drb->Base);
	} else {
	    r600InitPointers_ARGB8888(&drb->Base);
	}
    } else if (drb->Base.InternalFormat == GL_DEPTH_COMPONENT16) {
	r600InitDepthPointers_z16(&drb->Base);
    } else if (drb->Base.InternalFormat == GL_DEPTH_COMPONENT24) {
	r600InitDepthPointers_z24_s8(&drb->Base);
    } else if (drb->Base.InternalFormat == GL_STENCIL_INDEX8_EXT) {
	r600InitStencilPointers_z24_s8(&drb->Base);
    }
}
