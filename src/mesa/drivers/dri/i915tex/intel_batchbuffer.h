#ifndef INTEL_BATCHBUFFER_H
#define INTEL_BATCHBUFFER_H

#include "mtypes.h"
#include "dri_bufmgr.h"

struct intel_context;

/* Must be able to hold at minimum VB->Size * 3 * 2 bytes for
 * intel_idx_render.c indices, which is currently about 20k.
 */
#define BATCH_SZ (3*32*1024)
#define SEGMENT_SZ (32*1024)
#define BATCH_RESERVED 16

#define MAX_RELOCS 400

#define INTEL_BATCH_NO_CLIPRECTS 0x1
#define INTEL_BATCH_CLIPRECTS    0x2

struct buffer_reloc
{
   struct _DriBufferObject *buf;
   GLuint offset;
   GLuint delta;                /* not needed? */
   GLuint segment;
};

enum {
   SEGMENT_IMMEDIATE = 0,
   SEGMENT_DYNAMIC_INDIRECT = 1,
   SEGMENT_OTHER_INDIRECT = 2,
   NR_SEGMENTS = 3
};

struct intel_batchbuffer
{
   struct bufmgr *bm;
   struct intel_context *intel;

   struct _DriBufferObject *buffer;
   struct _DriBufferObject *state_buffer;
   struct _DriFenceObject *last_fence;
   GLuint flags;
   GLuint state_memtype;
   GLuint state_memflags;

   drmBOList list;
   GLuint list_count;
   GLubyte *map;
   GLubyte *state_map;

   struct buffer_reloc reloc[MAX_RELOCS];
   GLuint nr_relocs;
   GLuint size;

   /* Put all the different types of packets into one buffer for
    * easier validation.  This will have to change, but for now it is
    * enough to get started.
    */
   GLuint segment_start_offset[NR_SEGMENTS];
   GLuint segment_finish_offset[NR_SEGMENTS];
   GLuint segment_max_offset[NR_SEGMENTS];
};

struct intel_batchbuffer *intel_batchbuffer_alloc(struct intel_context
                                                  *intel);

void intel_batchbuffer_free(struct intel_batchbuffer *batch);


void intel_batchbuffer_finish(struct intel_batchbuffer *batch);

struct _DriFenceObject *intel_batchbuffer_flush(struct intel_batchbuffer
                                                *batch);

void intel_batchbuffer_reset(struct intel_batchbuffer *batch);


/* Unlike bmBufferData, this currently requires the buffer be mapped.
 * Consider it a convenience function wrapping multple
 * intel_buffer_dword() calls.
 */
void intel_batchbuffer_data(struct intel_batchbuffer *batch,
			    GLuint segment,
                            const void *data, GLuint bytes, GLuint flags);

void intel_batchbuffer_release_space(struct intel_batchbuffer *batch,
				     GLuint segment,
                                     GLuint bytes);

GLboolean
intel_batchbuffer_set_reloc(struct intel_batchbuffer *batch,
			    GLuint segment, GLuint offset,
			    struct _DriBufferObject *buffer,
			    GLuint flags, GLuint mask, GLuint delta);

GLboolean intel_batchbuffer_emit_reloc(struct intel_batchbuffer *batch,
				       GLuint segment,
                                       struct _DriBufferObject *buffer,
                                       GLuint flags,
                                       GLuint mask, GLuint offset);

/* Inline functions - might actually be better off with these
 * non-inlined.  Certainly better off switching all command packets to
 * be passed as structs rather than dwords, but that's a little bit of
 * work...
 */
static INLINE GLuint
intel_batchbuffer_space(struct intel_batchbuffer *batch,
			GLuint segment)
{
   return (batch->segment_max_offset[segment] - 
	   batch->segment_finish_offset[segment]);
}


static INLINE void
intel_batchbuffer_emit_dword(struct intel_batchbuffer *batch, 
			     GLuint segment,
			     GLuint dword)
{
   assert(batch->map);
   assert(intel_batchbuffer_space(batch, segment) >= 4);
   *(GLuint *) (batch->map + batch->segment_finish_offset[segment]) = dword;
   batch->segment_finish_offset[segment] += 4;
}

static INLINE void
intel_batchbuffer_require_space(struct intel_batchbuffer *batch,
				GLuint segment,
                                GLuint sz, GLuint flags)
{
   /* XXX:  need to figure out flushing, etc.
    */
   assert(sz < SEGMENT_SZ);

   if (intel_batchbuffer_space(batch, segment) < sz ||
       (batch->flags != 0 && flags != 0 && batch->flags != flags))
      intel_batchbuffer_flush(batch);

   batch->flags |= flags;
}

/* Here are the crusty old macros, to be removed:
 */
#define BATCH_LOCALS


/* Hack for indirect emit:
 */
#define BEGIN_BATCH_SEGMENT(seg, n, flags) do {				\
   assert(!intel->prim.flush);					\
   intel_batchbuffer_require_space(intel->batch, seg, (n)*4, flags);	\
   if (0) _mesa_printf("BEGIN_BATCH(%d,%d,%d) in %s\n", seg, n, flags, __FUNCTION__); \
} while (0)

#define OUT_BATCH_SEGMENT(seg, d) do {				\
      if (0) _mesa_printf("OUT_BATCH(%d, 0x%08x)\n", seg, d);  		\
      intel_batchbuffer_emit_dword(intel->batch, seg, d);	\
} while (0)

#define OUT_BATCH_F_SEGMENT(seg, fl) do {			\
   fi_type fi;					\
   fi.f = fl;					\
   if (0) _mesa_printf("OUT_BATCH(%d, 0x%08x)\n", seg, fi.i);  \
   intel_batchbuffer_emit_dword(intel->batch, seg, fi.i);	\
} while (0)

#define OUT_RELOC_SEGMENT(seg, buf,flags,mask,delta) do {				\
   assert((delta) >= 0);						\
   if (0) _mesa_printf("OUT_RELOC( seg %d buf %p offset %x )\n", seg, buf, delta);		\
   intel_batchbuffer_emit_reloc(intel->batch, seg, buf, flags, mask, delta);	\
} while (0)

#define ADVANCE_BATCH_SEGMENT(seg) do { \
   if (0) _mesa_printf("ADVANCE_BATCH()\n");		\
} while(0)


#define BEGIN_BATCH(n, flags)           BEGIN_BATCH_SEGMENT(0, n, flags)
#define OUT_BATCH(d)                    OUT_BATCH_SEGMENT(0, d)
#define OUT_BATCH_F(fl)                 OUT_BATCH_F_SEGMENT(0, fl)
#define OUT_RELOC(buf,flags,mask,delta) OUT_RELOC_SEGMENT(0,buf,flags,mask, delta)
#define ADVANCE_BATCH()                 ADVANCE_BATCH_SEGMENT(0)



#endif
