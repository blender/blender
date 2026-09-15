/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device int svm_integer_math_divide_floor(const int a, const int b)
{
  const int d = a / b;
  const int r = a % b;
  return (r != 0) ? (d - int((a < 0) != (b < 0))) : d;
}

ccl_device int svm_integer_math_gcd(int a, int b)
{
  a = abs(a);
  b = abs(b);
  while (b != 0) {
    const int t = b;
    b = a % b;
    a = t;
  }
  return a;
}

ccl_device int svm_integer_math_power(int base, int exponent)
{
  if (exponent < 0) {
    if (base == 1 || base == -1) {
      return (base < 0 && (exponent & 1) != 0) ? -1 : 1;
    }
    return 0;
  }
  int result = 1;
  while (exponent != 0) {
    if ((exponent & 1) != 0) {
      result *= base;
    }
    exponent >>= 1;
    base *= base;
  }
  return result;
}

ccl_device int svm_integer_math(const NodeIntegerMathType type,
                                const int a,
                                const int b,
                                const int c)
{
  switch (type) {
    case NODE_INTEGER_MATH_ADD:
      return a + b;
    case NODE_INTEGER_MATH_SUBTRACT:
      return a - b;
    case NODE_INTEGER_MATH_MULTIPLY:
      return a * b;
    case NODE_INTEGER_MATH_DIVIDE:
      return (b != 0) ? (a / b) : 0;
    case NODE_INTEGER_MATH_MULTIPLY_ADD:
      return a * b + c;
    case NODE_INTEGER_MATH_POWER:
      return svm_integer_math_power(a, b);
    case NODE_INTEGER_MATH_FLOORED_MODULO:
      return (b != 0) ? (((a % b) + b) % b) : 0;
    case NODE_INTEGER_MATH_ABSOLUTE:
      return abs(a);
    case NODE_INTEGER_MATH_MINIMUM:
      return min(a, b);
    case NODE_INTEGER_MATH_MAXIMUM:
      return max(a, b);
    case NODE_INTEGER_MATH_GCD:
      return svm_integer_math_gcd(a, b);
    case NODE_INTEGER_MATH_LCM: {
      const int gcd = svm_integer_math_gcd(a, b);
      return (gcd != 0) ? abs(a / gcd * b) : 0;
    }
    case NODE_INTEGER_MATH_NEGATE:
      return -a;
    case NODE_INTEGER_MATH_SIGN:
      return int(0 < a) - int(a < 0);
    case NODE_INTEGER_MATH_DIVIDE_FLOOR:
      return (b != 0) ? svm_integer_math_divide_floor(a, b) : 0;
    case NODE_INTEGER_MATH_DIVIDE_CEIL:
      return (b != 0) ? -svm_integer_math_divide_floor(a, -b) : 0;
    case NODE_INTEGER_MATH_DIVIDE_ROUND: {
      const int abs_b = abs(b);
      const int sign_b = int(0 < b) - int(b < 0);
      if (a >= 0) {
        return ((abs_b != 0) ? ((2 * a + abs_b) / (2 * abs_b)) : 0) * sign_b;
      }
      return -((abs_b != 0) ? ((2 * -a + abs_b) / (2 * abs_b)) : 0) * sign_b;
    }
    case NODE_INTEGER_MATH_MODULO:
      return (b != 0) ? (a % b) : 0;
  }

  return 0;
}

ccl_device_noinline void svm_node_integer_math(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeIntegerMath &ccl_restrict node)
{
  const int a = stack_load(stack, node.value1);
  const int b = stack_load(stack, node.value2);
  const int c = stack_load(stack, node.value3);
  const int result = svm_integer_math(node.math_type, a, b, c);

  if (stack_valid(node.result_offset)) {
    stack_store_int(stack, node.result_offset, result);
  }
}

CCL_NAMESPACE_END
