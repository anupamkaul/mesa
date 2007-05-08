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
#include "macros.h"
#include "intel_state.h"

#define INTEL_SWZ_PRIVATE
#include "intel_swz.h"

/* XXX: this sucks, want a specific call from draw for when state
 * might have changed...
 */
static void invalidate_bins( struct swz_render *swz )
{
   struct intel_context *intel = swz->intel;

   if (intel->state.dirty.intel)
      intel_update_software_state( intel );

   {
      struct intel_hw_dirty flags = intel_track_states( intel,
							swz->last_driver_state,
							intel->state.current );

      GLuint i;
   
      if (flags.dirty == 0)
	 return;

      assert (swz->started_binning);

      /* Just mark the differences, state will be emitted per-zone later
       * on.
       */
      for (i = 0; i < swz->nr_zones; i++)
	 swz->zone[i].state.dirty |= flags.dirty;

      swz->reset_state.dirty |= flags.dirty;
   }
}



static void do_update_state( struct swz_render *swz, 
			     struct swz_zone *zone,
			     GLuint prim,
			     GLuint space )
{
   struct intel_context *intel = swz->intel;

   /* Finish old prim
    */
   zone_finish_prim( zone );
	
   /* Emit state
    */
   if (zone->state.dirty) 
   {
      GLuint size = intel->vtbl.get_state_emit_size( intel, zone->state );

      if (intel_cmdstream_space( zone->ptr ) <  size + space )
	 zone_get_space( swz, zone );

      intel->vtbl.emit_hardware_state( intel, 
				       (GLuint *)zone->ptr,
				       intel->state.current,
				       zone->state,
				       0 );

      zone->ptr += size;
      zone->state.dirty = 0;
      zone->state.swz_reset = 1;
   }
   else if (intel_cmdstream_space( zone->ptr ) <  space ) 
   {
      zone_get_space( swz, zone );
   }

   /* Start new prim
    */
   if (prim != ZONE_NONE)
      zone_start_prim( zone, prim );
}

static INLINE void zone_update_state( struct swz_render *swz, 
				      struct swz_zone *zone,
				      GLuint prim,
				      GLuint space )
{
   if (zone->state.dirty != 0 ||
       zone->state.prim != prim ||
       intel_cmdstream_space( zone->ptr ) < space )
   {
      do_update_state( swz, zone, prim, space );
   }
}

static void tri( struct swz_render *swz,
		 const GLfloat *v0,
		 const GLfloat *v1,
		 const GLfloat *v2,
		 GLuint i0, 
		 GLuint i1, 
		 GLuint i2 )
{
   GLfloat x0, x1, y0, y1;
   GLint zone_x0, zone_x1, zone_y0, zone_y1;
   GLint x,y;

#if 0
   GLfloat ex = v0[0] - v2[0];
   GLfloat ey = v0[1] - v2[1];
   GLfloat fx = v1[0] - v2[0];
   GLfloat fy = v1[1] - v2[1];
   
   GLfloat det = ex * fy - ey * fx;

   if (det >= 0) 
      return;
#endif

   /* Calculate bounds: NOTE: reading back from the vbo - must be
    * declared with appropriate flags.
    *
    * Almost all of the slowdown of swz relative to other rendering is
    * attributable to this calculation.  Need an optimized sse version
    * to get performance back.
    */
   x0 = v0[0];
   x1 = v0[0];
   y0 = v0[1];
   y1 = v0[1];

   if (x0 > v1[0]) x0 = v1[0];
   if (x0 > v2[0]) x0 = v2[0];
   if (y0 > v1[1]) y0 = v1[1];
   if (y0 > v2[1]) y0 = v2[1];

   if (x1 < v1[0]) x1 = v1[0];
   if (x1 < v2[0]) x1 = v2[0];
   if (y1 < v1[1]) y1 = v1[1];
   if (y1 < v2[1]) y1 = v2[1];

   zone_x0 = x0 + swz->xoff;
   zone_x1 = x1 + swz->xoff;
   zone_y0 = y0 + swz->yoff;
   zone_y1 = y1 + swz->yoff;

   zone_x0 /= ZONE_WIDTH;
   zone_x1 /= ZONE_WIDTH;
   zone_y0 /= ZONE_HEIGHT;
   zone_y1 /= ZONE_HEIGHT;

   if (zone_x0 < 0) zone_x0 = 0;
   if (zone_y0 < 0) zone_y0 = 0;
   if (zone_x1 >= swz->zone_width) zone_x1 = swz->zone_width-1;
   if (zone_y1 >= swz->zone_height) zone_y1 = swz->zone_height-1;
   

   if (0) _mesa_printf("tri (%f..%f)x(%f..%f) --> (%d..%d)x(%d..%d)\n", 
		       x0, x1, y0, y1,
		       zone_x0, zone_x1, zone_y0, zone_y1 );


   /* Emit to each zone:
    */
   for (y = zone_y0; y <= zone_y1; y++) {
      struct swz_zone *zone = &swz->zone[y * swz->zone_width + zone_x0];

      for (x = zone_x0; x <= zone_x1; x++, zone++) {
	 zone_update_state(swz, zone, ZONE_TRIS, ZONE_PRIM_SPACE );
	 zone_emit_tri(zone, i0, i1, i2);
      }
   }
}


static void line( struct swz_render *swz,
		  const GLfloat *v0,
		  const GLfloat *v1,
		  GLuint i0, 
		  GLuint i1 )
{
   GLfloat x0, x1, y0, y1;
   GLint zone_x0, zone_x1, zone_y0, zone_y1;
   GLint x,y;
   GLuint w = 1;

   /* Calculate bounds:
    */
   x0 = x1 = v0[0];
   y0 = y1 = v0[1];

   if (x0 > v1[0]) x0 = v1[0];
   if (x1 < v1[0]) x1 = v1[0];
   if (y0 > v1[1]) y0 = v1[1];
   if (y1 < v1[1]) y1 = v1[1];

   /* Convert to zones:
    */
   x0 = (x0 - w);
   x1 = (x1 + w);
   y0 = (y0 - w);
   y1 = (y1 + w);

   zone_x0 = x0;
   zone_x1 = x1;
   zone_y0 = y0;
   zone_y1 = y1;

   zone_x0 /= ZONE_WIDTH;
   zone_x1 /= ZONE_WIDTH;
   zone_y0 /= ZONE_HEIGHT;
   zone_y1 /= ZONE_HEIGHT;

   if (zone_x0 < 0) zone_x0 = 0;
   if (zone_y0 < 0) zone_y0 = 0;
   if (zone_x1 >= swz->zone_width) zone_x1 = swz->zone_width-1;
   if (zone_y1 >= swz->zone_height) zone_y1 = swz->zone_height-1;
   


   if (0) _mesa_printf("point (%f..%f)x(%f..%f) --> (%d..%d)x(%d..%d)\n", 
		       x0, x1, y0, y1,
		       zone_x0, zone_x1, zone_y0, zone_y1 );

   /* Emit to each zone:
    */
   for (y = zone_y0; y <= zone_y1; y++) {
      struct swz_zone *zone = &swz->zone[y * swz->zone_width + zone_x0];

      for (x = zone_x0; x <= zone_x1; x++, zone++) {
	 zone_update_state(swz, zone, ZONE_LINES, ZONE_PRIM_SPACE);
	 zone_emit_line(zone, i0, i1);
      }
   }
}

static void point( struct swz_render *swz,
		  const GLfloat *v0,
		   GLuint i0 )
{
   GLfloat x0, x1, y0, y1;
   GLint zone_x0, zone_x1, zone_y0, zone_y1;
   GLint x,y;
   GLuint w = 1;

   /* Calculate bounds:
    */
   x0 = x1 = v0[0];
   y0 = y1 = v0[1];

   /* Perform viewport transform (duplicate work!) and convert to zones:
    */
   x0 = (x0 - w);
   x1 = (x1 + w);
   y0 = (y0 - w);
   y1 = (y1 + w);

   zone_x0 = x0;
   zone_x1 = x1;
   zone_y0 = y0;
   zone_y1 = y1;

   zone_x0 /= ZONE_WIDTH;
   zone_x1 /= ZONE_WIDTH;
   zone_y0 /= ZONE_HEIGHT;
   zone_y1 /= ZONE_HEIGHT;

   if (zone_x0 < 0) zone_x0 = 0;
   if (zone_y0 < 0) zone_y0 = 0;
   if (zone_x1 >= swz->zone_width) zone_x1 = swz->zone_width-1;
   if (zone_y1 >= swz->zone_height) zone_y1 = swz->zone_height-1;

   if (0) _mesa_printf("point (%f..%f)x(%f..%f) --> (%d..%d)x(%d..%d)\n", 
		       x0, x1, y0, y1,
		       zone_x0, zone_x1, zone_y0, zone_y1 );

   /* Emit to each zone:
    */
   for (y = zone_y0; y <= zone_y1; y++) {
      struct swz_zone *zone = &swz->zone[y * swz->zone_width + zone_x0];

      for (x = zone_x0; x <= zone_x1; x++, zone++) {
	 zone_update_state(swz, zone, ZONE_POINTS, ZONE_PRIM_SPACE);
	 zone_emit_point(zone, i0);
      }
   }
}


/* Presumably this should also be binned:
 */
void swz_clear_rect( struct intel_render *render,
		     GLuint unused_mask,
		     GLuint x0, GLuint y0, 
		     GLuint x1, GLuint y1 )
{
   struct swz_render *swz = swz_render( render );
   GLuint zone_x0, zone_x1, zone_y0, zone_y1;
   GLuint x, y;

   _mesa_printf("%s %d..%d %d..%d\n", __FUNCTION__, x0, x1, y0, y1);
   
   assert( swz->started_binning );

   invalidate_bins( swz );

   zone_x0 = x0;
   zone_x1 = x1-1;
   zone_y0 = y0;
   zone_y1 = y1-1;

   zone_x0 /= ZONE_WIDTH;
   zone_x1 /= ZONE_WIDTH;
   zone_y0 /= ZONE_HEIGHT;
   zone_y1 /= ZONE_HEIGHT;


   for (y = zone_y0; y <= zone_y1; y++) {
      struct swz_zone *zone = &swz->zone[y * swz->zone_width + zone_x0];
      for (x = zone_x0; x <= zone_x1; x++, zone++) {
	 zone_update_state( swz, zone, ZONE_NONE, ZONE_CLEAR_SPACE );
	 zone_clear_rect( zone, x0, y0, x1, y1 ); /* or b */
      }
   }
}



/* Presumably this should also be binned:
 */
void swz_zone_init( struct intel_render *render,
		    GLuint unused_mask,
		    GLuint x0, GLuint y0, 
		    GLuint x1, GLuint y1 )
{
   struct swz_render *swz = swz_render( render );
   GLuint zone_x0, zone_x1, zone_y0, zone_y1;
   GLuint x, y;

   _mesa_printf("%s %d..%d %d..%d\n", __FUNCTION__, x0, x1, y0, y1);

   assert( swz->started_binning );

   invalidate_bins( swz );

   zone_x0 = x0;
   zone_x1 = x1-1;
   zone_y0 = y0;
   zone_y1 = y1-1;

   zone_x0 /= ZONE_WIDTH;
   zone_x1 /= ZONE_WIDTH;
   zone_y0 /= ZONE_HEIGHT;
   zone_y1 /= ZONE_HEIGHT;


   for (y = zone_y0; y <= zone_y1; y++) {
      struct swz_zone *zone = &swz->zone[y * swz->zone_width + zone_x0];
      for (x = zone_x0; x <= zone_x1; x++, zone++) {
	 zone_update_state( swz, zone, ZONE_NONE, ZONE_CLEAR_SPACE );
	 zone_emit_zone_init( zone, x0, y0, x1, y1 );
      }
   }
}



static void draw_indexed_points( struct intel_render *render,
				 const GLuint *indices,
				 GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i < nr; i++) {
      point( swz, 
	     get_vert(swz, indices[i]),
	     swz->vbo_offset + indices[i] );
   }
}

static void draw_indexed_lines( struct intel_render *render,
				const GLuint *indices,
				GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+1 < nr; i += 2) {
      line( swz, 
	    get_vert(swz, indices[i]),
	    get_vert(swz, indices[i+1]),
	    swz->vbo_offset + indices[i], 
	    swz->vbo_offset + indices[i+1] );
   }
}

static void draw_indexed_linestrip( struct intel_render *render,
				    const GLuint *indices,
				    GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+1 < nr; i++) {
      line( swz, 
	    get_vert(swz, indices[i]),
	    get_vert(swz, indices[i+1]),
	    swz->vbo_offset + indices[i], 
	    swz->vbo_offset + indices[i+1] );
   }
}

static void draw_indexed_tris( struct intel_render *render,
			       const GLuint *indices,
			       GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i += 3) {
      tri( swz, 
	   get_vert(swz, indices[i]),
	   get_vert(swz, indices[i+1]),
	   get_vert(swz, indices[i+2]),
	   swz->vbo_offset + indices[i], 
	   swz->vbo_offset + indices[i+1],
	   swz->vbo_offset + indices[i+2] );
   }
}

static void draw_indexed_tristrip( struct intel_render *render,
				   const GLuint *indices,
				   GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, indices[i]),
	   get_vert(swz, indices[i+1]),
	   get_vert(swz, indices[i+2]),
	   swz->vbo_offset + indices[i], 
	   swz->vbo_offset + indices[i+1],
	   swz->vbo_offset + indices[i+2] );
   }
}

static void draw_indexed_trifan( struct intel_render *render,
				 const GLuint *indices,
				 GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, indices[0]),
	   get_vert(swz, indices[i+1]),
	   get_vert(swz, indices[i+2]),
	   swz->vbo_offset + indices[0], 
	   swz->vbo_offset + indices[i+1],
	   swz->vbo_offset + indices[i+2] );
   }
}

/* XXX: fix pv
 */
static void draw_indexed_poly( struct intel_render *render,
				 const GLuint *indices,
				 GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, indices[0]),
	   get_vert(swz, indices[i+1]),
	   get_vert(swz, indices[i+2]),
	   swz->vbo_offset + indices[0], 
	   swz->vbo_offset + indices[i+1],
	   swz->vbo_offset + indices[i+2] );
   }
}


static void draw_points( struct intel_render *render,
			 GLuint start,
			 GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i < nr; i++) {
      point( swz, 
	     get_vert(swz, i),
	     swz->vbo_offset + i );
   }
}

static void draw_lines( struct intel_render *render,
			GLuint start,
			GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+1 < nr; i += 2) {
      line( swz, 
	    get_vert(swz, i),
	    get_vert(swz, i+1),
	    swz->vbo_offset + i,
	    swz->vbo_offset + i + 1 );
   }
}

static void draw_linestrip( struct intel_render *render,
			    GLuint start,
			    GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+1 < nr; i++) {
      line( swz, 
	    get_vert(swz, i),
	    get_vert(swz, i+1),
	    swz->vbo_offset + i,
	    swz->vbo_offset + i + 1 );
   }
}

static void draw_tris( struct intel_render *render,
		       GLuint start,
		       GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i += 3) {
      tri( swz, 
	   get_vert(swz, i),
	   get_vert(swz, i+1),
	   get_vert(swz, i+2),
	   swz->vbo_offset + i,
	   swz->vbo_offset + i + 1,
	   swz->vbo_offset + i + 2 );
   }
}

static void draw_tristrip( struct intel_render *render,
			   GLuint start,
			   GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, i),
	   get_vert(swz, i+1),
	   get_vert(swz, i+2),
	   swz->vbo_offset + i,
	   swz->vbo_offset + i + 1,
	   swz->vbo_offset + i + 2 );
   }
}

static void draw_trifan( struct intel_render *render,
			 GLuint start,
			 GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, 0),
	   get_vert(swz, i+1),
	   get_vert(swz, i+2),
	   swz->vbo_offset + 0,
	   swz->vbo_offset + i + 1,
	   swz->vbo_offset + i + 2 );
   }
}

/* XXX: fix pv
 */
static void draw_poly( struct intel_render *render,
		       GLuint start,
		       GLuint nr )
{
   struct swz_render *swz = swz_render( render );
   GLuint i;

   invalidate_bins( swz );
   for (i = 0; i+2 < nr; i++) {
      tri( swz, 
	   get_vert(swz, 0),
	   get_vert(swz, i+1),
	   get_vert(swz, i+2),
	   swz->vbo_offset + 0,
	   swz->vbo_offset + i + 1,
	   swz->vbo_offset + i + 2 );
   }
}


void swz_set_prim( struct intel_render *render,
		   GLenum prim )
{
   struct swz_render *swz = swz_render( render );

   switch (prim) {
   case GL_POINTS:
      swz->render.draw_prim = draw_points;
      swz->render.draw_indexed_prim = draw_indexed_points;
      break;

   case GL_LINES:
      swz->render.draw_prim = draw_lines;
      swz->render.draw_indexed_prim = draw_indexed_lines;
      break;

   case GL_LINE_STRIP:
      swz->render.draw_prim = draw_linestrip;
      swz->render.draw_indexed_prim = draw_indexed_linestrip;
      break;

   case GL_TRIANGLES:
      swz->render.draw_prim = draw_tris;
      swz->render.draw_indexed_prim = draw_indexed_tris;
      break;

   case GL_TRIANGLE_STRIP:
      swz->render.draw_prim = draw_tristrip;
      swz->render.draw_indexed_prim = draw_indexed_tristrip;
      break;

   case GL_TRIANGLE_FAN:
      swz->render.draw_prim = draw_trifan;
      swz->render.draw_indexed_prim = draw_indexed_trifan;
      break;

   case GL_POLYGON:
      swz->render.draw_prim = draw_poly;
      swz->render.draw_indexed_prim = draw_indexed_poly;
      break;

   default:
      /* Handled elsewhere. 
       */
      assert(0);
      break;
   }
}
