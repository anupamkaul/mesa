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

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */

#ifndef INTEL_SWZ_H
#define INTEL_SWZ_H

struct intel_context;
struct intel_render *intel_create_swz_render( struct intel_context *intel );



/***********************************************************************
 * Private structures and functions
 */
#ifdef INTEL_SWZ_PRIVATE


#include "intel_utils.h"
#include "intel_cmdstream.h"
#include "intel_reg.h"
#include "intel_batchbuffer.h"

struct swz_zone {
   struct intel_hw_dirty state;
   GLubyte *ptr;
};

#define MAX_ZONES 1024

struct swz_render {
   struct intel_render render;
   struct intel_context *intel;

   /* Zone starting points, used for chaining at flush. 
    */
   GLubyte *initial_ptr[MAX_ZONES];
   struct swz_zone zone[MAX_ZONES];
   GLuint nr_zones;   
   GLuint zone_width;
   GLuint zone_height;

   GLuint state_reset_bits;
   
   GLboolean started_binning;

   GLubyte *vertices;
   GLuint vertex_stride;
   GLuint vbo_offset;
};

static INLINE struct swz_render *swz_render( struct intel_render *render )
{
   return (struct swz_render *)render;
}


/* Functions in intel_swz_prims.c: 
 */
void swz_clear_rect( struct intel_render *render,
		     GLuint unused_mask,
		     GLuint x1, GLuint y1, 
		     GLuint x2, GLuint y2 );

void swz_set_prim( struct intel_render *render,
		   GLenum prim );


/* Inlines:
 */
static INLINE void *get_vert( struct swz_render *swz,
			      GLuint idx )
{
   return swz->vertices + idx * swz->vertex_stride;
}


#define ZONE_WIDTH (1<<6)
#define ZONE_HEIGHT (1<<5)

#define ZONE_NONE   0
#define ZONE_POINTS 1
#define ZONE_LINES  2
#define ZONE_TRIS   3

#define I915_BB_CHAIN_SIZE   2	/* potentially */
#define I915_CLEAR_RECT_SIZE 6	/* the largest single primitive */

/* Room for the worst case: full state emit, clear rect, bb chain.
 * About 32 dwords == 128 bytes waste, quite a lot if you are using
 * 512 byte bins!
 */
#define I915_HW_STATE_SIZE			\
   (I915_MAX_IMMEDIATE + 1 + 			\
    I915_MAX_CACHE * 2 + 1 + 			\
    I915_CLEAR_RECT_SIZE + 			\
    I915_BB_CHAIN_SIZE)

#define ZONE_PRIM_SPACE  ((I915_BB_CHAIN_SIZE + 4) * sizeof(GLuint))
#define ZONE_CLEAR_SPACE ((I915_BB_CHAIN_SIZE + I915_CLEAR_RECT_SIZE) * sizeof(GLuint))
				 
static const GLuint hw_prim[4] = {
   0,
   PRIM3D_POINTLIST,
   PRIM3D_LINELIST,
   PRIM3D_TRILIST
};
   

static INLINE GLuint *zone_get_dwords( struct swz_zone *zone, GLuint nr )
{
   GLuint *out = (GLuint *) zone->ptr;
   zone->ptr += sizeof(out[0]) * nr;
   return out;
}

static INLINE GLushort *zone_get_words( struct swz_zone *zone, GLuint nr )
{
   GLushort *out = (GLushort *) zone->ptr;
   zone->ptr += sizeof(out[0]) * nr;
   return out;
}


static INLINE void zone_start_prim( struct swz_zone *zone, GLuint prim )
{
   GLuint *out = zone_get_dwords( zone, 1 );

   out[0] = ( _3DPRIMITIVE | 
	      hw_prim[prim] | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_ELTS );

   zone->state.prim = prim;
}


static INLINE void zone_emit_tri( struct swz_zone *zone, GLuint i0, GLuint i1, GLuint i2 )
{
   GLushort *out = zone_get_words( zone, 3 );

   out[0] = i0;
   out[1] = i1;
   out[2] = i2;
}

static INLINE void zone_emit_line( struct swz_zone *zone, GLuint i0, GLuint i1 )
{
   GLushort *out = zone_get_words( zone, 2 );

   out[0] = i0;
   out[1] = i1;
}

static INLINE void zone_emit_point( struct swz_zone *zone, GLuint i0 )
{
   GLushort *out = zone_get_words( zone, 1 );

   out[0] = i0;
}


static INLINE void zone_finish_prim( struct swz_zone *zone )
{
   if (zone->state.prim != ZONE_NONE) 
   {
      zone->state.prim = ZONE_NONE;
      while ( ((unsigned long)zone->ptr) & 2 ) 
      {
	 GLushort *out = zone_get_words( zone, 1 );
	 out[0] = 0xffff;
      }
   }
}


static INLINE void zone_end_batch( struct swz_zone *zone,
				   GLuint flushcmd )
{
   if ( ((unsigned long)zone->ptr) & 4 ) {
      GLuint *out = zone_get_dwords( zone, 3 );
      out[0] = flushcmd;
      out[1] = 0;
      out[2] = MI_BATCH_BUFFER_END;
   }
   else {
      GLuint *out = zone_get_dwords( zone, 2 );
      out[0] = flushcmd;
      out[1] = MI_BATCH_BUFFER_END;
   }
}

#define MI_BATCH_BUFFER_START 	(0x31<<23)
#define MI_BATCH_GTT    	(2<<6)
#define MI_BATCH_PHYSICAL    	(0<<6)

/* Assumes prevalidated batch buffers:
 */
static INLINE void zone_begin_batch( struct swz_render *swz,
				     struct swz_zone *zone,
				     GLubyte *newptr )
{
   GLuint *out = zone_get_dwords( zone, 2 );

   *out++ = ( MI_BATCH_BUFFER_START |
	      MI_BATCH_GTT );	

   /* Both out and newptr are assumed to be in the batchbuffer!!
    */
   intel_batchbuffer_set_reloc( swz->intel->batch,
				SEGMENT_IMMEDIATE,
				((GLubyte *)out) - swz->intel->batch->map,
				swz->intel->batch->buffer,
				DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_EXE,
				DRM_BO_MASK_MEM | DRM_BO_FLAG_EXE,			 
				newptr - swz->intel->batch->map );
}


/* Emit drawrect as an immediate command.  Can't really do this until
 * the lock is finally grabbed, so we end up emitting the drawrect for
 * one zone at the tail of the previous zone.
 * 
 * Regarding cliprects, the best we can do is select one of the
 * potentially many intersecting the zone.  Once we recognize that
 * there is a cliprect situation, we can avoid swz in future frames,
 * but at this point we have to live with glitches.
 */
static INLINE void zone_draw_rect( struct swz_zone *zone,
				   GLuint origin_x,
				   GLuint origin_y,
				   GLuint x1, 
				   GLuint y1,
				   GLuint x2,
				   GLuint y2 )
{
   GLuint *out = zone_get_dwords( zone, 5 );

   out[0] = GFX_OP_DRAWRECT_INFO;
   out[1] = 0;
   out[2] = (x1 & 0xffff) | (y1 << 16);
   out[3] = (x2 & 0xffff) | (y2 << 16);
   out[4] = (origin_x & 0xffff) | (origin_y << 16);
}

/* XXX: i915 only
 */
#define PRIM3D_CLEAR_RECT	(0xa<<18)

static INLINE void zone_clear_rect( struct swz_zone *zone,
				    GLuint x1,
				    GLuint y1,
				    GLuint x2,
				    GLuint y2 )
{

   union fi *out = (union fi *)zone_get_dwords( zone, 7 );
   out[0].u = _3DPRIMITIVE | PRIM3D_CLEAR_RECT | 5;
   out[1].f = x2;
   out[2].f = y2;
   out[3].f = x1;
   out[4].f = y2;
   out[5].f = x1;
   out[6].f = y1;
}
				    

static INLINE void zone_get_space( struct swz_render *swz,
				   struct swz_zone *zone )
{
   GLubyte *newptr = intel_cmdstream_alloc_block( swz->intel );
   zone_begin_batch( swz, zone, newptr );
   zone->ptr = newptr;
}




#endif
#endif
