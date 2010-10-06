/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
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

#include "draw_llvm.h"

#include "draw_context.h"
#include "draw_vs.h"

#include "gallivm/lp_bld_arit.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_struct.h"
#include "gallivm/lp_bld_type.h"
#include "gallivm/lp_bld_flow.h"
#include "gallivm/lp_bld_debug.h"
#include "gallivm/lp_bld_tgsi.h"
#include "gallivm/lp_bld_printf.h"
#include "gallivm/lp_bld_intr.h"
#include "gallivm/lp_bld_init.h"

#include "tgsi/tgsi_exec.h"
#include "tgsi/tgsi_dump.h"

#include "util/u_cpu_detect.h"
#include "util/u_pointer.h"
#include "util/u_string.h"
#include "util/u_simple_list.h"


#define DEBUG_STORE 0


struct draw_llvm_globals draw_llvm_global;


/**
 * This function is called by the gallivm "garbage collector" when
 * the LLVM global data structures are freed.  We must free all LLVM-related
 * data.  Specifically, all JIT'd shader variants.
 */
static void
draw_llvm_garbage_collect_callback(void *cb_data)
{
   struct draw_llvm_variant_list_item *li;

   /* free all shader variants */
   li = first_elem(&draw_llvm_global.vs_variants_list);
   while (!at_end(&draw_llvm_global.vs_variants_list, li)) {
      struct draw_llvm_variant_list_item *next = next_elem(li);
      draw_llvm_destroy_variant(li->base);
      li = next;
   }

   /* Null-out global pointers so they get remade next time they're needed.
    * See the accessor functions below.
    */
   draw_llvm_global.context_ptr_type = NULL;
   draw_llvm_global.buffer_ptr_type = NULL;
   draw_llvm_global.vb_ptr_type = NULL;
   draw_llvm_global.vertex_header_ptr_type = NULL;
}


/**
 * One-time inits for llvm-related data.
 */
static void
draw_llvm_init_globals(void)
{
   static boolean initialized = FALSE;
   if (!initialized) {
      memset(&draw_llvm_global, 0, sizeof(draw_llvm_global));

      make_empty_list(&draw_llvm_global.vs_variants_list);

      lp_register_garbage_collector_callback(
                     draw_llvm_garbage_collect_callback, NULL);

      initialized = TRUE;
   }
}



static void
draw_llvm_generate(struct draw_llvm *llvm, struct draw_llvm_variant *var);

static void
draw_llvm_generate_elts(struct draw_llvm *llvm, struct draw_llvm_variant *var);


/**
 * Create LLVM type for struct draw_jit_texture
 */
static LLVMTypeRef
create_jit_texture_type(LLVMTargetDataRef target)
{
   LLVMTypeRef texture_type;
   LLVMTypeRef elem_types[DRAW_JIT_TEXTURE_NUM_FIELDS];

   elem_types[DRAW_JIT_TEXTURE_WIDTH]  =
   elem_types[DRAW_JIT_TEXTURE_HEIGHT] =
   elem_types[DRAW_JIT_TEXTURE_DEPTH] =
   elem_types[DRAW_JIT_TEXTURE_LAST_LEVEL] = LLVMInt32TypeInContext(LC);
   elem_types[DRAW_JIT_TEXTURE_ROW_STRIDE] =
   elem_types[DRAW_JIT_TEXTURE_IMG_STRIDE] =
      LLVMArrayType(LLVMInt32TypeInContext(LC), DRAW_MAX_TEXTURE_LEVELS);
   elem_types[DRAW_JIT_TEXTURE_DATA] =
      LLVMArrayType(LLVMPointerType(LLVMInt8TypeInContext(LC), 0),
                    DRAW_MAX_TEXTURE_LEVELS);
   elem_types[DRAW_JIT_TEXTURE_MIN_LOD] =
   elem_types[DRAW_JIT_TEXTURE_MAX_LOD] =
   elem_types[DRAW_JIT_TEXTURE_LOD_BIAS] = LLVMFloatTypeInContext(LC);
   elem_types[DRAW_JIT_TEXTURE_BORDER_COLOR] = 
      LLVMArrayType(LLVMFloatTypeInContext(LC), 4);

   texture_type = LLVMStructTypeInContext(LC, elem_types,
                                          Elements(elem_types), 0);

   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, width,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_WIDTH);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, height,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_HEIGHT);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, depth,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_DEPTH);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, last_level,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_LAST_LEVEL);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, row_stride,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_ROW_STRIDE);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, img_stride,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_IMG_STRIDE);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, data,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_DATA);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, min_lod,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_MIN_LOD);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, max_lod,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_MAX_LOD);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, lod_bias,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_LOD_BIAS);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, border_color,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_BORDER_COLOR);

   LP_CHECK_STRUCT_SIZE(struct draw_jit_texture, target, texture_type);

   return texture_type;
}


/**
 * Create LLVM type for struct draw_jit_texture
 */
static LLVMTypeRef
create_jit_context_type(LLVMTargetDataRef target, LLVMTypeRef texture_type)
{
   LLVMTypeRef elem_types[3];
   LLVMTypeRef context_type;

   elem_types[0] = /* vs_constants */
   elem_types[1] = LLVMPointerType(LLVMFloatTypeInContext(LC), 0); /* vs_constants */
   elem_types[2] = LLVMArrayType(texture_type,
                                 PIPE_MAX_VERTEX_SAMPLERS); /* textures */

   context_type = LLVMStructTypeInContext(LC, elem_types,
                                          Elements(elem_types), 0);

   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, vs_constants,
                          target, context_type, 0);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, gs_constants,
                          target, context_type, 1);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, textures,
                          target, context_type,
                          DRAW_JIT_CTX_TEXTURES);

   LP_CHECK_STRUCT_SIZE(struct draw_jit_context, target, context_type);

   return context_type;
}


/**
 * Create LLVM type for struct pipe_vertex_buffer
 */
static LLVMTypeRef
create_jit_vertex_buffer_type(LLVMTargetDataRef target)
{
   LLVMTypeRef elem_types[4];
   LLVMTypeRef vb_type;

   elem_types[0] =
   elem_types[1] =
   elem_types[2] = LLVMInt32TypeInContext(LC);
   elem_types[3] = LLVMPointerType(LLVMOpaqueTypeInContext(LC), 0); /* vs_constants */

   vb_type = LLVMStructTypeInContext(LC, elem_types,
                                     Elements(elem_types), 0);

   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, stride,
                          target, vb_type, 0);
   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, buffer_offset,
                          target, vb_type, 2);

   LP_CHECK_STRUCT_SIZE(struct pipe_vertex_buffer, target, vb_type);

   return vb_type;
}


/**
 * Create LLVM type for struct vertex_header;
 */
static LLVMTypeRef
create_jit_vertex_header(LLVMTargetDataRef target,
                         LLVMModuleRef module, int data_elems)
{
   LLVMTypeRef elem_types[3];
   LLVMTypeRef vertex_header;
   char struct_name[24];

   util_snprintf(struct_name, 23, "vertex_header%d", data_elems);

   elem_types[0]  = LLVMIntTypeInContext(LC, 32);
   elem_types[1]  = LLVMArrayType(LLVMFloatTypeInContext(LC), 4);
   elem_types[2]  = LLVMArrayType(elem_types[1], data_elems);

   vertex_header = LLVMStructTypeInContext(LC, elem_types,
                                           Elements(elem_types), 0);

   /* these are bit-fields and we can't take address of them
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, clipmask,
      target, vertex_header,
      DRAW_JIT_VERTEX_CLIPMASK);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, edgeflag,
      target, vertex_header,
      DRAW_JIT_VERTEX_EDGEFLAG);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, pad,
      target, vertex_header,
      DRAW_JIT_VERTEX_PAD);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, vertex_id,
      target, vertex_header,
      DRAW_JIT_VERTEX_VERTEX_ID);
   */
   LP_CHECK_MEMBER_OFFSET(struct vertex_header, clip,
                          target, vertex_header,
                          DRAW_JIT_VERTEX_CLIP);
   LP_CHECK_MEMBER_OFFSET(struct vertex_header, data,
                          target, vertex_header,
                          DRAW_JIT_VERTEX_DATA);

   LLVMAddTypeName(module, struct_name, vertex_header);

   return vertex_header;
}


/**
 * Create LLVM types for various structures.
 */
static void
create_global_types(void)
{
   LLVMTypeRef texture_type, context_type, buffer_type, vb_type;

   texture_type = create_jit_texture_type(gallivm.target);
   LLVMAddTypeName(gallivm.module, "texture", texture_type);

   context_type = create_jit_context_type(gallivm.target, texture_type);
   LLVMAddTypeName(gallivm.module, "draw_jit_context", context_type);
   draw_llvm_global.context_ptr_type = LLVMPointerType(context_type, 0);

   buffer_type = LLVMPointerType(LLVMIntTypeInContext(LC, 8), 0);
   LLVMAddTypeName(gallivm.module, "buffer", buffer_type);
   draw_llvm_global.buffer_ptr_type = LLVMPointerType(buffer_type, 0);

   vb_type = create_jit_vertex_buffer_type(gallivm.target);
   LLVMAddTypeName(gallivm.module, "pipe_vertex_buffer", vb_type);
   draw_llvm_global.vb_ptr_type = LLVMPointerType(vb_type, 0);
}


static LLVMTypeRef
get_context_ptr_type(void)
{
   if (!draw_llvm_global.context_ptr_type)
      create_global_types();
   return draw_llvm_global.context_ptr_type;
}


static LLVMTypeRef
get_buffer_ptr_type(void)
{
   if (!draw_llvm_global.buffer_ptr_type)
      create_global_types();
   return draw_llvm_global.buffer_ptr_type;
}


static LLVMTypeRef
get_vb_ptr_type(void)
{
   if (!draw_llvm_global.vb_ptr_type)
      create_global_types();
   return draw_llvm_global.vb_ptr_type;
}


static LLVMTypeRef
get_vertex_header_ptr_type(void)
{
   if (!draw_llvm_global.vertex_header_ptr_type)
      create_global_types();
   return draw_llvm_global.vertex_header_ptr_type;
}


/**
 * Create per-context LLVM info.
 */
struct draw_llvm *
draw_llvm_create(struct draw_context *draw)
{
   struct draw_llvm *llvm;

   draw_llvm_init_globals();

   llvm = CALLOC_STRUCT( draw_llvm );
   if (!llvm)
      return NULL;

   lp_build_init();

   llvm->draw = draw;

   if (gallivm_debug & GALLIVM_DEBUG_IR) {
      LLVMDumpModule(gallivm.module);
   }

   draw_llvm_global.nr_variants = 0;
   make_empty_list(&draw_llvm_global.vs_variants_list);

   return llvm;
}


/**
 * Free per-context LLVM info.
 */
void
draw_llvm_destroy(struct draw_llvm *llvm)
{
   FREE(llvm);
}


/**
 * Create LLVM-generated code for a vertex shader.
 */
struct draw_llvm_variant *
draw_llvm_create_variant(struct draw_llvm *llvm,
			 unsigned num_inputs,
			 const struct draw_llvm_variant_key *key)
{
   struct draw_llvm_variant *variant;
   struct llvm_vertex_shader *shader =
      llvm_vertex_shader(llvm->draw->vs.vertex_shader);
   LLVMTypeRef vertex_header;

   variant = MALLOC(sizeof *variant +
		    shader->variant_key_size -
		    sizeof variant->key);
   if (variant == NULL)
      return NULL;

   variant->llvm = llvm;

   memcpy(&variant->key, key, shader->variant_key_size);

   vertex_header = create_jit_vertex_header(gallivm.target, gallivm.module,
                                            num_inputs);
   draw_llvm_global.vertex_header_ptr_type = LLVMPointerType(vertex_header, 0);

   draw_llvm_generate(llvm, variant);
   draw_llvm_generate_elts(llvm, variant);

   variant->shader = shader;
   variant->list_item_global.base = variant;
   variant->list_item_local.base = variant;
   /*variant->no = */shader->variants_created++;

   return variant;
}

static void
generate_vs(struct draw_llvm *llvm,
            LLVMBuilderRef builder,
            LLVMValueRef (*outputs)[NUM_CHANNELS],
            const LLVMValueRef (*inputs)[NUM_CHANNELS],
            LLVMValueRef context_ptr,
            struct lp_build_sampler_soa *draw_sampler)
{
   const struct tgsi_token *tokens = llvm->draw->vs.vertex_shader->state.tokens;
   struct lp_type vs_type;
   LLVMValueRef consts_ptr = draw_jit_context_vs_constants(builder, context_ptr);
   struct lp_build_sampler_soa *sampler = 0;

   memset(&vs_type, 0, sizeof vs_type);
   vs_type.floating = TRUE; /* floating point values */
   vs_type.sign = TRUE;     /* values are signed */
   vs_type.norm = FALSE;    /* values are not limited to [0,1] or [-1,1] */
   vs_type.width = 32;      /* 32-bit float */
   vs_type.length = 4;      /* 4 elements per vector */
#if 0
   num_vs = 4;              /* number of vertices per block */
#endif

   if (gallivm_debug & GALLIVM_DEBUG_IR) {
      tgsi_dump(tokens, 0);
   }

   if (llvm->draw->num_sampler_views &&
       llvm->draw->num_samplers)
      sampler = draw_sampler;

   lp_build_tgsi_soa(builder,
                     tokens,
                     vs_type,
                     NULL /*struct lp_build_mask_context *mask*/,
                     consts_ptr,
                     NULL /*pos*/,
                     inputs,
                     outputs,
                     sampler,
                     &llvm->draw->vs.vertex_shader->info);
}

#if DEBUG_STORE
static void print_vectorf(LLVMBuilderRef builder,
                         LLVMValueRef vec)
{
   LLVMValueRef val[4];
   val[0] = LLVMBuildExtractElement(builder, vec,
                                    lp_build_const_int32(0), "");
   val[1] = LLVMBuildExtractElement(builder, vec,
                                    lp_build_const_int32(1), "");
   val[2] = LLVMBuildExtractElement(builder, vec,
                                    lp_build_const_int32(2), "");
   val[3] = LLVMBuildExtractElement(builder, vec,
                                    lp_build_const_int32(3), "");
   lp_build_printf(builder, "vector = [%f, %f, %f, %f]\n",
                   val[0], val[1], val[2], val[3]);
}
#endif

static void
generate_fetch(LLVMBuilderRef builder,
               LLVMValueRef vbuffers_ptr,
               LLVMValueRef *res,
               struct pipe_vertex_element *velem,
               LLVMValueRef vbuf,
               LLVMValueRef index,
               LLVMValueRef instance_id)
{
   LLVMValueRef indices = LLVMConstInt(LLVMInt64TypeInContext(LC), velem->vertex_buffer_index, 0);
   LLVMValueRef vbuffer_ptr = LLVMBuildGEP(builder, vbuffers_ptr,
                                           &indices, 1, "");
   LLVMValueRef vb_stride = draw_jit_vbuffer_stride(builder, vbuf);
   LLVMValueRef vb_max_index = draw_jit_vbuffer_max_index(builder, vbuf);
   LLVMValueRef vb_buffer_offset = draw_jit_vbuffer_offset(builder, vbuf);
   LLVMValueRef cond;
   LLVMValueRef stride;

   if (velem->instance_divisor) {
      /* array index = instance_id / instance_divisor */
      index = LLVMBuildUDiv(builder, instance_id,
                            lp_build_const_int32(velem->instance_divisor),
                            "instance_divisor");
   }

   /* limit index to min(inex, vb_max_index) */
   cond = LLVMBuildICmp(builder, LLVMIntULE, index, vb_max_index, "");
   index = LLVMBuildSelect(builder, cond, index, vb_max_index, "");

   stride = LLVMBuildMul(builder, vb_stride, index, "");

   vbuffer_ptr = LLVMBuildLoad(builder, vbuffer_ptr, "vbuffer");

   stride = LLVMBuildAdd(builder, stride,
                         vb_buffer_offset,
                         "");
   stride = LLVMBuildAdd(builder, stride,
                         lp_build_const_int32(velem->src_offset),
                         "");

   /*lp_build_printf(builder, "vbuf index = %d, stride is %d\n", indices, stride);*/
   vbuffer_ptr = LLVMBuildGEP(builder, vbuffer_ptr, &stride, 1, "");

   *res = draw_llvm_translate_from(builder, vbuffer_ptr, velem->src_format);
}

static LLVMValueRef
aos_to_soa(LLVMBuilderRef builder,
           LLVMValueRef val0,
           LLVMValueRef val1,
           LLVMValueRef val2,
           LLVMValueRef val3,
           LLVMValueRef channel)
{
   LLVMValueRef ex, res;

   ex = LLVMBuildExtractElement(builder, val0,
                                channel, "");
   res = LLVMBuildInsertElement(builder,
                                LLVMConstNull(LLVMTypeOf(val0)),
                                ex,
                                lp_build_const_int32(0),
                                "");

   ex = LLVMBuildExtractElement(builder, val1,
                                channel, "");
   res = LLVMBuildInsertElement(builder,
                                res, ex,
                                lp_build_const_int32(1),
                                "");

   ex = LLVMBuildExtractElement(builder, val2,
                                channel, "");
   res = LLVMBuildInsertElement(builder,
                                res, ex,
                                lp_build_const_int32(2),
                                "");

   ex = LLVMBuildExtractElement(builder, val3,
                                channel, "");
   res = LLVMBuildInsertElement(builder,
                                res, ex,
                                lp_build_const_int32(3),
                                "");

   return res;
}

static void
soa_to_aos(LLVMBuilderRef builder,
           LLVMValueRef soa[NUM_CHANNELS],
           LLVMValueRef aos[NUM_CHANNELS])
{
   LLVMValueRef comp;
   int i = 0;

   debug_assert(NUM_CHANNELS == 4);

   aos[0] = LLVMConstNull(LLVMTypeOf(soa[0]));
   aos[1] = aos[2] = aos[3] = aos[0];

   for (i = 0; i < NUM_CHANNELS; ++i) {
      LLVMValueRef channel = lp_build_const_int32(i);

      comp = LLVMBuildExtractElement(builder, soa[i],
                                     lp_build_const_int32(0), "");
      aos[0] = LLVMBuildInsertElement(builder, aos[0], comp, channel, "");

      comp = LLVMBuildExtractElement(builder, soa[i],
                                     lp_build_const_int32(1), "");
      aos[1] = LLVMBuildInsertElement(builder, aos[1], comp, channel, "");

      comp = LLVMBuildExtractElement(builder, soa[i],
                                     lp_build_const_int32(2), "");
      aos[2] = LLVMBuildInsertElement(builder, aos[2], comp, channel, "");

      comp = LLVMBuildExtractElement(builder, soa[i],
                                     lp_build_const_int32(3), "");
      aos[3] = LLVMBuildInsertElement(builder, aos[3], comp, channel, "");

   }
}

static void
convert_to_soa(LLVMBuilderRef builder,
               LLVMValueRef (*aos)[NUM_CHANNELS],
               LLVMValueRef (*soa)[NUM_CHANNELS],
               int num_attribs)
{
   int i;

   debug_assert(NUM_CHANNELS == 4);

   for (i = 0; i < num_attribs; ++i) {
      LLVMValueRef val0 = aos[i][0];
      LLVMValueRef val1 = aos[i][1];
      LLVMValueRef val2 = aos[i][2];
      LLVMValueRef val3 = aos[i][3];

      soa[i][0] = aos_to_soa(builder, val0, val1, val2, val3,
                             lp_build_const_int32(0));
      soa[i][1] = aos_to_soa(builder, val0, val1, val2, val3,
                             lp_build_const_int32(1));
      soa[i][2] = aos_to_soa(builder, val0, val1, val2, val3,
                             lp_build_const_int32(2));
      soa[i][3] = aos_to_soa(builder, val0, val1, val2, val3,
                             lp_build_const_int32(3));
   }
}

static void
store_aos(LLVMBuilderRef builder,
          LLVMValueRef io_ptr,
          LLVMValueRef index,
          LLVMValueRef value)
{
   LLVMValueRef id_ptr = draw_jit_header_id(builder, io_ptr);
   LLVMValueRef data_ptr = draw_jit_header_data(builder, io_ptr);
   LLVMValueRef indices[3];

   indices[0] = lp_build_const_int32(0);
   indices[1] = index;
   indices[2] = lp_build_const_int32(0);

   /* undefined vertex */
   LLVMBuildStore(builder, lp_build_const_int32(0xffff), id_ptr);

#if DEBUG_STORE
   lp_build_printf(builder, "    ---- %p storing attribute %d (io = %p)\n", data_ptr, index, io_ptr);
#endif
#if 0
   /*lp_build_printf(builder, " ---- %p storing at %d (%p)  ", io_ptr, index, data_ptr);
     print_vectorf(builder, value);*/
   data_ptr = LLVMBuildBitCast(builder, data_ptr,
                               LLVMPointerType(LLVMArrayType(LLVMVectorType(LLVMFloatTypeInContext(LC), 4), 0), 0),
                               "datavec");
   data_ptr = LLVMBuildGEP(builder, data_ptr, indices, 2, "");

   LLVMBuildStore(builder, value, data_ptr);
#else
   {
      LLVMValueRef x, y, z, w;
      LLVMValueRef idx0, idx1, idx2, idx3;
      LLVMValueRef gep0, gep1, gep2, gep3;
      data_ptr = LLVMBuildGEP(builder, data_ptr, indices, 3, "");

      idx0 = lp_build_const_int32(0);
      idx1 = lp_build_const_int32(1);
      idx2 = lp_build_const_int32(2);
      idx3 = lp_build_const_int32(3);

      x = LLVMBuildExtractElement(builder, value,
                                  idx0, "");
      y = LLVMBuildExtractElement(builder, value,
                                  idx1, "");
      z = LLVMBuildExtractElement(builder, value,
                                  idx2, "");
      w = LLVMBuildExtractElement(builder, value,
                                  idx3, "");

      gep0 = LLVMBuildGEP(builder, data_ptr, &idx0, 1, "");
      gep1 = LLVMBuildGEP(builder, data_ptr, &idx1, 1, "");
      gep2 = LLVMBuildGEP(builder, data_ptr, &idx2, 1, "");
      gep3 = LLVMBuildGEP(builder, data_ptr, &idx3, 1, "");

      /*lp_build_printf(builder, "##### x = %f (%p), y = %f (%p), z = %f (%p), w = %f (%p)\n",
        x, gep0, y, gep1, z, gep2, w, gep3);*/
      LLVMBuildStore(builder, x, gep0);
      LLVMBuildStore(builder, y, gep1);
      LLVMBuildStore(builder, z, gep2);
      LLVMBuildStore(builder, w, gep3);
   }
#endif
}

static void
store_aos_array(LLVMBuilderRef builder,
                LLVMValueRef io_ptr,
                LLVMValueRef aos[NUM_CHANNELS],
                int attrib,
                int num_outputs)
{
   LLVMValueRef attr_index = lp_build_const_int32(attrib);
   LLVMValueRef ind0 = lp_build_const_int32(0);
   LLVMValueRef ind1 = lp_build_const_int32(1);
   LLVMValueRef ind2 = lp_build_const_int32(2);
   LLVMValueRef ind3 = lp_build_const_int32(3);
   LLVMValueRef io0_ptr, io1_ptr, io2_ptr, io3_ptr;

   debug_assert(NUM_CHANNELS == 4);

   io0_ptr = LLVMBuildGEP(builder, io_ptr,
                          &ind0, 1, "");
   io1_ptr = LLVMBuildGEP(builder, io_ptr,
                          &ind1, 1, "");
   io2_ptr = LLVMBuildGEP(builder, io_ptr,
                          &ind2, 1, "");
   io3_ptr = LLVMBuildGEP(builder, io_ptr,
                          &ind3, 1, "");

#if DEBUG_STORE
   lp_build_printf(builder, "   io = %p, indexes[%d, %d, %d, %d]\n",
                   io_ptr, ind0, ind1, ind2, ind3);
#endif

   store_aos(builder, io0_ptr, attr_index, aos[0]);
   store_aos(builder, io1_ptr, attr_index, aos[1]);
   store_aos(builder, io2_ptr, attr_index, aos[2]);
   store_aos(builder, io3_ptr, attr_index, aos[3]);
}

static void
convert_to_aos(LLVMBuilderRef builder,
               LLVMValueRef io,
               LLVMValueRef (*outputs)[NUM_CHANNELS],
               int num_outputs,
               int max_vertices)
{
   unsigned chan, attrib;

#if DEBUG_STORE
   lp_build_printf(builder, "   # storing begin\n");
#endif
   for (attrib = 0; attrib < num_outputs; ++attrib) {
      LLVMValueRef soa[4];
      LLVMValueRef aos[4];
      for(chan = 0; chan < NUM_CHANNELS; ++chan) {
         if(outputs[attrib][chan]) {
            LLVMValueRef out = LLVMBuildLoad(builder, outputs[attrib][chan], "");
            lp_build_name(out, "output%u.%c", attrib, "xyzw"[chan]);
            /*lp_build_printf(builder, "output %d : %d ",
                            LLVMConstInt(LLVMInt32Type(), attrib, 0),
                            LLVMConstInt(LLVMInt32Type(), chan, 0));
              print_vectorf(builder, out);*/
            soa[chan] = out;
         } else
            soa[chan] = 0;
      }
      soa_to_aos(builder, soa, aos);
      store_aos_array(builder,
                      io,
                      aos,
                      attrib,
                      num_outputs);
   }
#if DEBUG_STORE
   lp_build_printf(builder, "   # storing end\n");
#endif
}

static void
draw_llvm_generate(struct draw_llvm *llvm, struct draw_llvm_variant *variant)
{
   LLVMTypeRef arg_types[8];
   LLVMTypeRef func_type;
   LLVMValueRef context_ptr;
   LLVMBasicBlockRef block;
   LLVMBuilderRef builder;
   LLVMValueRef start, end, count, stride, step, io_itr;
   LLVMValueRef io_ptr, vbuffers_ptr, vb_ptr;
   LLVMValueRef instance_id;
   struct draw_context *draw = llvm->draw;
   unsigned i, j;
   struct lp_build_context bld;
   struct lp_build_loop_state lp_loop;
   const int max_vertices = 4;
   LLVMValueRef outputs[PIPE_MAX_SHADER_OUTPUTS][NUM_CHANNELS];
   void *code;
   struct lp_build_sampler_soa *sampler = 0;

   arg_types[0] = get_context_ptr_type();           /* context */
   arg_types[1] = get_vertex_header_ptr_type();     /* vertex_header */
   arg_types[2] = get_buffer_ptr_type();            /* vbuffers */
   arg_types[3] =
   arg_types[4] =
   arg_types[5] = LLVMInt32TypeInContext(LC);       /* stride */
   arg_types[6] = get_vb_ptr_type();                /* pipe_vertex_buffer's */
   arg_types[7] = LLVMInt32TypeInContext(LC);       /* instance_id */

   func_type = LLVMFunctionType(LLVMVoidTypeInContext(LC),
                                arg_types, Elements(arg_types), 0);

   variant->function = LLVMAddFunction(gallivm.module, "draw_llvm_shader", func_type);
   LLVMSetFunctionCallConv(variant->function, LLVMCCallConv);
   for(i = 0; i < Elements(arg_types); ++i)
      if(LLVMGetTypeKind(arg_types[i]) == LLVMPointerTypeKind)
         LLVMAddAttribute(LLVMGetParam(variant->function, i), LLVMNoAliasAttribute);

   context_ptr  = LLVMGetParam(variant->function, 0);
   io_ptr       = LLVMGetParam(variant->function, 1);
   vbuffers_ptr = LLVMGetParam(variant->function, 2);
   start        = LLVMGetParam(variant->function, 3);
   count        = LLVMGetParam(variant->function, 4);
   stride       = LLVMGetParam(variant->function, 5);
   vb_ptr       = LLVMGetParam(variant->function, 6);
   instance_id  = LLVMGetParam(variant->function, 7);

   lp_build_name(context_ptr, "context");
   lp_build_name(io_ptr, "io");
   lp_build_name(vbuffers_ptr, "vbuffers");
   lp_build_name(start, "start");
   lp_build_name(count, "count");
   lp_build_name(stride, "stride");
   lp_build_name(vb_ptr, "vb");
   lp_build_name(instance_id, "instance_id");

   /*
    * Function body
    */

   block = LLVMAppendBasicBlockInContext(LC, variant->function, "entry");
   builder = LLVMCreateBuilderInContext(LC);
   LLVMPositionBuilderAtEnd(builder, block);

   lp_build_context_init(&bld, builder, lp_type_int(32));

   end = lp_build_add(&bld, start, count);

   step = lp_build_const_int32(max_vertices);

   /* code generated texture sampling */
   sampler = draw_llvm_sampler_soa_create(
      draw_llvm_variant_key_samplers(&variant->key),
      context_ptr);

#if DEBUG_STORE
   lp_build_printf(builder, "start = %d, end = %d, step = %d\n",
                   start, end, step);
#endif
   lp_build_loop_begin(builder, start, &lp_loop);
   {
      LLVMValueRef inputs[PIPE_MAX_SHADER_INPUTS][NUM_CHANNELS];
      LLVMValueRef aos_attribs[PIPE_MAX_SHADER_INPUTS][NUM_CHANNELS] = { { 0 } };
      LLVMValueRef io;
      const LLVMValueRef (*ptr_aos)[NUM_CHANNELS];

      io_itr = LLVMBuildSub(builder, lp_loop.counter, start, "");
      io = LLVMBuildGEP(builder, io_ptr, &io_itr, 1, "");
#if DEBUG_STORE
      lp_build_printf(builder, " --- io %d = %p, loop counter %d\n",
                      io_itr, io, lp_loop.counter);
#endif
      for (i = 0; i < NUM_CHANNELS; ++i) {
         LLVMValueRef true_index = LLVMBuildAdd(
            builder,
            lp_loop.counter,
            lp_build_const_int32(i), "");
         for (j = 0; j < draw->pt.nr_vertex_elements; ++j) {
            struct pipe_vertex_element *velem = &draw->pt.vertex_element[j];
            LLVMValueRef vb_index = lp_build_const_int32(velem->vertex_buffer_index);
            LLVMValueRef vb = LLVMBuildGEP(builder, vb_ptr,
                                           &vb_index, 1, "");
            generate_fetch(builder, vbuffers_ptr,
                           &aos_attribs[j][i], velem, vb, true_index,
                           instance_id);
         }
      }
      convert_to_soa(builder, aos_attribs, inputs,
                     draw->pt.nr_vertex_elements);

      ptr_aos = (const LLVMValueRef (*)[NUM_CHANNELS]) inputs;
      generate_vs(llvm,
                  builder,
                  outputs,
                  ptr_aos,
                  context_ptr,
                  sampler);

      convert_to_aos(builder, io, outputs,
                     draw->vs.vertex_shader->info.num_outputs,
                     max_vertices);
   }
   lp_build_loop_end_cond(builder, end, step, LLVMIntUGE, &lp_loop);

   sampler->destroy(sampler);

#ifdef PIPE_ARCH_X86
   /* Avoid corrupting the FPU stack on 32bit OSes. */
   lp_build_intrinsic(builder, "llvm.x86.mmx.emms", LLVMVoidType(), NULL, 0);
#endif

   LLVMBuildRetVoid(builder);

   LLVMDisposeBuilder(builder);

   /*
    * Translate the LLVM IR into machine code.
    */
#ifdef DEBUG
   if(LLVMVerifyFunction(variant->function, LLVMPrintMessageAction)) {
      lp_debug_dump_value(variant->function);
      assert(0);
   }
#endif

   LLVMRunFunctionPassManager(gallivm.passmgr, variant->function);

   if (gallivm_debug & GALLIVM_DEBUG_IR) {
      lp_debug_dump_value(variant->function);
      debug_printf("\n");
   }

   code = LLVMGetPointerToGlobal(gallivm.engine, variant->function);
   variant->jit_func = (draw_jit_vert_func)pointer_to_func(code);

   if (gallivm_debug & GALLIVM_DEBUG_ASM) {
      lp_disassemble(code);
   }
   lp_func_delete_body(variant->function);
}


static void
draw_llvm_generate_elts(struct draw_llvm *llvm, struct draw_llvm_variant *variant)
{
   LLVMTypeRef arg_types[8];
   LLVMTypeRef func_type;
   LLVMValueRef context_ptr;
   LLVMBasicBlockRef block;
   LLVMBuilderRef builder;
   LLVMValueRef fetch_elts, fetch_count, stride, step, io_itr;
   LLVMValueRef io_ptr, vbuffers_ptr, vb_ptr;
   LLVMValueRef instance_id;
   struct draw_context *draw = llvm->draw;
   unsigned i, j;
   struct lp_build_context bld;
   struct lp_build_loop_state lp_loop;
   const int max_vertices = 4;
   LLVMValueRef outputs[PIPE_MAX_SHADER_OUTPUTS][NUM_CHANNELS];
   LLVMValueRef fetch_max;
   void *code;
   struct lp_build_sampler_soa *sampler = 0;

   arg_types[0] = get_context_ptr_type();               /* context */
   arg_types[1] = get_vertex_header_ptr_type();         /* vertex_header */
   arg_types[2] = get_buffer_ptr_type();                /* vbuffers */
   arg_types[3] = LLVMPointerType(LLVMInt32TypeInContext(LC), 0);  /* fetch_elts * */
   arg_types[4] = LLVMInt32TypeInContext(LC);           /* fetch_count */
   arg_types[5] = LLVMInt32TypeInContext(LC);           /* stride */
   arg_types[6] = get_vb_ptr_type();                    /* pipe_vertex_buffer's */
   arg_types[7] = LLVMInt32TypeInContext(LC);           /* instance_id */

   func_type = LLVMFunctionType(LLVMVoidTypeInContext(LC),
                                arg_types, Elements(arg_types), 0);

   variant->function_elts = LLVMAddFunction(gallivm.module, "draw_llvm_shader_elts",
                                            func_type);
   LLVMSetFunctionCallConv(variant->function_elts, LLVMCCallConv);
   for(i = 0; i < Elements(arg_types); ++i)
      if(LLVMGetTypeKind(arg_types[i]) == LLVMPointerTypeKind)
         LLVMAddAttribute(LLVMGetParam(variant->function_elts, i),
                          LLVMNoAliasAttribute);

   context_ptr  = LLVMGetParam(variant->function_elts, 0);
   io_ptr       = LLVMGetParam(variant->function_elts, 1);
   vbuffers_ptr = LLVMGetParam(variant->function_elts, 2);
   fetch_elts   = LLVMGetParam(variant->function_elts, 3);
   fetch_count  = LLVMGetParam(variant->function_elts, 4);
   stride       = LLVMGetParam(variant->function_elts, 5);
   vb_ptr       = LLVMGetParam(variant->function_elts, 6);
   instance_id  = LLVMGetParam(variant->function_elts, 7);

   lp_build_name(context_ptr, "context");
   lp_build_name(io_ptr, "io");
   lp_build_name(vbuffers_ptr, "vbuffers");
   lp_build_name(fetch_elts, "fetch_elts");
   lp_build_name(fetch_count, "fetch_count");
   lp_build_name(stride, "stride");
   lp_build_name(vb_ptr, "vb");
   lp_build_name(instance_id, "instance_id");

   /*
    * Function body
    */

   block = LLVMAppendBasicBlockInContext(LC, variant->function_elts, "entry");
   builder = LLVMCreateBuilderInContext(LC);
   LLVMPositionBuilderAtEnd(builder, block);

   lp_build_context_init(&bld, builder, lp_type_int(32));

   step = lp_build_const_int32(max_vertices);

   /* code generated texture sampling */
   sampler = draw_llvm_sampler_soa_create(
      draw_llvm_variant_key_samplers(&variant->key),
      context_ptr);

   fetch_max = LLVMBuildSub(builder, fetch_count,
                            lp_build_const_int32(1),
                            "fetch_max");

   lp_build_loop_begin(builder, lp_build_const_int32(0), &lp_loop);
   {
      LLVMValueRef inputs[PIPE_MAX_SHADER_INPUTS][NUM_CHANNELS];
      LLVMValueRef aos_attribs[PIPE_MAX_SHADER_INPUTS][NUM_CHANNELS] = { { 0 } };
      LLVMValueRef io;
      const LLVMValueRef (*ptr_aos)[NUM_CHANNELS];

      io_itr = lp_loop.counter;
      io = LLVMBuildGEP(builder, io_ptr, &io_itr, 1, "");
#if DEBUG_STORE
      lp_build_printf(builder, " --- io %d = %p, loop counter %d\n",
                      io_itr, io, lp_loop.counter);
#endif
      for (i = 0; i < NUM_CHANNELS; ++i) {
         LLVMValueRef true_index = LLVMBuildAdd(
            builder,
            lp_loop.counter,
            lp_build_const_int32(i), "");
         LLVMValueRef fetch_ptr;

         /* make sure we're not out of bounds which can happen
          * if fetch_count % 4 != 0, because on the last iteration
          * a few of the 4 vertex fetches will be out of bounds */
         true_index = lp_build_min(&bld, true_index, fetch_max);

         fetch_ptr = LLVMBuildGEP(builder, fetch_elts,
                                  &true_index, 1, "");
         true_index = LLVMBuildLoad(builder, fetch_ptr, "fetch_elt");
         for (j = 0; j < draw->pt.nr_vertex_elements; ++j) {
            struct pipe_vertex_element *velem = &draw->pt.vertex_element[j];
            LLVMValueRef vb_index = lp_build_const_int32(velem->vertex_buffer_index);
            LLVMValueRef vb = LLVMBuildGEP(builder, vb_ptr,
                                           &vb_index, 1, "");
            generate_fetch(builder, vbuffers_ptr,
                           &aos_attribs[j][i], velem, vb, true_index,
                           instance_id);
         }
      }
      convert_to_soa(builder, aos_attribs, inputs,
                     draw->pt.nr_vertex_elements);

      ptr_aos = (const LLVMValueRef (*)[NUM_CHANNELS]) inputs;
      generate_vs(llvm,
                  builder,
                  outputs,
                  ptr_aos,
                  context_ptr,
                  sampler);

      convert_to_aos(builder, io, outputs,
                     draw->vs.vertex_shader->info.num_outputs,
                     max_vertices);
   }
   lp_build_loop_end_cond(builder, fetch_count, step, LLVMIntUGE, &lp_loop);

   sampler->destroy(sampler);

#ifdef PIPE_ARCH_X86
   /* Avoid corrupting the FPU stack on 32bit OSes. */
   lp_build_intrinsic(builder, "llvm.x86.mmx.emms", LLVMVoidType(), NULL, 0);
#endif

   LLVMBuildRetVoid(builder);

   LLVMDisposeBuilder(builder);

   /*
    * Translate the LLVM IR into machine code.
    */
#ifdef DEBUG
   if(LLVMVerifyFunction(variant->function_elts, LLVMPrintMessageAction)) {
      lp_debug_dump_value(variant->function_elts);
      assert(0);
   }
#endif

   LLVMRunFunctionPassManager(gallivm.passmgr, variant->function_elts);

   if (gallivm_debug & GALLIVM_DEBUG_IR) {
      lp_debug_dump_value(variant->function_elts);
      debug_printf("\n");
   }

   code = LLVMGetPointerToGlobal(gallivm.engine, variant->function_elts);
   variant->jit_func_elts = (draw_jit_vert_func_elts)pointer_to_func(code);

   if (gallivm_debug & GALLIVM_DEBUG_ASM) {
      lp_disassemble(code);
   }
   lp_func_delete_body(variant->function_elts);
}


struct draw_llvm_variant_key *
draw_llvm_make_variant_key(struct draw_llvm *llvm, char *store)
{
   unsigned i;
   struct draw_llvm_variant_key *key;
   struct lp_sampler_static_state *sampler;

   key = (struct draw_llvm_variant_key *)store;

   /* Presumably all variants of the shader should have the same
    * number of vertex elements - ie the number of shader inputs.
    */
   key->nr_vertex_elements = llvm->draw->pt.nr_vertex_elements;

   /* All variants of this shader will have the same value for
    * nr_samplers.  Not yet trying to compact away holes in the
    * sampler array.
    */
   key->nr_samplers = llvm->draw->vs.vertex_shader->info.file_max[TGSI_FILE_SAMPLER] + 1;

   sampler = draw_llvm_variant_key_samplers(key);

   memcpy(key->vertex_element,
          llvm->draw->pt.vertex_element,
          sizeof(struct pipe_vertex_element) * key->nr_vertex_elements);
   
   memset(sampler, 0, key->nr_samplers * sizeof *sampler);

   for (i = 0 ; i < key->nr_samplers; i++) {
      lp_sampler_static_state(&sampler[i],
			      llvm->draw->sampler_views[i],
			      llvm->draw->samplers[i]);
   }

   return key;
}

void
draw_llvm_set_mapped_texture(struct draw_context *draw,
                             unsigned sampler_idx,
                             uint32_t width, uint32_t height, uint32_t depth,
                             uint32_t last_level,
                             uint32_t row_stride[DRAW_MAX_TEXTURE_LEVELS],
                             uint32_t img_stride[DRAW_MAX_TEXTURE_LEVELS],
                             const void *data[DRAW_MAX_TEXTURE_LEVELS])
{
   unsigned j;
   struct draw_jit_texture *jit_tex;

   assert(sampler_idx < PIPE_MAX_VERTEX_SAMPLERS);


   jit_tex = &draw->llvm->jit_context.textures[sampler_idx];

   jit_tex->width = width;
   jit_tex->height = height;
   jit_tex->depth = depth;
   jit_tex->last_level = last_level;

   for (j = 0; j <= last_level; j++) {
      jit_tex->data[j] = data[j];
      jit_tex->row_stride[j] = row_stride[j];
      jit_tex->img_stride[j] = img_stride[j];
   }
}

void
draw_llvm_destroy_variant(struct draw_llvm_variant *variant)
{
   if (variant->function_elts) {
      if (variant->function_elts)
         LLVMFreeMachineCodeForFunction(gallivm.engine,
                                        variant->function_elts);
      LLVMDeleteFunction(variant->function_elts);
   }

   if (variant->function) {
      if (variant->function)
         LLVMFreeMachineCodeForFunction(gallivm.engine,
                                        variant->function);
      LLVMDeleteFunction(variant->function);
   }

   remove_from_list(&variant->list_item_local);
   variant->shader->variants_cached--;

   remove_from_list(&variant->list_item_global);
   draw_llvm_global.nr_variants--;

   FREE(variant);
}
