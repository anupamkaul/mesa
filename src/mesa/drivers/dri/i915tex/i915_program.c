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

#include "program_instruction.h"
#include "program.h"
#include "programopt.h"


static void i915BindProgram( GLcontext *ctx,
			    GLenum target, 
			    struct gl_program *prog )
{
   struct intel_context *intel = intel_context(ctx);

   switch (target) {
   case GL_VERTEX_PROGRAM_ARB: 
/*       intel->state.dirty.intel |= INTEL_NEW_VERTEX_PROGRAM; */
      break;
   case GL_FRAGMENT_PROGRAM_ARB:
      intel->state.dirty.intel |= INTEL_NEW_FRAGMENT_PROGRAM;
      break;
   }
}

static struct gl_program *i915NewProgram( GLcontext *ctx,
					  GLenum target, 
					  GLuint id )
{
   struct i915_context *i915 = i915_context(ctx);

   switch (target) {
   case GL_VERTEX_PROGRAM_ARB:
      return _mesa_init_vertex_program(ctx, CALLOC_STRUCT(gl_vertex_program),
                                       target, id);

   case GL_FRAGMENT_PROGRAM_ARB: {
      struct i915_fragment_program *prog = CALLOC_STRUCT(i915_fragment_program);
      if (prog) {
	 prog->id = i915->program_id++;

	 return _mesa_init_fragment_program( ctx, &prog->Base,
					     target, id );
      }
      else
	 return NULL;
   }

   default:
      return _mesa_new_program(ctx, target, id);
   }
}

static void i915DeleteProgram( GLcontext *ctx,
			      struct gl_program *prog )
{
   
   _mesa_delete_program( ctx, prog );
}


static GLboolean i915IsProgramNative( GLcontext *ctx,
				     GLenum target, 
				     struct gl_program *prog )
{
   return GL_TRUE;
}

static void i915ProgramStringNotify( GLcontext *ctx,
				    GLenum target,
				    struct gl_program *prog )
{
   if (target == GL_FRAGMENT_PROGRAM_ARB) {
      struct i915_context *i915 = i915_context(ctx);
      struct i915_fragment_program *p = (struct i915_fragment_program *)prog;
      struct i915_fragment_program *fp = (struct i915_fragment_program *)i915->fragment_program;
      if (p == fp)
	 i915->intel.state.dirty.intel |= INTEL_NEW_FRAGMENT_PROGRAM;
      p->id = i915->program_id++;      
      p->param_state = p->Base.Base.Parameters->StateFlags; 
   }
   else if (target == GL_VERTEX_PROGRAM_ARB) {

      /* Also tell tnl about it:
       */
      _tnl_program_string(ctx, target, prog);
   }
}

void i915InitFragProgFuncs( struct dd_function_table *functions )
{
   assert(functions->ProgramStringNotify == _tnl_program_string); 

   functions->BindProgram = i915BindProgram;
   functions->NewProgram = i915NewProgram;
   functions->DeleteProgram = i915DeleteProgram;
   functions->IsProgramNative = i915IsProgramNative;
   functions->ProgramStringNotify = i915ProgramStringNotify;
}




