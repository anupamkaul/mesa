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
 

#include "intel_context.h"
#include "draw/intel_draw.h"

static GLuint translate_fill( GLenum mode )
{
   switch (mode) {
   case GL_POINT: return FILL_POINT;
   case GL_LINE: return FILL_LINE;
   case GL_FILL: return FILL_TRI;
   default: assert(0); return 0;
   }
}

static GLboolean get_offset_flag( GLuint fill_mode, 
				  const struct gl_polygon_attrib *Polygon )
{
   switch (fill_mode) {
   case FILL_POINT: return Polygon->OffsetPoint;
   case FILL_LINE: return Polygon->OffsetLine;
   case FILL_TRI: return Polygon->OffsetFill;
   default: assert(0); return 0;
   }
}


static void update_draw_state( struct intel_context *intel )
{
   struct intel_draw_state state;

   memset(&state, 0, sizeof(state));
   
   /* _NEW_POLYGON, _NEW_BUFFERS
    */
   {
      state.front_winding = WINDING_CW;
	
      if (intel->state.DrawBuffer && intel->state.DrawBuffer->Name != 0)
	 state.front_winding ^= WINDING_BOTH;

      if (intel->state.Polygon->FrontFace != GL_CCW)
	 state.front_winding ^= WINDING_BOTH;
   }

   /* _NEW_LIGHT
    */
   if (intel->state.Light->ShadeModel == GL_FLAT)
      state.flatshade = 1;

   /* _NEW_LIGHT 
    *
    * Not sure about the light->enabled requirement - does this still
    * apply??
    */
   if (intel->state.Light->Enabled && 
       intel->state.Light->Model.TwoSide)
      state.light_twoside = 1;


   /* _NEW_POLYGON
    */
   if (intel->state.Polygon->CullFlag) {
      if (intel->state.Polygon->CullFaceMode == GL_FRONT_AND_BACK) {
	 state.cull_mode = WINDING_BOTH;
      }
      else if (intel->state.Polygon->CullFaceMode == GL_FRONT) {
	 state.cull_mode = state.front_winding;
      }
      else {
	 state.cull_mode = state.front_winding ^ WINDING_BOTH;
      }
   }

   /* _NEW_POLYGON
    */
   {
      GLuint fill_front = translate_fill( intel->state.Polygon->FrontMode );
      GLuint fill_back = translate_fill( intel->state.Polygon->BackMode );
      
      if (state.front_winding == WINDING_CW) {
	 state.fill_cw = fill_front;
	 state.fill_ccw = fill_back;
      }
      else {
	 state.fill_cw = fill_back;
	 state.fill_ccw = fill_front;
      }

      /* Simplify when culling is active:
       */
      if (state.cull_mode & WINDING_CW) {
	 state.fill_cw = state.fill_ccw;
      }
      
      if (state.cull_mode & WINDING_CCW) {
	 state.fill_ccw = state.fill_cw;
      }
   }

   /* Hardware does offset for filled prims, but need to do it in
    * software for unfilled.
    *
    * _NEW_POLYGON 
    */
   if (state.fill_cw != FILL_TRI)
      state.offset_cw = get_offset_flag( state.fill_cw, 
					 intel->state.Polygon );
   
   if (state.fill_ccw != FILL_TRI)
      state.offset_ccw = get_offset_flag( state.fill_ccw, 
					  intel->state.Polygon );


   /* _NEW_BUFFERS, _NEW_POLYGON
    */
   if (state.fill_cw != FILL_TRI ||
       state.fill_ccw != FILL_TRI)
   {
      GLfloat mrd = intel->state.DrawBuffer->_MRD;
      state.offset_units = intel->state.Polygon->OffsetFactor * mrd;
      state.offset_scale = (intel->state.Polygon->OffsetUnits * mrd *
			    intel->polygon_offset_scale);
   }
      

   if (memcmp(&state, &intel->draw_state, sizeof(state)) != 0) {
      intel_draw_set_state( intel->draw, &state );
      memcpy( &intel->draw_state, &state, sizeof(state));
   }
}

const struct intel_tracked_state intel_update_draw_state = {
   .dirty = {
      .mesa = (_NEW_LIGHT | _NEW_POLYGON | _NEW_BUFFERS),
      .intel  = 0,
      .extra = 0
   },
   .update = update_draw_state
};


/* Second state atom for user clip planes:
 */
static void update_draw_userclip( struct intel_context *intel )
{
   GLuint nr = 0;
   GLfloat plane[6][4];
   GLuint i;

   for (i = 0; i < 6; i++) {
      if (intel->state.Transform->ClipPlanesEnabled & (1 << i)) {
	 memcpy(plane[nr], intel->state.Transform->_ClipUserPlane[i], 
		sizeof(plane[nr]));
	 nr++;
      }
   }
      
   intel_draw_set_userclip(intel->draw, plane, nr);
}


const struct intel_tracked_state intel_update_draw_userclip = {
   .dirty = {
      .mesa = (_NEW_TRANSFORM),
      .intel  = 0,
      .extra = 0
   },
   .update = update_draw_userclip
};





