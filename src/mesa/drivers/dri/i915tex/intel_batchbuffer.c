/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "intel_batchbuffer.h"
#include "intel_frame_tracker.h"
#include "intel_fbo.h"
#include "intel_ioctl.h"
#include "intel_vb.h"
#include "intel_reg.h"
#include "intel_utils.h"
#include "intel_lock.h"
#include "intel_cmdstream.h"

/* needed for hwz:
 */
#include "i915_context.h"

#include <unistd.h>
#include <sys/mman.h>

/* Relocations in kernel space:
 *    - pass dma buffer seperately
 *    - memory manager knows how to patch
 *    - pass list of dependent buffers
 *    - pass relocation list
 *
 * Either:
 *    - get back an offset for buffer to fire
 *    - memory manager knows how to fire buffer
 *
 * Really want the buffer to be AGP and pinned.
 *
 */

/* Cliprect fence: The highest fence protecting a dma buffer
 * containing explicit cliprect information.  Like the old drawable
 * lock but irq-driven.  X server must wait for this fence to expire
 * before changing cliprects [and then doing sw rendering?].  For
 * other dma buffers, the scheduler will grab current cliprect info
 * and mix into buffer.  X server must hold the lock while changing
 * cliprects???  Make per-drawable.  Need cliprects in shared memory
 * -- beats storing them with every cmd buffer in the queue.
 *
 * ==> X server must wait for this fence to expire before touching the
 * framebuffer with new cliprects.
 *
 * ==> Cliprect-dependent buffers associated with a
 * cliprect-timestamp.  All of the buffers associated with a timestamp
 * must go to hardware before any buffer with a newer timestamp.
 *
 * ==> Dma should be queued per-drawable for correct X/GL
 * synchronization.  Or can fences be used for this?
 *
 * Applies to: Blit operations, metaops, X server operations -- X
 * server automatically waits on its own dma to complete before
 * modifying cliprects ???
 */
static void dump(struct intel_context *intel,
		 GLubyte *buffer_ptr,
		 GLuint offset, 
		 GLuint max_offset,
		 GLboolean is_batch )
{
   struct debug_stream stream;
   GLboolean done = GL_FALSE;

   stream.offset = offset;
   stream.ptr = buffer_ptr - offset;
   stream.print_addresses = !is_batch;

   while (!done &&
	  stream.offset < max_offset &&
	  stream.offset >= offset)
   {
      done = !intel->vtbl.debug_packet( &stream );
      if (!done && is_batch)
	 assert(stream.offset < max_offset &&
		stream.offset >= offset);
   }
}


const GLuint foo[] = {
0x7d800003, 0x00000000, 0x00000000, 0x001f003f, 0x00000000, 0x7d040ff7, 0x08300000, 0x04040000,
0xffffffff, 0x00000000, 0x00902440, 0x00000002, 0x00000006, 0x00000000, 0x7d071202, 0x2c20002b,
 0x2c201003, 0x00000006, 0x7f280005, 0x43000000, 0x42000000, 0x00000000, 0x42000000, 0x00000000,
 0x00000000, 0x7f820000, 0x00010000, 0xffff0002, 0x05000000, 0x00000000, 0x00000000, 0x00000000
};

static void dump_foo( struct intel_context *intel )
{
   GLuint offset = 0xeabcf000;
   _mesa_printf("FOO\n");
   dump(intel, foo, offset, offset + sizeof(foo), 0);
   _mesa_printf("END_FOO\n");
}


static void
intel_dump_batchbuffer(struct intel_batchbuffer *batch, 
		       GLubyte *batch_map,
		       GLubyte *state_map)
{
   {
      GLuint count = batch->segment_finish_offset[0];
      GLuint batch_offset = driBOOffset(batch->buffer);

      fprintf(stderr, "\n\nBATCH: (%d)\n", count / 4);
      dump( batch->intel, batch_map, batch_offset, 
	    batch_offset + 3 * SEGMENT_SZ,
	    GL_TRUE );
      fprintf(stderr, "END-BATCH\n\n\n");
   }

   {
      GLuint count = ( batch->segment_finish_offset[1] - 
		       batch->segment_start_offset[1] );

      GLuint dyn_offset = (driBOOffset(batch->state_buffer) 
			   + batch->segment_start_offset[1]);
      
      GLubyte *dyn_ptr = state_map + batch->segment_start_offset[1];

      fprintf(stderr, "\n\nDYNAMIC: (%d)\n", count / 4);
      dump( batch->intel, dyn_ptr, dyn_offset, dyn_offset + count,
	    GL_FALSE );
      fprintf(stderr, "END-DYNAMIC\n\n\n");
   }


   {
      GLuint count = ( batch->segment_finish_offset[2] - 
		       batch->segment_start_offset[2] );

      GLuint stat_offset = (driBOOffset(batch->state_buffer) 
			   + batch->segment_start_offset[2]);
      
      GLubyte *stat_ptr = state_map + batch->segment_start_offset[2];

      fprintf(stderr, "\n\nSTATIC: (%d)\n", count / 4);
      dump( batch->intel, stat_ptr, stat_offset, stat_offset + count,
	    GL_FALSE );
      fprintf(stderr, "END-STATIC\n\n\n");
   }
}

void
intel_batchbuffer_reset(struct intel_batchbuffer *batch)
{

   int i;

   /*
    * Get a new, free batchbuffer.
    */

   batch->size =  batch->intel->intelScreen->maxBatchSize;
   driBOData(batch->buffer, batch->size, NULL, 0);

   if (batch->state_buffer != batch->buffer) {
      driBOData(batch->state_buffer, 8192, NULL, 0);
   }

   driBOResetList(batch->list);

   /*
    * Unreference buffers previously on the relocation list.
    */

   for (i = 0; i < batch->nr_relocs; i++) {
      struct buffer_reloc *r = &batch->reloc[i];
      driBOUnReference(r->buf);
   }

   batch->list_count = 0;
   batch->nr_relocs = 0;
   batch->flags = 0;

   /*
    * We don't refcount the batchbuffer itself since we can't destroy it
    * while it's on the list.
    */


   driBOAddListItem(batch->list, batch->buffer,
                    DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
                    DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE);


   batch->state_map = batch->map = driBOMap(batch->buffer, DRM_BO_FLAG_WRITE, 0);

   if (batch->state_buffer != batch->buffer) {
      driBOAddListItem(batch->list, batch->state_buffer,
		       batch->state_memflags,
		       DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE);

      batch->state_map = driBOMap(batch->state_buffer, DRM_BO_FLAG_WRITE, 0);
   }

   batch->segment_finish_offset[0] = batch->segment_start_offset[0];
   batch->segment_finish_offset[1] = batch->segment_start_offset[1];
   batch->segment_finish_offset[2] = batch->segment_start_offset[2];

   batch->zone_init_offset = 0;
}

/*======================================================================
 * Public functions
 */
struct intel_batchbuffer *
intel_batchbuffer_alloc(struct intel_context *intel)
{
   struct intel_batchbuffer *batch = calloc(sizeof(*batch), 1);
   int page_size = getpagesize();

   batch->reloc = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

   assert (batch->reloc != MAP_FAILED);

   batch->max_relocs = page_size / sizeof(struct buffer_reloc);

   //dump_foo(intel);
   (void) dump_foo;
   
   batch->intel = intel;

   driGenBuffers(intel->intelScreen->batchPool, "batchbuffer", 1,
                 &batch->buffer, 4096,
                 DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE, 0);
   batch->last_fence = NULL;
   batch->list = driBOCreateList(20);

   if (intel->intelScreen->statePool /*drmMinor >= 10*/) {

      _mesa_printf("using statePool for state buffer\n");

      batch->state_memtype = 0 << 14;
      batch->state_memflags = DRM_BO_FLAG_MEM_PRIV1 | DRM_BO_FLAG_EXE;

      driGenBuffers(intel->intelScreen->statePool, "state", 1,
		    &batch->state_buffer, 4096, batch->state_memflags, 0);

      batch->segment_start_offset[0] = 0;
      batch->segment_start_offset[1] = 0;
      batch->segment_start_offset[2] = 4096;

      batch->segment_finish_offset[0] = batch->segment_start_offset[0];
      batch->segment_finish_offset[1] = batch->segment_start_offset[1];
      batch->segment_finish_offset[2] = batch->segment_start_offset[2];

      batch->segment_max_offset[0] = 1 * SEGMENT_SZ - BATCH_RESERVED;
      batch->segment_max_offset[1] = 4096;
      batch->segment_max_offset[2] = 8192;

      /* Manage a chunk of the much-abused batch buffer as pages for
       * the swz binner:
       */
      if (_mesa_getenv("INTEL_SWZ"))
	 intel_cmdstream_use_batch_range( intel, 
					  0 * SEGMENT_SZ,
					  3 * SEGMENT_SZ );      
   } else {
      batch->state_buffer = batch->buffer;
      batch->state_memtype = 1 << 14;
      batch->state_memflags = DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE;

      batch->segment_start_offset[0] = 0 * SEGMENT_SZ;
      batch->segment_start_offset[1] = 1 * SEGMENT_SZ;
      batch->segment_start_offset[2] = 2 * SEGMENT_SZ;

      batch->segment_finish_offset[0] = 0 * SEGMENT_SZ;
      batch->segment_finish_offset[1] = 1 * SEGMENT_SZ;
      batch->segment_finish_offset[2] = 2 * SEGMENT_SZ;

      batch->segment_max_offset[0] = 1 * SEGMENT_SZ - BATCH_RESERVED;
      batch->segment_max_offset[1] = 2 * SEGMENT_SZ;
      batch->segment_max_offset[2] = 3 * SEGMENT_SZ;

      /* Can't swz bin:
       */
      intel_cmdstream_use_batch_range( intel, 0, 0 );      

   }

   intel_batchbuffer_reset(batch);
   return batch;
}

void
intel_batchbuffer_free(struct intel_batchbuffer *batch)
{
   if (batch->last_fence) {
      driFenceFinish(batch->last_fence,
      DRM_FENCE_TYPE_EXE | DRM_I915_FENCE_TYPE_RW, GL_FALSE);
      driFenceUnReference(batch->last_fence);
      batch->last_fence = NULL;
   }
   if (batch->state_map && batch->state_map != batch->map) {
      driBOUnmap(batch->state_buffer);
      batch->state_map = NULL;
   }
   if (batch->map) {
      driBOUnmap(batch->buffer);
      batch->map = NULL;
   }
   if (batch->state_buffer != batch->buffer) {
      driBOUnReference(batch->state_buffer);
   }
   driBOUnReference(batch->buffer);
   driBOFreeList(batch->list);
   munmap(batch->reloc, (batch->max_relocs + 1) *
	  sizeof(struct buffer_reloc) & ~(getpagesize() - 1));
   free(batch);
}

/* TODO: Push this whole function into bufmgr.
 */
static void
do_flush_locked( struct intel_batchbuffer *batch,
		 GLuint used )
{
   GLuint *ptr, *batch_ptr, *state_ptr;
   GLuint i;
   struct intel_context *intel = batch->intel;
   struct intel_framebuffer *intel_fb = intel_get_fb( intel );
   unsigned fenceFlags;
   struct _DriFenceObject *fo;

   driBOValidateList(batch->intel->driFd, batch->list);

   /* Apply the relocations.  This nasty map indicates to me that the
    * whole task should be done internally by the memory manager, and
    * that dma buffers probably need to be pinned within agp space.
    */
   state_ptr = batch_ptr = (GLuint *) driBOMap(batch->buffer, DRM_BO_FLAG_WRITE,
					       DRM_BO_HINT_ALLOW_UNFENCED_MAP);

   if (batch->state_buffer != batch->buffer) {
	 state_ptr = (GLuint *) driBOMap(batch->state_buffer, DRM_BO_FLAG_WRITE,
					 DRM_BO_HINT_ALLOW_UNFENCED_MAP);
   }

   if (batch->zone_init_offset && !intel_fb->may_use_zone_init) {
      *(GLuint*)(batch->map + batch->zone_init_offset) =
	 (_3DPRIMITIVE | PRIM3D_CLEAR_RECT | 5);
   }

   for (i = 0; i < batch->nr_relocs; i++) {
      struct buffer_reloc *r = &batch->reloc[i];

      ptr = r->segment ? state_ptr : batch_ptr;

      ptr[r->offset / 4] = driBOOffset(r->buf) + r->delta;
      
      if (INTEL_DEBUG & DEBUG_BATCH) 
	 _mesa_printf("reloc in %s buffer %p offset %x value 0x%x + 0x%x\n",
		      r->segment ? "state" : "batch",
		      ptr, r->offset, driBOOffset(r->buf), r->delta);
   }

   if (INTEL_DEBUG & DEBUG_BATCH) {
      intel_dump_batchbuffer(batch, (GLubyte *)batch_ptr, (GLubyte *)state_ptr);

   }

   if (batch->state_buffer != batch->buffer) {
     driBOUnmap(batch->state_buffer);
   }
   driBOUnmap(batch->buffer);
   batch->state_map = batch->map = NULL;

   if (batch->flags & (INTEL_BATCH_HWZ|INTEL_BATCH_CLIPRECTS)) {
      UPDATE_CLIPRECTS(intel);

      if (intel->numClipRects) {
	 if (batch->flags & INTEL_BATCH_HWZ) {
	    struct i915_state *state = &i915_context( &intel->ctx )->current;
	    struct intel_framebuffer *intel_fb =
	       (struct intel_framebuffer *) intel->ctx.DrawBuffer;

	    intel_cliprect_hwz_ioctl(batch->intel,
				     intel_fb->pf_current_page,
				     driBOOffset(batch->buffer),
				     used,
				     driBOOffset(batch->state_buffer) +
				     state->offsets[0],
				     state->sizes[0]);
	 }
	 else {
	    intel_cliprect_batch_ioctl(batch->intel,
				       driBOOffset(batch->buffer),
				       used);
	 }
      }
      else {
	 intel_lost_hardware( intel );
      }
   }
   else {
      intel_batch_ioctl(batch->intel,
                        driBOOffset(batch->buffer),
                        used);
   }



   /*
    * Kernel fencing. The flags tells the kernel that we've 
    * programmed an MI_FLUSH.
    */
   
   fenceFlags = DRM_I915_FENCE_FLAG_FLUSHED;
   fo = driFenceBuffers(batch->intel->driFd,
			"Batch fence", fenceFlags, batch->list);

   if (driFenceType(fo) == DRM_FENCE_TYPE_EXE) {

     /*
      * Oops. We only validated a batch buffer. This means we
      * didn't do any proper rendering. Discard this fence object.
      */
      driFenceUnReference(fo);
   } else {
      driFenceUnReference(batch->last_fence);
      batch->last_fence = fo;
   }
}


struct _DriFenceObject *
intel_batchbuffer_flush(struct intel_batchbuffer *batch, 
			GLboolean forced )
{
   struct intel_context *intel = batch->intel;
   GLuint used = batch->segment_finish_offset[0] - batch->segment_start_offset[0];
   GLboolean was_locked = intel->locked;
   GLint *ptr = (GLint *)(batch->map + batch->segment_finish_offset[0]);
   struct intel_framebuffer *intel_fb =
      (struct intel_framebuffer*)intel->ctx.DrawBuffer;

   if (used == 0)
      return batch->last_fence;

   _mesa_printf("%s used %d relocs: %d\n", __FUNCTION__, used, batch->nr_relocs);

   assert(used < SEGMENT_SZ);

   intel_vb_flush( intel->vb );

   /* Add the MI_BATCH_BUFFER_END.  Always add an MI_FLUSH - this is a
    * performance drain that we would like to avoid.
    */
   if (intel_fb->hwz) {
      *ptr++ = MI_BATCH_BUFFER_END;
      used += 4;
      if (used & 4) {
	 *ptr++ = 0;
	 used += 4;
      }
   } else if (used & 4) {
      ptr[0] = intel->vtbl.flush_cmd();
      ptr[1] = 0;
      ptr[2] = MI_BATCH_BUFFER_END;
      used += 12;
   }
   else {
      ptr[0] = intel->vtbl.flush_cmd();
      ptr[1] = MI_BATCH_BUFFER_END;
      used += 8;
   }

   if (batch->state_buffer != batch->buffer) {
      driBOUnmap(batch->state_buffer);
   }
   driBOUnmap(batch->buffer);
   batch->state_map = batch->map = NULL;

   /* TODO: Just pass the relocation list and dma buffer up to the
    * kernel.
    */
   if (!was_locked) {
      LOCK_HARDWARE(intel);
      if (batch->flags & INTEL_BATCH_CLIPRECTS)
	 UPDATE_CLIPRECTS(intel);
   }

   do_flush_locked(batch, used);

   if (!was_locked)
      UNLOCK_HARDWARE(intel);

   /* Reset the buffer:
    */
   intel_batchbuffer_reset(batch);


   if (INTEL_DEBUG & DEBUG_ALWAYS_SYNC)
   {
      struct _DriFenceObject *fence = batch->last_fence;
      driFenceReference(fence);
      driFenceFinish(fence, 3, GL_FALSE);
      driFenceUnReference(fence);
   }

   return batch->last_fence;
}

void
intel_batchbuffer_finish(struct intel_batchbuffer *batch)
{
   struct _DriFenceObject *fence = intel_batchbuffer_flush(batch, GL_TRUE);
   driFenceReference(fence);
   driFenceFinish(fence, 3, GL_FALSE);
   driFenceUnReference(fence);
}


/*  This is the only way buffers get added to the validate list.
 */
GLboolean
intel_batchbuffer_set_reloc(struct intel_batchbuffer *batch,
			    GLuint segment, GLuint offset,
			    struct _DriBufferObject *buffer,
			    GLuint flags, GLuint mask, GLuint delta)
{
   assert((offset & 3) == 0);

   driBOAddListItem(batch->list, buffer, flags, mask);

   if (batch->nr_relocs == batch->max_relocs) {
      int page_size = getpagesize();
      int pages = (batch->max_relocs + 1) * sizeof(struct buffer_reloc) /
	 page_size;

      batch->reloc = mremap(batch->reloc, pages * page_size, (pages + 1) *
			    page_size, MREMAP_MAYMOVE);

      assert (batch->reloc != MAP_FAILED);

      batch->max_relocs = (pages + 1) * page_size / sizeof(struct buffer_reloc);
   }

   {
      struct buffer_reloc *r = &batch->reloc[batch->nr_relocs++];
      driBOReference(buffer);
      r->buf = buffer;
      r->offset = offset;
      r->delta = delta;
      r->segment = segment;
   }

   return GL_TRUE;
}


GLboolean
intel_batchbuffer_emit_reloc(struct intel_batchbuffer *batch,
			     GLuint segment,
                             struct _DriBufferObject *buffer,
                             GLuint flags, GLuint mask, GLuint delta)
{
   GLboolean success;

   success = intel_batchbuffer_set_reloc( batch, segment,
					  batch->segment_finish_offset[segment],
					  buffer, flags, mask, delta );

   assert(success);
   batch->segment_finish_offset[segment] += 4;

   return success;
}


void
intel_batchbuffer_data(struct intel_batchbuffer *batch,
		       GLuint segment,
                       const void *data, GLuint bytes, GLuint flags)
{
   assert((bytes & 3) == 0);
   intel_batchbuffer_require_space(batch, segment, bytes, flags);
   __memcpy(batch->map + batch->segment_finish_offset[segment], data, bytes);
   batch->segment_finish_offset[segment] += bytes;
}

void
intel_batchbuffer_wait_last_fence(struct intel_batchbuffer *batch )
{
   if (batch->last_fence)  
   {
      driFenceReference(batch->last_fence);
      driFenceFinish(batch->last_fence, 
		     DRM_FENCE_TYPE_EXE | DRM_I915_FENCE_TYPE_RW, GL_FALSE);
      driFenceUnReference(batch->last_fence);
      //???
      //batch->last_fence = NULL;
   }
}
