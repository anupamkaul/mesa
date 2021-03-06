/**************************************************************************
 *
 * Copyright 2012 VMware, Inc.
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

#include "lp_bld_const.h"
#include "lp_bld_struct.h"
#include "lp_bld_format.h"
#include "lp_bld_debug.h"
#include "lp_bld_type.h"
#include "lp_bld_conv.h"
#include "lp_bld_pack.h"
#include "lp_bld_intr.h"
#include "lp_bld_gather.h"

#include "util/u_memory.h"
#include "util/u_format.h"
#include "pipe/p_state.h"


#ifdef PIPE_ARCH_BIG_ENDIAN
static LLVMValueRef
lp_build_read_int_bswap(struct gallivm_state *gallivm,
                        LLVMValueRef base_ptr,
                        unsigned src_width,
                        LLVMTypeRef src_type,
                        unsigned i,
                        LLVMTypeRef dst_type)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef index = lp_build_const_int32(gallivm, i);
   LLVMValueRef ptr = LLVMBuildGEP(builder, base_ptr, &index, 1, "");
   LLVMValueRef res = LLVMBuildLoad(builder, ptr, "");
   res = lp_build_bswap(gallivm, res, lp_type_uint(src_width));
   return LLVMBuildBitCast(builder, res, dst_type, "");
}

static LLVMValueRef
lp_build_fetch_read_big_endian(struct gallivm_state *gallivm,
                               struct lp_type src_type,
                               LLVMValueRef base_ptr)
{
   LLVMBuilderRef builder = gallivm->builder;
   unsigned src_width = src_type.width;
   unsigned length = src_type.length;
   LLVMTypeRef src_elem_type = LLVMIntTypeInContext(gallivm->context, src_width);
   LLVMTypeRef dst_elem_type = lp_build_elem_type (gallivm, src_type);
   LLVMTypeRef src_ptr_type = LLVMPointerType(src_elem_type, 0);
   LLVMValueRef res;

   base_ptr = LLVMBuildPointerCast(builder, base_ptr, src_ptr_type, "");
   if (length == 1) {
      /* Scalar */
      res = lp_build_read_int_bswap(gallivm, base_ptr, src_width, src_elem_type,
                                    0, dst_elem_type);
   } else {
      /* Vector */
      LLVMTypeRef dst_vec_type = LLVMVectorType(dst_elem_type, length);
      unsigned i;

      res = LLVMGetUndef(dst_vec_type);
      for (i = 0; i < length; ++i) {
         LLVMValueRef index = lp_build_const_int32(gallivm, i);
         LLVMValueRef elem = lp_build_read_int_bswap(gallivm, base_ptr, src_width,
                                                     src_elem_type, i, dst_elem_type);
         res = LLVMBuildInsertElement(builder, res, elem, index, "");
      }
   }

   return res;
}
#endif

/**
 * @brief lp_build_fetch_rgba_aos_array
 *
 * \param format_desc   describes format of the image we're fetching from
 * \param dst_type      output type
 * \param base_ptr      address of the pixel block (or the texel if uncompressed)
 * \param offset        ptr offset
 */
LLVMValueRef
lp_build_fetch_rgba_aos_array(struct gallivm_state *gallivm,
                              const struct util_format_description *format_desc,
                              struct lp_type dst_type,
                              LLVMValueRef base_ptr,
                              LLVMValueRef offset)
{
   struct lp_build_context bld;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMTypeRef src_vec_type;
   LLVMValueRef ptr, res = NULL;
   struct lp_type src_type;

   lp_type_from_format_desc(&src_type, format_desc);

   assert(src_type.length <= dst_type.length);

   src_vec_type  = lp_build_vec_type(gallivm,  src_type);

   /* Read whole vector from memory, unaligned */
   ptr = LLVMBuildGEP(builder, base_ptr, &offset, 1, "");
#ifdef PIPE_ARCH_BIG_ENDIAN
   res = lp_build_fetch_read_big_endian(gallivm, src_type, ptr);
#else
   ptr = LLVMBuildPointerCast(builder, ptr, LLVMPointerType(src_vec_type, 0), "");
   res = LLVMBuildLoad(builder, ptr, "");
   lp_set_load_alignment(res, src_type.width / 8);
#endif

   /* Truncate doubles to float */
   if (src_type.floating && src_type.width == 64) {
      src_type.width = 32;
      src_vec_type  = lp_build_vec_type(gallivm,  src_type);

      res = LLVMBuildFPTrunc(builder, res, src_vec_type, "");
   }

   /* Expand to correct length */
   if (src_type.length < dst_type.length) {
      res = lp_build_pad_vector(gallivm, res, dst_type.length);
      src_type.length = dst_type.length;
   }

   /* Convert to correct format */
   lp_build_conv(gallivm, src_type, dst_type, &res, 1, &res, 1);

   /* Swizzle it */
   lp_build_context_init(&bld, gallivm, dst_type);
   return lp_build_format_swizzle_aos(format_desc, &bld, res);
}
