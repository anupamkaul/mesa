/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright 2006, 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIAIOCTL_H
#define _VIAIOCTL_H

#include "drm.h"
#include "ochr_drm.h"
#include "dri_util.h"
#include "main/context.h"
#include "wsbm_driver.h"
#include "assert.h"

#define VIA_DMA_BUFSIZ                  32768
#define VIA_LOSTSTATEDMA_BUFSIZ         4096
#define VIA_DMA_HIGHWATER               (VIA_DMA_BUFSIZ - 128)
#define VIA_LOSTSTATEDMA_HIGHWATER      (VIA_LOSTSTATEDMA_BUFSIZ - 128)
#define VIA_NO_CLIPRECTS 0x1

#define VIA_BLIT_CLEAR 0x00
#define VIA_BLIT_COPY 0xCC
#define VIA_BLIT_FILL 0xF0
#define VIA_BLIT_SET 0xFF

struct _ViaDrmValidateNode
{
    struct _ValidateNode base;
    struct drm_via_validate_arg val_arg;
};

extern struct _WsbmVNodeFuncs *viaVNodeFuncs(void);

static inline struct drm_via_validate_req *
viaValReq(struct _ValidateNode *node)
{
    assert(node->driver_private == 0);
    return &(containerOf(node, struct _ViaDrmValidateNode, base)->
	     val_arg.d.req);
}

struct via_reloc_texlist
{
    struct _WsbmBufferObject *buf;
    uint32_t delta;
};

struct via_context;
struct via_reloc_bufinfo;
struct via_reloc_savestate;

extern struct via_reloc_bufinfo *via_create_reloc_buffer(void);
extern void via_free_reloc_buffer(struct via_reloc_bufinfo *info);
extern int
via_add_reloc(struct via_reloc_bufinfo *info, void *reloc, size_t size);
extern struct via_reloc_savestate *via_reloc_save_state_alloc(void);
extern void via_reloc_state_save(struct via_reloc_bufinfo *info,
				 struct via_reloc_savestate *save_state);
extern void via_reloc_state_restore(struct via_reloc_savestate *save_state);

extern int via_reset_cmdlists(struct via_context *vmesa);

GLboolean viaCreateSafeClip(struct drm_via_clip_rect *r, int x, int y,
			    int w, int h);
void viaFinishPrimitive(struct via_context *vmesa);
void viaFlushDma(struct via_context *vmesa);
void via_execbuf(struct via_context *vmesa, GLuint flags);

void viaInitIoctlFuncs(GLcontext * ctx);
void viaCopyBuffer(__DRIdrawablePrivate * dpriv);
void viaPageFlip(__DRIdrawablePrivate * dpriv);
void viaCheckDma(struct via_context *vmesa, GLuint bytes);
void viaResetPageFlippingLocked(struct via_context *vmesa);

extern int via_depth_relocation(struct via_context *vmesa,
				uint32_t ** cmdbuf,
				struct _WsbmBufferObject *depthBuffer,
				uint64_t flags, uint64_t mask);

extern int via_2d_relocation(struct via_context *vmesa,
			     uint32_t ** cmdbuf,
			     struct _WsbmBufferObject *buffer,
			     uint32_t delta,
			     uint32_t bpp, uint32_t pos,
			     uint64_t flags, uint64_t mask);

extern int via_tex_relocation(struct via_context *vmesa,
			      uint32_t ** cmdbuf,
			      const struct via_reloc_texlist *addr,
			      uint32_t low_mip,
			      uint32_t hi_mip,
			      uint32_t reg_tex_fm,
			      uint64_t flags, uint64_t mask);

/*
 * Utils.
 */

extern int
via_intersect_drm_rect(struct drm_via_clip_rect *out,
		       struct drm_via_clip_rect *a, struct drm_clip_rect *b);
extern int
via_intersect_via_rect(struct drm_via_clip_rect *out,
		       struct drm_via_clip_rect *a,
		       struct drm_via_clip_rect *b);
extern void viaBlit(struct via_context *vmesa, GLuint bpp,
		    struct _WsbmBufferObject *srcBuf,
		    struct _WsbmBufferObject *dstBuf, GLuint srcDelta,
		    GLuint srcPitch, GLuint dstDelta, GLuint dstPitch,
		    GLint xdir, GLint ydir, GLuint w, GLuint h,
		    GLuint blitMode, GLuint color, GLuint nMask);

#define VIA_FINISH_PRIM(vmesa) do {		\
   if (vmesa->dmaLastPrim)			\
      viaFinishPrimitive( vmesa );		\
} while (0)

#define VIA_FLUSH_DMA(vmesa) do {		\
   VIA_FINISH_PRIM(vmesa);			\
   if (vmesa->dmaLow)		\
      viaFlushDma(vmesa);			\
} while (0)

void viaWrapPrimitive(struct via_context *vmesa);

extern void via_drop_cmdbuf(struct via_context *vmesa);

#define RING_VARS GLuint *_vb = 0, _nr, _x;

#define BEGIN_RING(n) do {				\
   if (_vb != 0) abort();				\
   _vb = viaAllocDma(vmesa, (n) * sizeof(GLuint));	\
   _nr = (n);						\
   _x = 0;						\
} while (0)

#define BEGIN_STATE_RING(n, _isLostState) do {				\
	if (_vb != 0) abort();						\
	if (!(_isLostState))						\
	    _vb = viaAllocDma(vmesa, (n) * sizeof(GLuint));		\
	else								\
	    _vb = viaAllocLostStateDma(vmesa, (n) * sizeof(GLuint));	\
	_nr = (n);							\
	_x = 0;								\
    } while (0)

#define BEGIN_RING_NOCHECK(n) do {			\
   if (_vb != 0) abort();				\
   _vb = (GLuint *)(vmesa->dma + vmesa->dmaLow);	\
   vmesa->dmaLow += (n) * sizeof(GLuint);		\
   _nr = (n);						\
   _x = 0;						\
} while (0)

#define OUT_RING(n) _vb[_x++] = (n)
#define ACCESS_RING() ((uint32_t *) &_vb[_x])
#define FINISH_ACCESS_RING(n) {_x += (n);}

#define ADVANCE_RING() do {			\
   if (_x != _nr) abort();			\
   _vb = 0;						\
} while (0)

#define ADVANCE_RING_VARIABLE() do {			\
   if (_x > _nr) abort();				\
   vmesa->dmaLow -= (_nr - _x) * sizeof(GLuint);	\
   _vb = 0;						\
} while (0)

#define ADVANCE_STATE_RING_VARIABLE(_lostState) do {	\
   if (_x > _nr) abort();				\
   if (_lostState)						\
     vmesa->lostStateDmaLow -= (_nr - _x) * sizeof(GLuint);	\
   else								\
     vmesa->dmaLow -= (_nr - _x) * sizeof(GLuint);		\
   _vb = 0;							\
  } while (0)

#define QWORD_PAD_RING() do {			\
   if (vmesa->dmaLow & 0x4) {			\
      BEGIN_RING(1);				\
      OUT_RING(HC_DUMMY);			\
      ADVANCE_RING();				\
   }						\
} while (0)

#define QWORD_PAD_LOSTSTATE_RING() do {			\
	if (vmesa->lostStateDmaLow & 0x4) {		\
	    BEGIN_STATE_RING(1,1);			\
	    OUT_RING(HC_DUMMY);				\
	    ADVANCE_RING();				\
	}						\
    } while (0)

#define VIA_GEQ_WRAP(left, right) \
    (((left) - (right)) < ( 1 << 23))

#define VIA_OFFSET_ALIGN_BYTES 16

#endif
