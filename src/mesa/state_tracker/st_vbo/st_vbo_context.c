/*
 * Mesa 3-D graphics library
 * Version:  6.3
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
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
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "main/imports.h"
#include "main/mtypes.h"
#include "main/api_arrayelt.h"
#include "math/m_eval.h"
#include "st_vbo.h"
#include "st_vbo_context.h"

#if 0
/* Reach out and grab this to use as the default:
 */
extern void _tnl_draw_prims( GLcontext *ctx,
			     const struct gl_client_array *arrays[],
			     const struct st_mesa_prim *prims,
			     GLuint nr_prims,
			     const struct st_mesa_index_buffer *ib,
			     GLuint min_index,
			     GLuint max_index );
#endif



#define NR_LEGACY_ATTRIBS 16
#define NR_GENERIC_ATTRIBS 16
#define NR_MAT_ATTRIBS 12

static GLuint check_size( const GLfloat *attr )
{
   if (attr[3] != 1.0) return 4;
   if (attr[2] != 0.0) return 3;
   if (attr[1] != 0.0) return 2;
   return 1;
}

static void init_legacy_currval(GLcontext *ctx)
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct gl_client_array *arrays = st_vbo->legacy_currval;
   GLuint i;

   memset(arrays, 0, sizeof(*arrays) * NR_LEGACY_ATTRIBS);

   /* Set up a constant (StrideB == 0) array for each current
    * attribute:
    */
   for (i = 0; i < NR_LEGACY_ATTRIBS; i++) {
      struct gl_client_array *cl = &arrays[i];

      /* Size will have to be determined at runtime:
       */
      cl->Size = check_size(ctx->Current.Attrib[i]);
      cl->Stride = 0;
      cl->StrideB = 0;
      cl->Enabled = 1;
      cl->Type = GL_FLOAT;
      cl->Format = GL_RGBA;
      cl->Ptr = (const void *)ctx->Current.Attrib[i];
      cl->BufferObj = ctx->Shared->NullBufferObj;
   }
}


static void init_generic_currval(GLcontext *ctx)
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct gl_client_array *arrays = st_vbo->generic_currval;
   GLuint i;

   memset(arrays, 0, sizeof(*arrays) * NR_GENERIC_ATTRIBS);

   for (i = 0; i < NR_GENERIC_ATTRIBS; i++) {
      struct gl_client_array *cl = &arrays[i];

      /* This will have to be determined at runtime:
       */
      cl->Size = 1;
      cl->Type = GL_FLOAT;
      cl->Format = GL_RGBA;
      cl->Ptr = (const void *)ctx->Current.Attrib[VERT_ATTRIB_GENERIC0 + i];
      cl->Stride = 0;
      cl->StrideB = 0;
      cl->Enabled = 1;
      cl->BufferObj = ctx->Shared->NullBufferObj;
   }
}


static void init_mat_currval(GLcontext *ctx)
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct gl_client_array *arrays = st_vbo->mat_currval;
   GLuint i;

   ASSERT(NR_MAT_ATTRIBS == MAT_ATTRIB_MAX);

   memset(arrays, 0, sizeof(*arrays) * NR_MAT_ATTRIBS);

   /* Set up a constant (StrideB == 0) array for each current
    * attribute:
    */
   for (i = 0; i < NR_MAT_ATTRIBS; i++) {
      struct gl_client_array *cl = &arrays[i];

      /* Size is fixed for the material attributes, for others will
       * be determined at runtime:
       */
      switch (i - VERT_ATTRIB_GENERIC0) {
      case MAT_ATTRIB_FRONT_SHININESS:
      case MAT_ATTRIB_BACK_SHININESS:
	 cl->Size = 1;
	 break;
      case MAT_ATTRIB_FRONT_INDEXES:
      case MAT_ATTRIB_BACK_INDEXES:
	 cl->Size = 3;
	 break;
      default:
	 cl->Size = 4;
	 break;
      }

      cl->Ptr = (const void *)ctx->Light.Material.Attrib[i];
      cl->Type = GL_FLOAT;
      cl->Format = GL_RGBA;
      cl->Stride = 0;
      cl->StrideB = 0;
      cl->Enabled = 1;
      cl->BufferObj = ctx->Shared->NullBufferObj;
   }
}

#if 0

static void st_vbo_exec_current_init( struct st_vbo_exec_context *exec )
{
   GLcontext *ctx = exec->ctx;
   GLint i;

   /* setup the pointers for the typical 16 vertex attributes */
   for (i = 0; i < ST_VBO_ATTRIB_FIRST_MATERIAL; i++)
      exec->vtx.current[i] = ctx->Current.Attrib[i];

   /* setup pointers for the 12 material attributes */
   for (i = 0; i < MAT_ATTRIB_MAX; i++)
      exec->vtx.current[ST_VBO_ATTRIB_FIRST_MATERIAL + i] =
	 ctx->Light.Material.Attrib[i];
}
#endif

GLboolean _st_vbo_CreateContext( GLcontext *ctx )
{
   struct st_vbo_context *st_vbo = CALLOC_STRUCT(st_vbo_context);

   st_vbo->ctx = ctx;
   ctx->swtnl_im = (void *)st_vbo;

   /* Initialize the arrayelt helper
    */
   if (!ctx->aelt_context &&
       !_ae_create_context( ctx )) {
      return GL_FALSE;
   }

   /* TODO: remove these pointers.
    */
   st_vbo->legacy_currval = &st_vbo->currval[ST_VBO_ATTRIB_POS];
   st_vbo->generic_currval = &st_vbo->currval[ST_VBO_ATTRIB_GENERIC0];
   st_vbo->mat_currval = &st_vbo->currval[ST_VBO_ATTRIB_MAT_FRONT_AMBIENT];

   init_legacy_currval( ctx );
   init_generic_currval( ctx );
   init_mat_currval( ctx );

   /* Build mappings from VERT_ATTRIB -> ST_VBO_ATTRIB depending on type
    * of vertex program active.
    */
   {
      GLuint i;

      /* When no vertex program, pull in the material attributes in
       * the 16..32 generic range.
       */
      for (i = 0; i < 16; i++)
	 st_vbo->map_vp_none[i] = i;
      for (i = 0; i < 12; i++)
	 st_vbo->map_vp_none[16+i] = ST_VBO_ATTRIB_MAT_FRONT_AMBIENT + i;
      for (i = 0; i < 4; i++)
	 st_vbo->map_vp_none[28+i] = i;

      for (i = 0; i < VERT_ATTRIB_MAX; i++)
	 st_vbo->map_vp_arb[i] = i;
   }


   /* By default:
    */
#if 0 /* dead - see st_vbo_set_draw_func() */
   st_vbo->draw_prims = _tnl_draw_prims;
#endif

   /* Hook our functions into exec and compile dispatch tables.  These
    * will pretty much be permanently installed, which means that the
    * vtxfmt mechanism can be removed now.
    */
   st_vbo_exec_init( ctx );
#if FEATURE_dlist
   st_vbo_save_init( ctx );
#endif

   _math_init_eval();

   return GL_TRUE;
}

void _st_vbo_InvalidateState( GLcontext *ctx, GLuint new_state )
{
   _ae_invalidate_state(ctx, new_state);
   st_vbo_exec_invalidate_state(ctx, new_state);
}


void _st_vbo_DestroyContext( GLcontext *ctx )
{
   if (ctx->aelt_context) {
      _ae_destroy_context( ctx );
      ctx->aelt_context = NULL;
   }

   if (st_vbo_context(ctx)) {
      st_vbo_exec_destroy(ctx);
#if FEATURE_dlist
      st_vbo_save_destroy(ctx);
#endif
      FREE(st_vbo_context(ctx));
      ctx->swtnl_im = NULL;
   }
}


void st_vbo_set_draw_func(GLcontext *ctx, st_vbo_draw_func func)
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   st_vbo->draw_prims = func;
}

