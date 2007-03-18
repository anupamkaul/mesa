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
      
#ifndef I915_CACHE_H
#define I915_CACHE_H

struct i915_cache_context;
struct i915_cache_packet;
struct i915_context;


struct i915_cache_context *i915_create_caches( struct i915_context *i915 );

void i915_destroy_caches( struct i915_cache_context *cctx );
void i915_clear_caches( struct i915_cache_context *cctx );

void i915_cache_emit( struct i915_cache_context *cctx,
		      const struct i915_cache_packet *packet );


/* A much of helpers for building packets containing relocation
 * information.  
 * 
 * Initially allocate and fill in a maximally sized packet, as big as
 * any that we ever submit to hardware.  
 *
 * Once cached, we will store a compacted version of this packet, but
 * this is a convient interface for clients to build & submit packets.
 */
#define PACKET_MAX_DWORDS (I915_MAX_CONSTANT * 4 + 4)
#define PACKET_MAX_RELOCS (I915_TEX_UNITS)

struct i915_cache_reloc {
   struct _DriBufferObject *buffer;
   GLuint flags;
   GLuint mask;
   GLuint dword;
};

union fi {
   GLfloat f;
   GLuint u;
   GLint i;
};

struct i915_cache_packet {   
   GLuint nr_dwords;
   GLuint nr_relocs;
   GLuint max_dwords;
   GLuint cache_id;

   union fi dword[PACKET_MAX_DWORDS + 
		  PACKET_MAX_RELOCS * sizeof(struct i915_cache_reloc) / sizeof(GLuint)];

   struct i915_cache_reloc *reloc;
} packet;


static inline void packet_init( struct i915_cache_packet *packet,
				GLuint cache_id,
				GLuint nr_dwords, 
				GLuint nr_relocs )
{
   packet->nr_dwords = 0;
   packet->nr_relocs = 0;
   packet->reloc = (struct i915_cache_reloc *)&packet->dword[nr_dwords];
   packet->max_dwords = nr_dwords;
   packet->cache_id = cache_id;
}

static inline void packet_dword( struct i915_cache_packet *packet, GLuint d )
{
   assert(packet->nr_dwords < packet->max_dwords);
   packet->dword[packet->nr_dwords++].u = d;
}

static inline void packet_float( struct i915_cache_packet *packet, GLfloat f )
{
   assert(packet->nr_dwords < packet->max_dwords);
   packet->dword[packet->nr_dwords++].f = f;
}

static inline void packet_reloc( struct i915_cache_packet *packet,
				 struct _DriBufferObject *buf,
				 GLuint flags, 
				 GLuint mask, 
				 GLuint delta ) 
{
   packet->reloc[packet->nr_relocs].buffer = buf;
   packet->reloc[packet->nr_relocs].flags = flags;
   packet->reloc[packet->nr_relocs].mask = mask;
   packet->reloc[packet->nr_relocs].dword = packet->nr_dwords;
   packet->dword[packet->nr_dwords].u = delta;
   packet->nr_relocs++;
   packet->nr_dwords++;
} 



#endif
