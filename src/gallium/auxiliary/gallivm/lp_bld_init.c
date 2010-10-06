/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.
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
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#include "pipe/p_compiler.h"
#include "util/u_cpu_detect.h"
#include "util/u_debug.h"
#include "lp_bld_debug.h"
#include "lp_bld_init.h"

#include <llvm-c/Transforms/Scalar.h>


#ifdef DEBUG
unsigned gallivm_debug = 0;

static const struct debug_named_value lp_bld_debug_flags[] = {
   { "tgsi",   GALLIVM_DEBUG_TGSI, NULL },
   { "ir",     GALLIVM_DEBUG_IR, NULL },
   { "asm",    GALLIVM_DEBUG_ASM, NULL },
   { "nopt",   GALLIVM_DEBUG_NO_OPT, NULL },
   { "perf",   GALLIVM_DEBUG_PERF, NULL },
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(gallivm_debug, "GALLIVM_DEBUG", lp_bld_debug_flags, 0)
#endif


struct gallivm_state gallivm = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };



/*
 * Optimization values are:
 * - 0: None (-O0)
 * - 1: Less (-O1)
 * - 2: Default (-O2, -Os)
 * - 3: Aggressive (-O3)
 *
 * See also CodeGenOpt::Level in llvm/Target/TargetMachine.h
 */
enum LLVM_CodeGenOpt_Level {
#if HAVE_LLVM >= 0x207
   None,        // -O0
   Less,        // -O1
   Default,     // -O2, -Os
   Aggressive   // -O3
#else
   Default,
   None,
   Aggressive
#endif
};


extern void
lp_register_oprofile_jit_event_listener(LLVMExecutionEngineRef EE);

extern void
lp_set_target_options(void);



/**
 * Create the LLVM (optimization) pass manager and install
 * relevant optimization passes.
 */
static void
create_pass_manager(struct gallivm_state *gallivm)
{
   assert(!gallivm->passmgr);

   gallivm->passmgr = LLVMCreateFunctionPassManager(gallivm->provider);
   LLVMAddTargetData(gallivm->target, gallivm->passmgr);

   if ((gallivm_debug & GALLIVM_DEBUG_NO_OPT) == 0) {
      /* These are the passes currently listed in llvm-c/Transforms/Scalar.h,
       * but there are more on SVN.
       * TODO: Add more passes.
       */
      LLVMAddCFGSimplificationPass(gallivm->passmgr);

      if (HAVE_LLVM >= 0x207 && sizeof(void*) == 4) {
         /* For LLVM >= 2.7 and 32-bit build, use this order of passes to
          * avoid generating bad code.
          * Test with piglit glsl-vs-sqrt-zero test.
          */
         LLVMAddConstantPropagationPass(gallivm->passmgr);
         LLVMAddPromoteMemoryToRegisterPass(gallivm->passmgr);
      }
      else {
         LLVMAddPromoteMemoryToRegisterPass(gallivm->passmgr);
         LLVMAddConstantPropagationPass(gallivm->passmgr);
      }

      if (util_cpu_caps.has_sse4_1) {
         /* FIXME: There is a bug in this pass, whereby the combination
          * of fptosi and sitofp (necessary for trunc/floor/ceil/round
          * implementation) somehow becomes invalid code.
          */
         LLVMAddInstructionCombiningPass(gallivm->passmgr);
      }
      LLVMAddGVNPass(gallivm->passmgr);
   }
   else {
      /* We need at least this pass to prevent the backends to fail in
       * unexpected ways.
       */
      LLVMAddPromoteMemoryToRegisterPass(gallivm->passmgr);
   }

   assert(gallivm->passmgr);
}


/**
 * Create the global LLVM resources.
 */
static void
init_gallivm_state(struct gallivm_state *gallivm)
{
   if (!gallivm->context)
      gallivm->context = LLVMContextCreate();

   if (!gallivm->module)
      gallivm->module = LLVMModuleCreateWithNameInContext("gallivm",
                                                          gallivm->context);

   if (!gallivm->provider)
      gallivm->provider = LLVMCreateModuleProviderForExistingModule(gallivm->module);

   if (!gallivm->engine) {
      enum LLVM_CodeGenOpt_Level optlevel;
      char *error = NULL;

      if (gallivm_debug & GALLIVM_DEBUG_NO_OPT) {
         optlevel = None;
      }
      else {
         optlevel = Default;
      }

      if (LLVMCreateJITCompiler(&gallivm->engine, gallivm->provider,
                                (unsigned)optlevel, &error)) {
         _debug_printf("%s\n", error);
         LLVMDisposeMessage(error);
         assert(0);
      }

#if defined(DEBUG) || defined(PROFILE)
      lp_register_oprofile_jit_event_listener(gallivm->engine);
#endif
   }

   if (!gallivm->target)
      gallivm->target = LLVMGetExecutionEngineTargetData(gallivm->engine);

   if (!gallivm->passmgr) {
      create_pass_manager(gallivm);
   }
}


/**
 * Free all global LLVM resources.
 */
static void
free_gallivm_state(struct gallivm_state *gallivm)
{
   LLVMModuleRef mod;
   char *error;

   LLVMRemoveModuleProvider(gallivm->engine, gallivm->provider,
                            &mod, &error);

   LLVMDisposePassManager(gallivm->passmgr);
   LLVMDisposeModule(gallivm->module);
   LLVMDisposeExecutionEngine(gallivm->engine);
   LLVMContextDispose(gallivm->context);

   gallivm->engine = NULL;
   gallivm->target = NULL;
   gallivm->module = NULL;
   gallivm->provider = NULL;
   gallivm->passmgr = NULL;
   gallivm->context = NULL;
   gallivm->builder = NULL;
}



struct callback
{
   garbage_collect_callback_func func;
   void *cb_data;
};


#define MAX_CALLBACKS 8
static struct callback Callbacks[MAX_CALLBACKS];
static unsigned NumCallbacks = 0;


/**
 * Register a function with gallivm which will be called when we
 * do garbage collection.
 */
void
lp_register_garbage_collector_callback(garbage_collect_callback_func func,
                                       void *cb_data)
{
   unsigned i;

   for (i = 0; i < NumCallbacks; i++) {
      if (Callbacks[i].func == func) {
         /* already in list, just update callback data */
         Callbacks[i].cb_data = cb_data;
         return;
      }
   }

   assert(NumCallbacks < MAX_CALLBACKS);
   if (NumCallbacks < MAX_CALLBACKS) {
      Callbacks[NumCallbacks].func = func;
      Callbacks[NumCallbacks].cb_data = cb_data;
      NumCallbacks++;
   }
}


/**
 * Call the callback functions (which are typically in the
 * draw module and llvmpipe driver.
 */
static void
call_garbage_collector_callbacks(void)
{
   unsigned i;

   for (i = 0; i < NumCallbacks; i++) {
      Callbacks[i].func(Callbacks[i].cb_data);
   }
}



/**
 * Other gallium components using gallivm should call this periodically
 * to let us do garbage collection (or at least try to free memory
 * accumulated by the LLVM libraries).
 */
boolean
lp_garbage_collect(void)
{
   static uint counter = 0;

   counter++;
   debug_printf("%s %d\n", __FUNCTION__, counter);
   if (counter >= 10) {
      if (gallivm.context) {
         if (1)
            debug_printf("***** Doing LLVM garbage collection\n");

         call_garbage_collector_callbacks();
         free_gallivm_state(&gallivm);
         init_gallivm_state(&gallivm);
      }

      counter = 0;
      return TRUE;
   }
   else {
      return FALSE;
   }
}


void
lp_build_init(void)
{
#ifdef DEBUG
   gallivm_debug = debug_get_option_gallivm_debug();
#endif

   lp_set_target_options();

   LLVMInitializeNativeTarget();

   LLVMLinkInJIT();

   init_gallivm_state(&gallivm);
 
   util_cpu_detect();
 
#if 0
   /* For simulating less capable machines */
   util_cpu_caps.has_sse3 = 0;
   util_cpu_caps.has_ssse3 = 0;
   util_cpu_caps.has_sse4_1 = 0;
#endif

   if (0)
      atexit(lp_build_cleanup);
}


void
lp_build_cleanup(void)
{
   free_gallivm_state(&gallivm);
}



/* 
 * Hack to allow the linking of release LLVM static libraries on a debug build.
 *
 * See also:
 * - http://social.msdn.microsoft.com/Forums/en-US/vclanguage/thread/7234ea2b-0042-42ed-b4e2-5d8644dfb57d
 */
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdefs.h>
_CRTIMP void __cdecl
_invalid_parameter_noinfo(void) {}
#endif
