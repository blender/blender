/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device int svm_boolean_math(const NodeBooleanMathType type, const int a_int, const int b_int)
{
  const bool a = a_int > 0;
  const bool b = b_int > 0;
  bool result = false;

  switch (type) {
    case NODE_BOOLEAN_MATH_AND:
      result = a && b;
      break;
    case NODE_BOOLEAN_MATH_OR:
      result = a || b;
      break;
    case NODE_BOOLEAN_MATH_NOT:
      result = !a;
      break;
    case NODE_BOOLEAN_MATH_NAND:
      result = !(a && b);
      break;
    case NODE_BOOLEAN_MATH_NOR:
      result = !(a || b);
      break;
    case NODE_BOOLEAN_MATH_XNOR:
      result = a == b;
      break;
    case NODE_BOOLEAN_MATH_XOR:
      result = a != b;
      break;
    case NODE_BOOLEAN_MATH_IMPLY:
      result = !a || b;
      break;
    case NODE_BOOLEAN_MATH_NIMPLY:
      result = a && !b;
      break;
  }

  return result ? 1 : 0;
}

ccl_device_noinline void svm_node_boolean_math(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeBooleanMath &ccl_restrict node)
{
  const int a = stack_load(stack, node.value1);
  const int b = stack_load(stack, node.value2);
  const int result = svm_boolean_math(node.math_type, a, b);

  if (stack_valid(node.result_offset)) {
    stack_store_int(stack, node.result_offset, result);
  }
}

CCL_NAMESPACE_END
