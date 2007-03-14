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
#include "i915_fp.h"

#include "program_instruction.h"
#include "program.h"
#include "programopt.h"


static void brwBindProgram( GLcontext *ctx,
			    GLenum target, 
			    struct gl_program *prog )
{
   struct brw_context *brw = brw_context(ctx);

   switch (target) {
   case GL_VERTEX_PROGRAM_ARB: 
      brw->state.dirty.brw |= BRW_NEW_VERTEX_PROGRAM;
      break;
   case GL_FRAGMENT_PROGRAM_ARB:
      brw->state.dirty.brw |= BRW_NEW_FRAGMENT_PROGRAM;
      break;
   }
}

static struct gl_program *brwNewProgram( GLcontext *ctx,
				      GLenum target, 
				      GLuint id )
{
   struct brw_context *brw = brw_context(ctx);

   switch (target) {
   case GL_VERTEX_PROGRAM_ARB: {
      struct brw_vertex_program *prog = CALLOC_STRUCT(brw_vertex_program);
      if (prog) {
	 prog->id = brw->program_id++;

	 return _mesa_init_vertex_program( ctx, &prog->program,
					     target, id );
      }
      else
	 return NULL;
   }

   case GL_FRAGMENT_PROGRAM_ARB: {
      struct brw_fragment_program *prog = CALLOC_STRUCT(brw_fragment_program);
      if (prog) {
	 prog->id = brw->program_id++;

	 return _mesa_init_fragment_program( ctx, &prog->program,
					     target, id );
      }
      else
	 return NULL;
   }

   default:
      return _mesa_new_program(ctx, target, id);
   }
}

static void brwDeleteProgram( GLcontext *ctx,
			      struct gl_program *prog )
{
   
   _mesa_delete_program( ctx, prog );
}


static GLboolean brwIsProgramNative( GLcontext *ctx,
				     GLenum target, 
				     struct gl_program *prog )
{
   return GL_TRUE;
}

static void brwProgramStringNotify( GLcontext *ctx,
				    GLenum target,
				    struct gl_program *prog )
{
   if (target == GL_FRAGMENT_PROGRAM_ARB) {
      struct brw_context *brw = brw_context(ctx);
      struct brw_fragment_program *p = (struct brw_fragment_program *)prog;
      struct brw_fragment_program *fp = (struct brw_fragment_program *)brw->fragment_program;
      if (p == fp)
	 brw->state.dirty.brw |= BRW_NEW_FRAGMENT_PROGRAM;
      p->id = brw->program_id++;      
      p->param_state = brw_parameter_list_state_flags(p->program.Base.Parameters); 
   }
   else if (target == GL_VERTEX_PROGRAM_ARB) {
      struct brw_context *brw = brw_context(ctx);
      struct brw_vertex_program *p = (struct brw_vertex_program *)prog;
      struct brw_vertex_program *vp = (struct brw_vertex_program *)brw->vertex_program;
      if (p == vp)
	 brw->state.dirty.brw |= BRW_NEW_VERTEX_PROGRAM;
      p->id = brw->program_id++;      
      p->param_state = brw_parameter_list_state_flags(p->program.Base.Parameters); 

      /* Also tell tnl about it:
       */
      _tnl_program_string(ctx, target, prog);
   }
}

void brwInitFragProgFuncs( struct dd_function_table *functions )
{
   assert(functions->ProgramStringNotify == _tnl_program_string); 

   functions->BindProgram = brwBindProgram;
   functions->NewProgram = brwNewProgram;
   functions->DeleteProgram = brwDeleteProgram;
   functions->IsProgramNative = brwIsProgramNative;
   functions->ProgramStringNotify = brwProgramStringNotify;
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
i915InitFragProgFuncs(struct dd_function_table *functions)
{
   functions->BindProgram = i915BindProgram;
   functions->NewProgram = i915NewProgram;
   functions->DeleteProgram = i915DeleteProgram;
   functions->IsProgramNative = i915IsProgramNative;
   functions->ProgramStringNotify = i915ProgramStringNotify;
}
