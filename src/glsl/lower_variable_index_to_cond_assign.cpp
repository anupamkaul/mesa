/*
 * Copyright © 2010 Luca Barbieri
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file lower_variable_index_to_cond_assign.cpp
 *
 * Turns non-constant indexing into array types to a series of
 * conditional moves of each element into a temporary.
 *
 * Pre-DX10 GPUs often don't have a native way to do this operation,
 * and this works around that.
 */

#include "ir.h"
#include "ir_rvalue_visitor.h"
#include "ir_optimization.h"
#include "glsl_types.h"
#include "main/macros.h"

struct assignment_generator
{
   ir_instruction* base_ir;
   ir_rvalue* array;
   bool is_write;
   ir_variable* var;

   assignment_generator()
   {
   }

   exec_list generate(unsigned i, ir_rvalue* condition) const
   {
      /* Just clone the rest of the deref chain when trying to get at the
       * underlying variable.
       * XXX: what if it has side effects?!? ir_vector_index_to_cond_assign does this too!
       */
      void *mem_ctx = talloc_parent(base_ir);
      ir_rvalue* element = new(base_ir) ir_dereference_array(this->array->clone(mem_ctx, NULL), new(base_ir) ir_constant(i));
      ir_rvalue* variable = new(base_ir) ir_dereference_variable(this->var);
      exec_list list;
      ir_assignment* assignment;
      if(is_write)
         assignment = new(base_ir) ir_assignment(element, variable, condition);
      else
         assignment = new(base_ir) ir_assignment(variable, element, condition);
      list.push_tail(assignment);
      return list;
   }
};

struct switch_generator
{
   /* make TFunction a template parameter if you need to use other generators */
   typedef assignment_generator TFunction;
   const TFunction& generator;

   ir_variable* index;
   unsigned linear_sequence_max_length;
   unsigned condition_components;

   switch_generator(const TFunction& generator)
   : generator(generator)
   {}

   exec_list linear_sequence(unsigned begin, unsigned end)
   {
      exec_list list, toappend;
      if(begin == end)
         return list;

      /* do the first one unconditionally
       * FINISHME: may not want this in some cases
       */

      toappend = this->generator.generate(begin, 0);
      list.append_list(&toappend);

      for (unsigned i = begin + 1; i < end; i += 4) {
         int comps = MIN2(condition_components, end - i);

         ir_rvalue* broadcast_index = new(index) ir_dereference_variable(index);
         if(comps)
            broadcast_index = new(index) ir_swizzle(broadcast_index, 0, 1, 2, 3, comps);

         ir_constant* test_indices;
         ir_constant_data test_indices_data;
         memset(&test_indices_data, 0, sizeof(test_indices_data));
         test_indices_data.i[0] = i;
         test_indices_data.i[1] = i + 1;
         test_indices_data.i[2] = i + 2;
         test_indices_data.i[3] = i + 3;
         test_indices = new(index) ir_constant(broadcast_index->type, &test_indices_data);

         ir_rvalue* condition_val = new(index) ir_expression(ir_binop_equal,
                                                &glsl_type::bool_type[comps - 1],
                                                broadcast_index,
                                                test_indices);
         ir_variable* condition = new(index) ir_variable(&glsl_type::bool_type[comps], "dereference_array_condition", ir_var_temporary);
         list.push_tail(condition);
         list.push_tail(new (index) ir_assignment(new(index) ir_dereference_variable(condition), condition_val, 0));

         if(comps == 1)
         {
            toappend = this->generator.generate(i, new(index) ir_dereference_variable(condition));
            list.append_list(&toappend);
         }
         else
         {
            for(int j = 0; j < comps; ++j)
            {
               toappend = this->generator.generate(i + j, new(index) ir_swizzle(new(index) ir_dereference_variable(condition), j, 0, 0, 0, 1));
               list.append_list(&toappend);
            }
         }
      }

      return list;
   }

   exec_list bisect(unsigned begin, unsigned end)
   {
      unsigned middle = (begin + end) >> 1;
      ir_constant* middle_c;

      if(index->type->base_type == GLSL_TYPE_UINT)
         middle_c = new(index) ir_constant((unsigned)middle);
      else if(index->type->base_type == GLSL_TYPE_UINT)
         middle_c = new(index) ir_constant((int)middle);
      else
         assert(0);

      ir_expression* less = new(index) ir_expression(
            ir_binop_less, glsl_type::bool_type,
            new(index) ir_dereference_variable(this->index),
            middle_c);

      ir_if* if_less = new(index) ir_if(less);
      exec_list toappend;

      toappend = generate(begin, middle);
      if_less->then_instructions.append_list(&toappend);

      toappend = generate(middle, end);
      if_less->else_instructions.append_list(&toappend);

      exec_list list;
      list.push_tail(if_less);
      return list;
   }

   exec_list generate(unsigned begin, unsigned end)
   {
      unsigned length = end - begin;
      if(length <= this->linear_sequence_max_length)
         return linear_sequence(begin, end);
      else
         return bisect(begin, end);
   }
};

/**
 * Visitor class for replacing expressions with ir_constant values.
 */

class ir_array_index_to_cond_assign_visitor : public ir_rvalue_visitor {
public:
   ir_array_index_to_cond_assign_visitor()
   {
      progress = false;
   }

   bool progress;

   ir_variable* convert_dereference_array(ir_dereference_array *orig_deref, ir_rvalue* value)
   {
      ir_assignment *assign;

      unsigned length;
      if (orig_deref->array->type->is_array())
         length = orig_deref->array->type->length;
      else if (orig_deref->array->type->is_matrix())
         length = orig_deref->array->type->matrix_columns;
      else
         assert(0);

      ir_variable* var = new(base_ir) ir_variable(orig_deref->type, "dereference_array_value", ir_var_temporary);
      base_ir->insert_before(var);

      if(value)
         base_ir->insert_before(new(base_ir) ir_assignment(new(base_ir) ir_dereference_variable(var), value, NULL));

      /* Store the index to a temporary to avoid reusing its tree. */
      ir_variable *index = new(base_ir) ir_variable(orig_deref->array_index->type, "dereference_array_index", ir_var_temporary);
      base_ir->insert_before(index);
      assign = new(base_ir) ir_assignment(new(base_ir) ir_dereference_variable(index), orig_deref->array_index, NULL);
      base_ir->insert_before(assign);

      assignment_generator ag;
      ag.array = orig_deref->array;
      ag.base_ir = base_ir;
      ag.var = var;
      ag.is_write = !!value;

      switch_generator sg(ag);
      sg.index = index;
      sg.linear_sequence_max_length = 4;
      sg.condition_components = 4;

     exec_list list = sg.generate(0, length);
     base_ir->insert_before(&list);

      return var;
   }

   virtual void handle_rvalue(ir_rvalue **pir)
   {
      if(!*pir)
         return;

      ir_dereference_array* orig_deref = (*pir)->as_dereference_array();
      if (orig_deref && !orig_deref->array_index->as_constant()
            && (orig_deref->array->type->is_array() || orig_deref->array->type->is_matrix()))
      {
         ir_variable* var = convert_dereference_array(orig_deref, 0);
         assert(var);
         *pir = new(base_ir) ir_dereference_variable(var);
         this->progress = true;
      }
   }

   ir_visitor_status
   visit_leave(ir_assignment *ir)
   {
      ir_rvalue_visitor::visit_leave(ir);

      ir_dereference_array *orig_deref = ir->lhs->as_dereference_array();

      if (orig_deref && !orig_deref->array_index->as_constant()
            && (orig_deref->array->type->is_array() || orig_deref->array->type->is_matrix()))
      {
         convert_dereference_array(orig_deref, ir->rhs);
         ir->remove();
         this->progress = true;
      }

      return visit_continue;
   }
};

bool
lower_variable_index_to_cond_assign(exec_list *instructions)
{
   ir_array_index_to_cond_assign_visitor v;

   visit_list_elements(&v, instructions);

   return v.progress;
}
