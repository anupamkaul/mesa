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

#define ZONE_WIDTH (1<<6)
#define ZONE_HEIGHT (1<<5)



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

   struct swz_zone pre_post;
   
   GLboolean started_binning;

   GLubyte *vbo_vertices;
   GLubyte *vertices;
   GLuint vertex_stride;
   GLuint nr_vertices;
   GLuint vbo_offset;

   GLfloat xoff, yoff;

   struct intel_hw_dirty reset_state;

   GLuint initial_state_size;
   void *initial_driver_state;

   void *last_driver_state;
   GLuint driver_state_size;

   GLuint draws;
   GLuint clears;
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

void swz_zone_init( struct intel_render *render,
		    GLuint unused_mask,
		    GLuint x1, GLuint y1, 
		    GLuint x2, GLuint y2 );

void swz_set_prim( struct intel_render *render,
		   GLenum prim );

void swz_debug_zone( struct swz_render *swz,
		     struct swz_zone *zone );

/* Inlines:
 */
static INLINE void *get_vert( struct swz_render *swz,
			      GLuint idx )
{
   return swz->vertices + idx * swz->vertex_stride;
}


#define ZONE_NONE   0
#define ZONE_POINTS 1
#define ZONE_LINES  2
#define ZONE_TRIS   3

#define ZONE_WRAP_SPACE  (3 * sizeof(GLuint)) /* finish prim + begin_batch */
#define ZONE_PRIM_SPACE  ((3+3) * sizeof(GLuint)) /* start + 3 indices + ZONE_WRAP_SPACE */
#define ZONE_CLEAR_SPACE ((7+3) * sizeof(GLuint)) /* clear + BATCH_BEGIN */
				 
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

static INLINE void zone_emit_noop( struct swz_zone *zone )
{
   GLuint *out = zone_get_dwords( zone, 1 );

   out[0] = 0;
}


static INLINE void zone_finish_prim( struct swz_zone *zone )
{
   if (zone->state.prim != ZONE_NONE) 
   {
      zone->state.prim = ZONE_NONE;
      do
      {
	 GLushort *out = zone_get_words( zone, 1 );
	 out[0] = 0xffff;
      }
      while ( ((unsigned long)zone->ptr) & 2 ); 
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

static INLINE void zone_begin_batch( struct swz_render *swz,
				     struct swz_zone *zone,
				     const GLubyte *newptr )
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
				   GLuint x1, 
				   GLuint y1,
				   GLuint x2,
				   GLuint y2,
				   GLuint origin_x,
				   GLuint origin_y )
{
   GLuint *out = zone_get_dwords( zone, 5 );

   out[0] = GFX_OP_DRAWRECT_INFO;
   out[1] = 0;
   out[2] = (x1 & 0xffff) | (y1 << 16);
   out[3] = (x2 & 0xffff) | (y2 << 16);
   out[4] = (origin_x & 0xffff) | (origin_y << 16);
}

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
				    

static INLINE void zone_emit_zone_init( struct swz_zone *zone,
					GLuint x1,
					GLuint y1,
					GLuint x2,
					GLuint y2 )
{

   union fi *out = (union fi *)zone_get_dwords( zone, 7 );
   out[0].u = _3DPRIMITIVE | PRIM3D_ZONE_INIT | 5;
   out[1].f = x2;
   out[2].f = y2;
   out[3].f = x1;
   out[4].f = y2;
   out[5].f = x1;
   out[6].f = y1;
}


static INLINE void zone_loadreg_imm( struct swz_zone *zone,
				     GLuint reg,
				     GLuint value )
{
   GLuint *out = zone_get_dwords( zone, 3 );

   _mesa_printf("zone_loadreg_imm: %x / %08x\n", reg, value);

   out[0] = MI_LOAD_REGISTER_IMM;
   out[1] = reg;
   out[2] = value;
}

#define MI_FLUSH                   ((0<<29)|(4<<23))

static INLINE void zone_mi_flush( struct swz_zone *zone,
				  GLuint flags )
{
   GLuint *out = zone_get_dwords( zone, 1 );

   out[0] = MI_FLUSH | flags;
}
				    

static INLINE void zone_get_space( struct swz_render *swz,
				   struct swz_zone *zone )
{
   GLubyte *newptr = intel_cmdstream_alloc_block( swz->intel );

   if (newptr == NULL) {
      _mesa_printf("FLUSHING ****************\n");
      intel_frame_flush_and_restart( swz->intel->ft );
   }
   else {
      assert(newptr - zone->ptr > 0);
      zone_begin_batch( swz, zone, newptr );
      zone->ptr = newptr;
   }
}




#endif
#endif
