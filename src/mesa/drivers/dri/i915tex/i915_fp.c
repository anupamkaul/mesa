/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "glheader.h"
#include "macros.h"
#include "enums.h"

#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "intel_batchbuffer.h"

#include "i915_reg.h"
#include "i915_context.h"
#include "i915_program.h"

#include "program_instruction.h"
#include "program.h"
#include "programopt.h"



static void
track_params(struct i915_fragment_program *p)
{
   GLint i;

   if (p->nr_params)
      _mesa_load_state_parameters(p->ctx, p->FragProg.Base.Parameters);

   for (i = 0; i < p->nr_params; i++) {
      GLint reg = p->param[i].reg;
      COPY_4V(p->constant[reg], p->param[i].values);
   }
}


static void
i915BindProgram(GLcontext * ctx, GLenum target, struct gl_program *prog)
{
   if (target == GL_FRAGMENT_PROGRAM_ARB) {
      struct i915_context *i915 = I915_CONTEXT(ctx);
      struct i915_fragment_program *p = (struct i915_fragment_program *) prog;

      if (i915->current_program == p)
         return;

      i915->current_program = p;

      assert(p->on_hardware == 0);
      assert(p->params_uptodate == 0);

      /* Hack: make sure fog is correctly enabled according to this
       * fragment program's fog options.
       */
      ctx->Driver.Enable(ctx, GL_FRAGMENT_PROGRAM_ARB,
                         ctx->FragmentProgram.Enabled);
   }
}

static struct gl_program *
i915NewProgram(GLcontext * ctx, GLenum target, GLuint id)
{
   switch (target) {
   case GL_VERTEX_PROGRAM_ARB:
      return _mesa_init_vertex_program(ctx, CALLOC_STRUCT(gl_vertex_program),
                                       target, id);

   case GL_FRAGMENT_PROGRAM_ARB:{
         struct i915_fragment_program *prog =
            CALLOC_STRUCT(i915_fragment_program);
         if (prog) {
            i915_init_program(I915_CONTEXT(ctx), prog);

            return _mesa_init_fragment_program(ctx, &prog->FragProg,
                                               target, id);
         }
         else
            return NULL;
      }

   default:
      /* Just fallback:
       */
      return _mesa_new_program(ctx, target, id);
   }
}

static void
i915DeleteProgram(GLcontext * ctx, struct gl_program *prog)
{
   if (prog->Target == GL_FRAGMENT_PROGRAM_ARB) {
      struct i915_context *i915 = I915_CONTEXT(ctx);
      struct i915_fragment_program *p = (struct i915_fragment_program *) prog;

      if (i915->current_program == p)
         i915->current_program = 0;
   }

   _mesa_delete_program(ctx, prog);
}


static GLboolean
i915IsProgramNative(GLcontext * ctx, GLenum target, struct gl_program *prog)
{
   if (target == GL_FRAGMENT_PROGRAM_ARB) {
      struct i915_fragment_program *p = (struct i915_fragment_program *) prog;

      if (!p->translated)
         translate_program(p);

      return !p->error;
   }
   else
      return GL_TRUE;
}

static void
i915ProgramStringNotify(GLcontext * ctx,
                        GLenum target, struct gl_program *prog)
{
   if (target == GL_FRAGMENT_PROGRAM_ARB) {
      struct i915_fragment_program *p = (struct i915_fragment_program *) prog;
      p->translated = 0;

      /* Hack: make sure fog is correctly enabled according to this
       * fragment program's fog options.
       */
      ctx->Driver.Enable(ctx, GL_FRAGMENT_PROGRAM_ARB,
                         ctx->FragmentProgram.Enabled);

      if (p->FragProg.FogOption) {
         /* add extra instructions to do fog, then turn off FogOption field */
         _mesa_append_fog_code(ctx, &p->FragProg);
         p->FragProg.FogOption = GL_NONE;
      }
   }

   _tnl_program_string(ctx, target, prog);
}


void
i915ValidateFragmentProgram(struct i915_context *i915)
{
   GLcontext *ctx = &i915->intel.ctx;
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;

   struct i915_fragment_program *p =
      (struct i915_fragment_program *) ctx->FragmentProgram._Current;

   const GLuint inputsRead = p->FragProg.Base.InputsRead;
   GLuint s4 = i915->state.Ctx[I915_CTXREG_LIS4] & ~S4_VFMT_MASK;
   GLuint s2 = S2_TEXCOORD_NONE;
   int i, offset = 0;

   if (i915->current_program != p) {
      i915->current_program = p;
   }


   /* Important:
    */
   VB->AttribPtr[VERT_ATTRIB_POS] = VB->NdcPtr;

   if (!p->translated)
      translate_program(p);

   intel->vertex_attr_count = 0;
   intel->wpos_offset = 0;
   intel->wpos_size = 0;
   intel->coloroffset = 0;
   intel->specoffset = 0;

   if (inputsRead & FRAG_BITS_TEX_ANY) {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_4F_VIEWPORT, S4_VFMT_XYZW, 16);
   }
   else {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_3F_VIEWPORT, S4_VFMT_XYZ, 12);
   }

   if (inputsRead & FRAG_BIT_COL0) {
      intel->coloroffset = offset / 4;
      EMIT_ATTR(_TNL_ATTRIB_COLOR0, EMIT_4UB_4F_BGRA, S4_VFMT_COLOR, 4);
   }

   if ((inputsRead & (FRAG_BIT_COL1 | FRAG_BIT_FOGC)) ||
       i915->vertex_fog != I915_FOG_NONE) {

      if (inputsRead & FRAG_BIT_COL1) {
         intel->specoffset = offset / 4;
         EMIT_ATTR(_TNL_ATTRIB_COLOR1, EMIT_3UB_3F_BGR, S4_VFMT_SPEC_FOG, 3);
      }
      else
         EMIT_PAD(3);

      if ((inputsRead & FRAG_BIT_FOGC) || i915->vertex_fog != I915_FOG_NONE)
         EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1UB_1F, S4_VFMT_SPEC_FOG, 1);
      else
         EMIT_PAD(1);
   }

   /* XXX this was disabled, but enabling this code helped fix the Glean
    * tfragprog1 fog tests.
    */
   if ((inputsRead & FRAG_BIT_FOGC) || i915->vertex_fog != I915_FOG_NONE) {
      EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1F, S4_VFMT_FOG_PARAM, 4);
   }

   for (i = 0; i < p->ctx->Const.MaxTextureCoordUnits; i++) {
      if (inputsRead & FRAG_BIT_TEX(i)) {
         int sz = VB->TexCoordPtr[i]->size;

         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(sz));

         EMIT_ATTR(_TNL_ATTRIB_TEX0 + i, EMIT_SZ(sz), 0, sz * 4);
      }
      else if (i == p->wpos_tex) {

         /* If WPOS is required, duplicate the XYZ position data in an
          * unused texture coordinate:
          */
         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(3));

         intel->wpos_offset = offset;
         intel->wpos_size = 3 * sizeof(GLuint);

         EMIT_PAD(intel->wpos_size);
      }
   }

   if (s2 != i915->state.Ctx[I915_CTXREG_LIS2] ||
       s4 != i915->state.Ctx[I915_CTXREG_LIS4]) {
      int k;

      I915_STATECHANGE(i915, I915_UPLOAD_CTX);

      /* Must do this *after* statechange, so as not to affect
       * buffered vertices reliant on the old state:
       */
      intel->vertex_size = _tnl_install_attrs(&intel->ctx,
                                              intel->vertex_attrs,
                                              intel->vertex_attr_count,
                                              intel->ViewportMatrix.m, 0);

      intel->vertex_size >>= 2;

      i915->state.Ctx[I915_CTXREG_LIS2] = s2;
      i915->state.Ctx[I915_CTXREG_LIS4] = s4;
   }

   if (!p->params_uptodate)
      track_params(p);


      i915_upload_program(i915, p);
}

void
i915InitFragProgFuncs(struct dd_function_table *functions)
{
   functions->BindProgram = i915BindProgram;
   functions->NewProgram = i915NewProgram;
   functions->DeleteProgram = i915DeleteProgram;
   functions->IsProgramNative = i915IsProgramNative;
   functions->ProgramStringNotify = i915ProgramStringNotify;
}
