/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "intel_cmdstream.h"
#include "intel_batchbuffer.h"

GLubyte *intel_cmdstream_alloc_block( struct intel_context *intel )
{
   if (intel->cmdstream.used + CMDSTREAM_SIZE < intel->cmdstream.size) {
      GLubyte *ptr = intel->cmdstream.map + intel->cmdstream.used;
      intel->cmdstream.used += CMDSTREAM_SIZE;
      assert(ptr);
      return ptr;
   }
   else {
      return NULL;
   }
}

/* Use a chunk of the batchbuffer for our bin pool.  This way
 * relocations more or less continue to work.
 */
void intel_cmdstream_use_batch_range( struct intel_context *intel,
				      GLuint start_offset,
				      GLuint finish_offset )
{
   intel->cmdstream.offset = start_offset;
   intel->cmdstream.size = finish_offset - start_offset;
   intel->cmdstream.used = 0;
   intel->cmdstream.map = 0;
}

/* Called on batchbuffer flushes
 */
void intel_cmdstream_reset( struct intel_context *intel )
{
   if (0) 
      _mesa_printf("%s used %x of %x\n", __FUNCTION__, 
		   intel->cmdstream.used, intel->cmdstream.size);

   intel->cmdstream.map = intel->batch->map + intel->cmdstream.offset;
   intel->cmdstream.used = 0;

   assert((((unsigned long)intel->cmdstream.map) & (CMDSTREAM_SIZE-1)) == 0);
}


