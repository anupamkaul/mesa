/** 
 * \file t_context.c
 *
 * \author Keith Whitwell <keith@tungstengraphics.com>
 */

/*
 * Mesa 3-D graphics library
 * Version:  5.1
 *
 * Copyright (C) 1999-2003  Brian Paul   All Rights Reserved.
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
 *
 */

/* $Id: t_context.c,v 1.31.2.1 2003/04/05 16:42:15 jrfonseca Exp $ */
 
/**
 * \mainpage
 * 
 * \section TnlIntroduction Introduction
 * 
 * A generic, configurable software implementation of GL transformation &
 * lighting.
 * 
 * This module provides an implementation of the routines required by the
 * 'vtxfmt' mechanism of core mesa for tnl functionality in all
 * combinations of compile and execute modes.
 * 
 * Most current drivers use the tnl module exclusively to provide this
 * functionality, though there is an experimental alternate
 * implementation provided by the tnl_dd/t_dd_imm_* files which can
 * handle a small subset of GL states in execute mode only.
 * 
 *
 * \section TnlState State
 * 
 * To create and destroy the module:
 * 
 * \code
 * 	GLboolean _tnl_CreateContext( GLcontext *ctx );
 * 	void _tnl_DestroyContext( GLcontext *ctx );
 * \endcode
 * 
 * The module is not active by default, and must be installed by calling
 * _tnl_Wakeup().  This function installs internal tnl functions into all
 * the vtxfmt dispatch hooks, thus taking over the task of transformation
 * and lighting entirely:
 * 
 * \code
 * 	void _tnl_wakeup_exec( GLcontext *ctx );
 * 	void _tnl_wakeup_save_exec( GLcontext *ctx );
 * \endcode
 *    
 * This module tracks state changes internally and maintains derived
 * values based on the current state.  For this to work, the driver
 * ensure the following funciton is called whenever the state changes and
 * the swsetup module is 'awake':
 * 
 * \code
 * 	void _tnl_InvalidateState( GLcontext *ctx, GLuint new_state );
 * \endcode
 * 
 * There is no explicit call to put the tnl module to sleep.  Simply
 * install other function pointers into all the vtxfmt dispatch slots,
 * and (optionally) cease calling _tnl_InvalidateState().
 * 
 * \section TnlCustomization Customization
 * 
 * The module provides customizability through several mechanisms.  The
 * most important is by allowing drivers to specify the pipeline through
 * which vertex data is passed, including its eventual transfer to
 * rasterization hardware (or software).
 * 
 * The default pipeline is specified in t_pipeline.c, and is usually a
 * starting point for driver pipelines.  Some drivers will remove a stage
 * where hardware provides support for the implemented operation (for
 * instance fog where per-pixel hardware fog is available, as in the dri
 * tdfx driver), or add stages to shortcircuit latter operations (for
 * example taking advantage of hardware support for strips and other
 * higher-level primitives (for example the radeon driver).
 * 
 * In addition, the following functions provide further tweaks:
 * 
 * \code
 * extern void
 * _tnl_need_projected_coords( GLcontext *ctx, GLboolean flag );
 * \endcode
 * 
 * 	- Direct the default vertex transformation stage to
 *           produce/not produce projected clip coordinates.
 * 	  
 * \code
 * extern void
 * _tnl_need_dlist_loopback( GLcontext *ctx, GLboolean flag );
 * \endcode
 *       
 *         - Direct the display list component of the tnl module to
 *           replay display lists as 'glVertex' type calls, rather than
 *           passing the display list data directly into the tnl pipeline
 *           mechanism.  
 * 
 * 	  This allows display lists to be replayed by the tnl module
 * 	  even when the module is not strictly active.
 * 
 * 
 * \code
 * extern void
 * _tnl_need_dlist_norm_lengths( GLcontext *ctx, GLboolean flag );
 * \endcode
 * 
 * 	- Direct the display list component to enable/disable caching
 *           1/length values for display list normals.  Doing so is
 *           ususally helpful when lighting is performed in software, but
 *           wasteful otherwise.
 * 
 * 
 * \section TnlDriverInterface Driver Interface
 * 
 * The module itself offers a minimal driver interface:
 * 
 * \code
 * 	 void (*RunPipeline)( GLcontext *ctx );
 * \endcode
 * 
 * Normally this is set to _tnl_RunPipeline(), however the driver can use
 * this hook to wrap checks or other code around this call. 
 * 
 * In addition, the driver interface for the default render pipeline
 * stage is housed in the tnl context struct (this could be cleaner).  
 * 
 * 
 * \section TnlRenderDriverInterface Render Driver Interface
 * 
 * See t_context.h for the definition and explanation of this.
 */

#include "glheader.h"
#include "imports.h"
#include "macros.h"
#include "mtypes.h"
#include "dlist.h"
#include "light.h"
#include "vtxfmt.h"

#include "t_context.h"
#include "t_array_api.h"
#include "t_eval_api.h"
#include "t_imm_alloc.h"
#include "t_imm_api.h"
#include "t_imm_exec.h"
#include "t_imm_dlist.h"
#include "t_pipeline.h"
#include "tnl.h"

#ifndef THREADS
struct immediate *_tnl_CurrentInput = NULL;
#endif


void
_tnl_MakeCurrent( GLcontext *ctx,
		  GLframebuffer *drawBuffer,
		  GLframebuffer *readBuffer )
{
#ifndef THREADS
   SET_IMMEDIATE( ctx, TNL_CURRENT_IM(ctx) );
#endif
}


static void
install_driver_callbacks( GLcontext *ctx )
{
   ctx->Driver.NewList = _tnl_NewList;
   ctx->Driver.EndList = _tnl_EndList;
   ctx->Driver.FlushVertices = _tnl_flush_vertices;
   ctx->Driver.MakeCurrent = _tnl_MakeCurrent;
   ctx->Driver.BeginCallList = _tnl_BeginCallList;
   ctx->Driver.EndCallList = _tnl_EndCallList;
}



GLboolean
_tnl_CreateContext( GLcontext *ctx )
{
   TNLcontext *tnl;

   /* Create the TNLcontext structure
    */
   ctx->swtnl_context = tnl = (TNLcontext *) CALLOC( sizeof(TNLcontext) );

   if (!tnl) {
      return GL_FALSE;
   }

   /* Initialize the VB.
    */
   tnl->vb.Size = MAX2( IMM_SIZE,
			ctx->Const.MaxArrayLockSize + MAX_CLIPPED_VERTICES);


   /* Initialize tnl state and tnl->vtxfmt.
    */
   _tnl_dlist_init( ctx );
   _tnl_array_init( ctx );
   _tnl_imm_init( ctx );
   _tnl_eval_init( ctx );
   _tnl_install_pipeline( ctx, _tnl_default_pipeline );


   tnl->NeedNdcCoords = GL_TRUE;
   tnl->LoopbackDListCassettes = GL_FALSE;
   tnl->CalcDListNormalLengths = GL_TRUE;

   /* Hook our functions into exec and compile dispatch tables.
    */
   _mesa_install_exec_vtxfmt( ctx, &tnl->vtxfmt );

   tnl->save_vtxfmt = tnl->vtxfmt;
   tnl->save_vtxfmt.CallList = _mesa_save_CallList;	
   tnl->save_vtxfmt.EvalMesh1 = _mesa_save_EvalMesh1;	
   tnl->save_vtxfmt.EvalMesh2 = _mesa_save_EvalMesh2;
   tnl->save_vtxfmt.Begin = _tnl_save_Begin;

   _mesa_install_save_vtxfmt( ctx, &tnl->save_vtxfmt );


   /* Set a few default values in the driver struct.
    */
   install_driver_callbacks(ctx);
   ctx->Driver.NeedFlush = FLUSH_UPDATE_CURRENT;
   ctx->Driver.CurrentExecPrimitive = PRIM_OUTSIDE_BEGIN_END;
   ctx->Driver.CurrentSavePrimitive = PRIM_UNKNOWN;

   tnl->Driver.Render.PrimTabElts = _tnl_render_tab_elts;
   tnl->Driver.Render.PrimTabVerts = _tnl_render_tab_verts;
   tnl->Driver.NotifyMaterialChange = _mesa_validate_all_lighting_tables;


   
   return GL_TRUE;
}


void
_tnl_DestroyContext( GLcontext *ctx )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   _tnl_array_destroy( ctx );
   _tnl_imm_destroy( ctx );
   _tnl_destroy_pipeline( ctx );
   _tnl_free_immediate( ctx, tnl->freed_immediate );

   FREE(tnl);
   ctx->swtnl_context = 0;
}


void
_tnl_InvalidateState( GLcontext *ctx, GLuint new_state )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   if (new_state & _NEW_ARRAY) {
      struct immediate *IM = TNL_CURRENT_IM(ctx);
      IM->ArrayEltFlags = ~ctx->Array._Enabled;
      IM->ArrayEltFlush = (ctx->Array.LockCount 
			   ? FLUSH_ELT_LAZY : FLUSH_ELT_EAGER);
      IM->ArrayEltIncr = ctx->Array.Vertex.Enabled ? 1 : 0;
      tnl->pipeline.run_input_changes |= ctx->Array.NewState; /* overkill */
   }

   tnl->pipeline.run_state_changes |= new_state;
   tnl->pipeline.build_state_changes |= (new_state &
					 tnl->pipeline.build_state_trigger);

   tnl->eval.EvalNewState |= new_state;
}


void
_tnl_wakeup_exec( GLcontext *ctx )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   install_driver_callbacks(ctx);
   ctx->Driver.NeedFlush |= FLUSH_UPDATE_CURRENT;

   /* Hook our functions into exec and compile dispatch tables.
    */
   _mesa_install_exec_vtxfmt( ctx, &tnl->vtxfmt );

   /* Call all appropriate driver callbacks to revive state.
    */
   _tnl_MakeCurrent( ctx, ctx->DrawBuffer, ctx->ReadBuffer );

   /* Assume we haven't been getting state updates either:
    */
   _tnl_InvalidateState( ctx, ~0 );
   tnl->pipeline.run_input_changes = ~0;

   if (ctx->Light.ColorMaterialEnabled) {
      _mesa_update_color_material( ctx, ctx->Current.Attrib[VERT_ATTRIB_COLOR0] );
   }

}


void
_tnl_wakeup_save_exec( GLcontext *ctx )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   _tnl_wakeup_exec( ctx );
   _mesa_install_save_vtxfmt( ctx, &tnl->save_vtxfmt );
}


/**
 * Drivers call this function to tell the TCL module whether or not
 * it wants Normalized Device Coords (NDC) computed.  I.e. whether
 * we should "Divide-by-W".  Software renders will want that.
 */
void
_tnl_need_projected_coords( GLcontext *ctx, GLboolean mode )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   if (tnl->NeedNdcCoords != mode) {
      tnl->NeedNdcCoords = mode;
      _tnl_InvalidateState( ctx, _NEW_PROJECTION );
   }
}

void
_tnl_need_dlist_loopback( GLcontext *ctx, GLboolean mode )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   tnl->LoopbackDListCassettes = mode;
}

void
_tnl_need_dlist_norm_lengths( GLcontext *ctx, GLboolean mode )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   tnl->CalcDListNormalLengths = mode;
}

void
_tnl_isolate_materials( GLcontext *ctx, GLboolean mode )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   tnl->IsolateMaterials = mode;
}
