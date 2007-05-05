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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */
      
#include "i915_context.h"
#include "i915_cache.h"
#include "i915_reg.h"
#include "intel_batchbuffer.h"

/* Currently things are just being stuffed into the end of the
 * batchbuffer.  That's not great, but it works well for relocations.
 *
 * In future, keep non-relocation structs elsewhere, but want a way to
 * be able to emit more (ie map a referenced buffer) without waiting
 * on the fence - this should be possible as we currently do this with
 * batchbuffers.
 */

struct i915_cache_item {
   GLuint hash;
   GLuint size;		
   const void *data;
   GLuint offset;		/* relative to the batchbuffer start */
   struct i915_cache_item *next;
};   

struct i915_cache {
   GLuint id;   
   struct i915_cache_item **items;
   GLuint size, n_items;
};

struct i915_cache_context {
   struct i915_context *i915;
   struct i915_cache cache[I915_MAX_CACHE];
};



static GLuint emit_packet( struct intel_context *intel,
			   const struct i915_cache_packet *packet )
{
   GLuint size = packet->nr_dwords * sizeof(GLuint);
   GLuint segment = SEGMENT_OTHER_INDIRECT;
   GLuint offset = intel->batch->segment_finish_offset[segment];
   GLuint i;

   /* This should not be possible:
    */
   assert(intel->batch->segment_finish_offset[segment] + size <
	  intel->batch->segment_max_offset[segment]);

   intel->batch->segment_finish_offset[segment] += size;
   
   memcpy(intel->batch->state_map + offset, 
	  &packet->dword[0], 
	  size );

   for (i = 0; i < packet->nr_relocs; i++) {
      intel_batchbuffer_set_reloc( intel->batch, segment,
				   offset + packet->reloc[i].dword * sizeof(GLuint),
				   packet->reloc[i].buffer,
				   packet->reloc[i].flags,
				   packet->reloc[i].mask,
				   packet->dword[packet->reloc[i].dword].u );
   }

   return offset;
}


/* A cache for packets of variable sizes which are held in one of the
 * indirect state bins.  
 *
 * Note that dynamic indirect state is special and handled elsewhere.
 */
static GLuint hash_packet( const void *packet, GLuint size )
{
   GLuint hash = 0, i;

   assert(size % 4 == 0);

   /* I'm sure this can be improved on:
    */
   for (i = 0; i < size/4; i++)
      hash += ((GLuint *)packet)[i];

   return hash;
}


static void rehash( struct i915_cache *cache )
{
   struct i915_cache_item **items;
   struct i915_cache_item *c, *next;
   const GLuint size = cache->size * 2;
   GLuint i;

   items = (struct i915_cache_item**) _mesa_malloc(size * sizeof(*items));
   _mesa_memset(items, 0, size * sizeof(*items));

   for (i = 0; i < cache->size; i++) {
      for (c = cache->items[i]; c; c = next) {
	 next = c->next;
	 c->next = items[c->hash & (size-1)];
	 items[c->hash & (size-1)] = c;
      }
   }

   FREE(cache->items);
   cache->items = items;
   cache->size = size;
}



static GLuint search_cache( struct i915_cache *cache,
			    GLuint hash,
			    const void *data,
			    GLuint size)
{
   struct i915_cache_item *c;
   GLuint mask = cache->size - 1;

   for (c = cache->items[hash & mask]; c; c = c->next) {
      if (c->hash == hash && 
	  c->size == size &&
	  memcmp(c->data, data, size) == 0) 
      {
	 return c->offset;
      }
   }

   return 0;
}


static GLuint upload_cache( struct i915_cache *cache,
			    struct intel_context *intel,
			    GLuint hash,
			    const struct i915_cache_packet *packet,
			    GLuint size )
{   
   struct i915_cache_item *item = CALLOC_STRUCT(i915_cache_item);
   void *data = _mesa_malloc(size);
   
   memcpy(data, packet->dword, size);

   item->data = data;
   item->hash = hash;
   item->size = size;

   if (++cache->n_items > cache->size)
      rehash(cache);
   
   hash &= cache->size - 1;
   item->next = cache->items[hash];
   cache->items[hash] = item;

   /* Copy data to the buffer and emit relocations:
    */
   item->offset = emit_packet( intel, packet );
      
   return item->offset;
}


/* Returns the size of the in-use packet:
 */
static GLuint packet_size( const struct i915_cache_packet *packet ) 
{
   return ((const char *)(packet->reloc + packet->nr_relocs) -
	   (const char *)(packet->dword));
}


void i915_cache_emit(struct i915_cache_context *cctx,
		       const struct i915_cache_packet *packet )
{
   struct intel_context *intel = &cctx->i915->intel;
   GLuint size = packet_size( packet );

#if 0
   GLuint i;

   BEGIN_BATCH(packet->nr_dwords, 0);
   for (i = 0; i < packet->nr_relocs; i++) 
      intel_batchbuffer_set_reloc( intel->batch, packet->reloc[i].segment,
				   ( intel->batch->segment_finish_offset[0] + 
				     packet->reloc[i].dword * sizeof(GLuint) ),
				   packet->reloc[i].buffer,
				   packet->reloc[i].flags,
				   packet->reloc[i].mask,
				   packet->dword[packet->reloc[i].dword].u );

   for (i = 0; i < packet->nr_dwords; i++)
      OUT_BATCH(packet->dword[i].u);

   ADVANCE_BATCH();
#else
   GLuint hash = hash_packet( packet, size );
   struct i915_cache *cache = &cctx->cache[packet->cache_id];
   GLuint addr;

   assert(packet->nr_dwords == packet->max_dwords);

   addr = search_cache( cache, hash, packet->dword, size );
   if (addr == 0)
      addr = upload_cache( cache, intel, hash, packet, size );

   cctx->i915->current.offsets[packet->cache_id] = addr;
   cctx->i915->current.sizes[packet->cache_id] = packet->nr_dwords;

   cctx->i915->hardware_dirty |= I915_HW_INDIRECT;
#endif
}



/* When we lose hardware context, need to invalidate the surface cache
 * as these structs must be explicitly re-uploaded.  They are subject
 * to fixup by the memory manager as they contain absolute agp
 * offsets, so we need to ensure there is a fresh version of the
 * struct available to receive the fixup.
 *
 * Also, temporarily, need to do this for every cache because they are
 * all being stored in the batchbuffer!!
 */
static void clear_cache( struct i915_cache *cache )
{
   struct i915_cache_item *c, *next;
   GLuint i;

   for (i = 0; i < cache->size; i++) {
      for (c = cache->items[i]; c; c = next) {
	 next = c->next;
	 free((void *)c->data);
	 free(c);
      }
      cache->items[i] = NULL;
   }

   cache->n_items = 0;
}



static void init_cache( struct i915_cache_context *cctx, 
			GLuint id,
			GLuint state_type )
{
   struct i915_cache *cache = &cctx->cache[id];

   assert(state_type == (LI0_STATE_STATIC_INDIRECT << id));

   cache->id = id;
   cache->size = 32;
   cache->n_items = 0;
   cache->items = ((struct i915_cache_item **)
		   _mesa_calloc(cache->size * sizeof(cache->items[0])));
}

struct i915_cache_context *i915_create_caches( struct i915_context *i915 )
{
   struct i915_cache_context *cctx = CALLOC_STRUCT(i915_cache_context);

   cctx->i915 = i915;

   init_cache( cctx,
	       I915_CACHE_STATIC,
	       LI0_STATE_STATIC_INDIRECT );

   init_cache( cctx,
	       I915_CACHE_MAP,
	       LI0_STATE_MAP );

   init_cache( cctx,
	       I915_CACHE_SAMPLER,
	       LI0_STATE_SAMPLER );

   init_cache( cctx,
	       I915_CACHE_PROGRAM,
	       LI0_STATE_PROGRAM );

   init_cache( cctx,
	       I915_CACHE_CONSTANTS,
	       LI0_STATE_CONSTANTS );

   return cctx;
}


void i915_clear_caches( struct i915_cache_context *cctx )
{
   GLint i;

   for (i = 0; i < I915_MAX_CACHE; i++) {
      clear_cache(&cctx->cache[i]);      

      cctx->i915->current.offsets[i] = 0;
      cctx->i915->current.sizes[i] = 0;
   }
}


void i915_destroy_caches( struct i915_cache_context *cctx )
{
   GLint i;

   for (i = 0; i < I915_MAX_CACHE; i++) {
      clear_cache(&cctx->cache[i]);
      FREE(cctx->cache[i].items);
   }
   
   FREE(cctx);
}
