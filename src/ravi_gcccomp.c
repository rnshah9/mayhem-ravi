/******************************************************************************
* Copyright (C) 2015 Dibyendu Majumdar
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#include <ravi_gccjit.h>

// implements EQ, LE and LT - by using the supplied lua function to call.
void ravi_emit_EQ_LE_LT(ravi_function_def_t *def, int A, int B, int C, int j,
                        int jA, gcc_jit_function *callee, const char *opname,
                        int pc) {
  //  case OP_EQ: {
  //    TValue *rb = RKB(i);
  //    TValue *rc = RKC(i);
  //    Protect(
  //      if (cast_int(luaV_equalobj(L, rb, rc)) != GETARG_A(i))
  //        ci->u.l.savedpc++;
  //      else
  //        donextjump(ci);
  //    )
  //  } break;

  // Load pointer to base
  ravi_emit_load_base(def);

  // Get pointer to register B
  gcc_jit_rvalue *lhs_ptr = ravi_emit_get_register_or_constant(def, B);
  // Get pointer to register C
  gcc_jit_rvalue *rhs_ptr = ravi_emit_get_register_or_constant(def, C);

  // Call luaV_equalobj with register B and C
  gcc_jit_rvalue *result = ravi_function_call3_rvalue(
      def, callee, gcc_jit_param_as_rvalue(def->L), lhs_ptr, rhs_ptr);
  // Test if result is equal to operand A
  gcc_jit_rvalue *A_const = gcc_jit_context_new_rvalue_from_int(
      def->function_context, def->ravi->types->C_intT, A);
  gcc_jit_rvalue *result_eq_A = gcc_jit_context_new_comparison(
      def->function_context, NULL, GCC_JIT_COMPARISON_EQ, result, A_const);
  // If result == A then we need to execute the next statement which is a jump
  char temp[80];
  snprintf(temp, sizeof temp, "%s_then", opname);
  gcc_jit_block *then_block =
      gcc_jit_function_new_block(def->jit_function, unique_name(def, temp, pc));

  snprintf(temp, sizeof temp, "%s_else", opname);
  gcc_jit_block *else_block =
      gcc_jit_function_new_block(def->jit_function, unique_name(def, temp, pc));
  ravi_emit_conditional_branch(def, result_eq_A, then_block, else_block);
  ravi_set_current_block(def, then_block);

  // if (a > 0) luaF_close(L, ci->u.l.base + a - 1);
  if (jA > 0) {
    // jA is the A operand of the Jump instruction

    // Reload pointer to base as the call to luaV_equalobj() may
    // have invoked a Lua function and as a result the stack may have
    // been reallocated - so the previous base pointer could be stale
    ravi_emit_load_base(def);

    // base + a - 1
    gcc_jit_rvalue *val = ravi_emit_get_register(def, jA - 1);

    // Call luaF_close
    gcc_jit_block_add_eval(
        def->current_block, NULL,
        ravi_function_call2_rvalue(def, def->ravi->types->luaF_closeT,
                                   gcc_jit_param_as_rvalue(def->L), val));
  }
  // Do the jump
  ravi_emit_branch(def, def->jmp_targets[j]->jmp);
  // Add the else block and make it current so that the next instruction flows
  // here
  ravi_set_current_block(def, else_block);
}


gcc_jit_rvalue *ravi_emit_boolean_testfalse(ravi_function_def_t *def,
                                            gcc_jit_rvalue *reg,
                                            bool negate) {
  // (isnil() || isbool() && b == 0)

  gcc_jit_lvalue *var = gcc_jit_function_new_local(def->jit_function, NULL, def->ravi->types->C_intT,
                                                   unique_name(def, "bvalue", 0));
  gcc_jit_lvalue *type = ravi_emit_load_type(def, reg);

  // Test if type == LUA_TNIL (0)
  gcc_jit_rvalue *isnil = ravi_emit_is_value_of_type(def, gcc_jit_lvalue_as_rvalue(type), LUA__TNIL);
  gcc_jit_block *then_block =
          gcc_jit_function_new_block(def->jit_function, unique_name(def, "if.nil", 0));
  gcc_jit_block *else_block =
          gcc_jit_function_new_block(def->jit_function, unique_name(def, "not.nil", 0));
  gcc_jit_block *end_block =
          gcc_jit_function_new_block(def->jit_function, unique_name(def, "end", 0));

  ravi_emit_conditional_branch(def, isnil, then_block, else_block);
  ravi_set_current_block(def, then_block);

  gcc_jit_block_add_assignment(def->current_block, NULL, var, isnil);
  ravi_emit_branch(def, end_block);

  ravi_set_current_block(def, else_block);
  // value is not nil
  // so check if bool and b == 0

  // Test if type == LUA_TBOOLEAN
  gcc_jit_rvalue *isbool =
          ravi_emit_is_value_of_type(def, gcc_jit_lvalue_as_rvalue(type), LUA__TBOOLEAN);
  // Test if bool value == 0
  gcc_jit_lvalue *bool_value = ravi_emit_load_reg_b(def, reg);
  gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(def->function_context, def->ravi->types->C_intT, 0);
  gcc_jit_rvalue *boolzero =
          gcc_jit_context_new_comparison(def->function_context, NULL, GCC_JIT_COMPARISON_EQ,
                                         gcc_jit_lvalue_as_rvalue(bool_value), zero);
  // Test type == LUA_TBOOLEAN && bool value == 0
  gcc_jit_rvalue *andvalue = gcc_jit_context_new_binary_op(def->function_context, NULL,
                                                           GCC_JIT_BINARY_OP_LOGICAL_AND, def->ravi->types->C_intT,
                                                           isbool, boolzero);

  gcc_jit_block_add_assignment(def->current_block, NULL, var, andvalue);
  ravi_emit_branch(def, end_block);

  ravi_set_current_block(def, end_block);

  gcc_jit_rvalue *result = NULL;
  if (negate) {
    result = gcc_jit_context_new_unary_op(def->function_context, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE,
                                          def->ravi->types->C_intT, gcc_jit_lvalue_as_rvalue(var));
  } else {
    result = gcc_jit_lvalue_as_rvalue(var);
  }

  return result;
}

