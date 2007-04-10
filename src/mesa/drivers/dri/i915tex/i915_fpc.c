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

#include <strings.h>

#include "glheader.h"
#include "macros.h"
#include "enums.h"

#include "i915_fpc.h"



void
i915_program_error(struct i915_fp_compile *p, const char *msg)
{
   _mesa_problem(NULL, "i915_program_error: %s", msg);
   p->fp->error = 1;
}


static struct i915_fp_compile *
i915_init_compile(struct i915_context *i915, struct i915_fragment_program *fp)
{
   struct i915_fp_compile *p = CALLOC_STRUCT(i915_fp_compile);

   p->fp = fp;
   p->env_param = i915->intel.ctx.FragmentProgram.Parameters;

   p->nr_tex_indirect = 1;      /* correct? */
   p->nr_tex_insn = 0;
   p->nr_alu_insn = 0;
   p->nr_decl_insn = 0;

   memset(p->constant_flags, 0, sizeof(p->constant_flags));

   p->csr = p->program;
   p->decl = p->declarations;
   p->decl_s = 0;
   p->decl_t = 0;
   p->temp_flag = 0xffff000;
   p->utemp_flag = ~0x7;

   p->fp->translated = 0;
   p->fp->error = 0;
   p->fp->nr_constants = 0;
   p->fp->wpos_tex = -1;
   p->fp->nr_params = 0;

   *(p->decl++) = _3DSTATE_PIXEL_SHADER_PROGRAM;

   return p;
}

/* Copy compile results to the fragment program struct and destroy the
 * compilation context.
 */
static void
i915_fini_compile(struct i915_fp_compile *p)
{
   GLuint program_size = p->csr - p->program;
   GLuint decl_size = p->decl - p->declarations;

   if (p->nr_tex_indirect > I915_MAX_TEX_INDIRECT)
      i915_program_error(p, "Exceeded max nr indirect texture lookups");

   if (p->nr_tex_insn > I915_MAX_TEX_INSN)
      i915_program_error(p, "Exceeded max TEX instructions");

   if (p->nr_alu_insn > I915_MAX_ALU_INSN)
      i915_program_error(p, "Exceeded max ALU instructions");

   if (p->nr_decl_insn > I915_MAX_DECL_INSN)
      i915_program_error(p, "Exceeded max DECL instructions");

   if (p->fp->error) {
      p->fp->Base.Base.NumNativeInstructions = 0;
      p->fp->Base.Base.NumNativeAluInstructions = 0;
      p->fp->Base.Base.NumNativeTexInstructions = 0;
      p->fp->Base.Base.NumNativeTexIndirections = 0;
      return;
   }
   else {
      p->fp->Base.Base.NumNativeInstructions = (p->nr_alu_insn +
						    p->nr_tex_insn +
						    p->nr_decl_insn);
      p->fp->Base.Base.NumNativeAluInstructions = p->nr_alu_insn;
      p->fp->Base.Base.NumNativeTexInstructions = p->nr_tex_insn;
      p->fp->Base.Base.NumNativeTexIndirections = p->nr_tex_indirect;
   }

   p->declarations[0] |= program_size + decl_size - 2;

   /* Copy compilation results to fragment program struct: 
    */
   memcpy(p->fp->program, 
	  p->declarations, 
	  decl_size * sizeof(GLuint));

   memcpy(p->fp->program + decl_size, 
	  p->program, 
	  program_size * sizeof(GLuint));
      
   p->fp->program_size = program_size + decl_size;

   /* Release the compilation struct: 
    */
   FREE(p);
}



static void
check_wpos(struct i915_fp_compile *p)
{
   GLuint inputs = p->fp->Base.Base.InputsRead;
   GLint i;

   p->fp->wpos_tex = -1;

   if (inputs & FRAG_BIT_WPOS) {
      for (i = 0; i < I915_TEX_UNITS; i++) {
	 if ((inputs & (FRAG_BIT_TEX0 << i)) == 0) {
	    p->fp->wpos_tex = i;
	    return;
	 }
      }

      i915_program_error(p, "No free texcoord for wpos value");
   }
}



void i915_compile_fragment_program( struct i915_context *i915,
				    struct i915_fragment_program *fp )
{
   struct i915_fp_compile *p = i915_init_compile(i915, fp);

   check_wpos(p);

   i915_translate_program(p);
   i915_fixup_depth_write(p);

   i915_fini_compile(p);
   fp->translated = 1;
}
