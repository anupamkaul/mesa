/* $Id: t_vb_light.c,v 1.18.2.3 2003/01/16 00:38:44 keithw Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



#include "glheader.h"
#include "colormac.h"
#include "light.h"
#include "macros.h"
#include "mem.h"
#include "mmath.h"
#include "simple_list.h"
#include "mtypes.h"

#include "math/m_translate.h"

#include "t_context.h"
#include "t_pipeline.h"

#define LIGHT_TWOSIDE       0x1
#define LIGHT_MATERIAL      0x2
#define MAX_LIGHT_FUNC      0x4

typedef void (*light_func)( GLcontext *ctx,
			    struct vertex_buffer *VB,
			    struct tnl_pipeline_stage *stage,
			    GLvector4f *input );

struct light_stage_data {
   struct gl_client_array LitColor[2];
   struct gl_client_array LitSecondary[2];
   GLvector1ui LitIndex[2];
   light_func *light_func_tab;
};


#define LIGHT_STAGE_DATA(stage) ((struct light_stage_data *)(stage->privatePtr))



/* Tables for all the shading functions.
 */
static light_func _tnl_light_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_fast_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_fast_single_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_spec_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_ci_tab[MAX_LIGHT_FUNC];

#define TAG(x)           x
#define IDX              (0)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_twoside
#define IDX              (LIGHT_TWOSIDE)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_mat
#define IDX              (LIGHT_MATERIAL)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_twoside_mat
#define IDX              (LIGHT_TWOSIDE|LIGHT_MATERIAL)
#include "t_vb_lighttmp.h"


static void init_lighting( void )
{
   static int done;

   if (!done) {
      init_light_tab();
      init_light_tab_twoside();
      init_light_tab_mat();
      init_light_tab_twoside_mat();
      done = 1;
   }
}


static GLboolean run_lighting( GLcontext *ctx, struct tnl_pipeline_stage *stage )
{
   struct light_stage_data *store = LIGHT_STAGE_DATA(stage);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLvector4f *input = ctx->_NeedEyeCoords ? VB->EyePtr : VB->ObjPtr;
   GLuint ind = 0;

   if ((ctx->Light.ColorMaterialEnabled && (VB->Active & VERT_BITS_COLOR)) || 
       VB->Active & VERT_BITS_MATERIAL)
      ind |= LIGHT_MATERIAL;

   if (ctx->Light.Model.TwoSide)
      ind |= LIGHT_TWOSIDE;

   /* The individual functions know about replaying side-effects
    * vs. full re-execution. 
    */
   store->light_func_tab[ind]( ctx, VB, stage, input );

   return GL_TRUE;
}


/* Called in place of do_lighting when the light table may have changed.
 */
static GLboolean run_validate_lighting( GLcontext *ctx,
					struct tnl_pipeline_stage *stage )
{
   GLuint ind = 0;
   light_func *tab;

   if (ctx->Visual.rgbMode) {
      if (ctx->Light._NeedVertices) {
	 if (ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR)
	    tab = _tnl_light_spec_tab;
	 else
	    tab = _tnl_light_tab;
      }
      else {
	 if (ctx->Light.EnabledList.next == ctx->Light.EnabledList.prev)
	    tab = _tnl_light_fast_single_tab;
	 else
	    tab = _tnl_light_fast_tab;
      }
   }
   else
      tab = _tnl_light_ci_tab;

   LIGHT_STAGE_DATA(stage)->light_func_tab = tab;

   /* This and the above should only be done on _NEW_LIGHT:
    */
   TNL_CONTEXT(ctx)->Driver.NotifyMaterialChange( ctx );

   /* Now run the stage...
    */
   stage->run = run_lighting;
   return stage->run( ctx, stage );
}

static void alloc_4f( struct gl_client_array *a, GLuint sz )
{
   a->Ptr = ALIGN_MALLOC( sz * sizeof(GLfloat) * 4, 32 );
   a->Size = 4;
   a->Type = GL_FLOAT;
   a->Stride = 0;
   a->StrideB = sizeof(GLfloat) * 4;
   a->Enabled = 0;
   a->Flags = 0;
}


/* Called the first time stage->run is called.  In effect, don't
 * allocate data until the first time the stage is run.
 */
static GLboolean run_init_lighting( GLcontext *ctx,
				    struct tnl_pipeline_stage *stage )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct light_stage_data *store;
   GLuint size = tnl->vb.Size;

   stage->privatePtr = MALLOC(sizeof(*store));
   store = LIGHT_STAGE_DATA(stage);
   if (!store)
      return GL_FALSE;

   /* Do onetime init.
    */
   init_lighting();

   alloc_4f( &store->LitColor[0], size );
   alloc_4f( &store->LitColor[1], size );
   alloc_4f( &store->LitSecondary[0], size );
   alloc_4f( &store->LitSecondary[1], size );

   _mesa_vector1ui_alloc( &store->LitIndex[0], 0, size, 32 );
   _mesa_vector1ui_alloc( &store->LitIndex[1], 0, size, 32 );

   /* Now validate the stage derived data...
    */
   stage->run = run_validate_lighting;
   return stage->run( ctx, stage );
}



/*
 * Check if lighting is enabled.  If so, configure the pipeline stage's
 * type, inputs, and outputs.
 */
static void check_lighting( GLcontext *ctx, struct tnl_pipeline_stage *stage )
{
   stage->active = ctx->Light.Enabled && !ctx->VertexProgram.Enabled;
   if (stage->active) {
      if (stage->privatePtr)
	 stage->run = run_validate_lighting;
      
      if (ctx->Light._NeedVertices)
	 SET_BIT(stage->inputs, VERT_ATTRIB_POS);
      else
	 CLEAR_BIT(stage->inputs, VERT_ATTRIB_POS);

      if (ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR) {
	 SET_BIT(stage->outputs, VERT_BIT_COLOR1);
	 SET_BIT(stage->outputs, VERT_BIT_BACK_COLOR1);
      }
      else {
	 CLEAR_BIT(stage->outputs, VERT_BIT_COLOR1);
	 CLEAR_BIT(stage->outputs, VERT_BIT_BACK_COLOR1);
      }
   }
}

static void dtr( struct tnl_pipeline_stage *stage )
{
   struct light_stage_data *store = LIGHT_STAGE_DATA(stage);

   if (store) {
      ALIGN_FREE( store->LitColor[0].Ptr );
      ALIGN_FREE( store->LitColor[1].Ptr );
      ALIGN_FREE( store->LitSecondary[0].Ptr );
      ALIGN_FREE( store->LitSecondary[1].Ptr );

      _mesa_vector1ui_free( &store->LitIndex[0] );
      _mesa_vector1ui_free( &store->LitIndex[1] );
      FREE( store );
      stage->privatePtr = 0;
   }
}

struct tnl_pipeline_stage *_tnl_lighting_stage( GLcontext *ctx )
{
   stage = CALLOC_STRUCT( tnl_pipeline_stage );

   stage->name = "lighting";
   stage->recheck = _NEW_LIGHT;
   stage->recalc = _NEW_LIGHT|_NEW_MODELVIEW;
   stage->active = GL_FALSE;
   stage->destroy = dtr;
   stage->check = check_lighting;
   stage->run = run_init_lighting;

   SET_BIT(stage->inputs, VERT_ATTRIB_NORMAL);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_AMBIENT);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_DIFFUSE);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_SPECULAR);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_EMISSION);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_SHININESS);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_FRONT_INDEXES);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_AMBIENT);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_DIFFUSE);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_SPECULAR);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_EMISSION);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_SHININESS);
   SET_BIT(stage->inputs, VERT_ATTRIB_MAT_BACK_INDEXES);

   if (ctx->Visual.rgbMode) {
      SET_BIT(stage->outputs, VERT_ATTRIB_COLOR0);
      SET_BIT(stage->outputs, VERT_ATTRIB_BACK_COLOR0);
   }
   else {
      SET_BIT(stage->outputs, VERT_BIT_INDEX0);
      SET_BIT(stage->outputs, VERT_BIT_BACK_INDEX0);
   }
   
   return stage;
}
