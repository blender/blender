/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/math_util.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_math(ccl_private float *stack,
                                       const uint type,
                                       const uint inputs_stack_offsets,
                                       const uint result_stack_offset)
{
  uint a_stack_offset;
  uint b_stack_offset;
  uint c_stack_offset;
  svm_unpack_node_uchar3(inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &c_stack_offset);

  const float a = stack_load_float(stack, a_stack_offset);
  const float b = stack_load_float(stack, b_stack_offset);
  const float c = stack_load_float(stack, c_stack_offset);
  const float result = svm_math((NodeMathType)type, a, b, c);

  stack_store_float(stack, result_stack_offset, result);
}

template<typename Float3Type>
ccl_device_noinline int svm_node_vector_math(KernelGlobals kg,
                                             ccl_private float *stack,
                                             const uint type,
                                             const uint inputs_stack_offsets,
                                             const uint outputs_stack_offsets,
                                             int offset)
{
  using FloatType = dual_scalar_t<Float3Type>;

  uint value_stack_offset;
  uint vector_stack_offset;
  uint a_stack_offset;
  uint b_stack_offset;
  uint param1_stack_offset;
  svm_unpack_node_uchar3(
      inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &param1_stack_offset);
  svm_unpack_node_uchar2(outputs_stack_offsets, &value_stack_offset, &vector_stack_offset);

  const Float3Type a = stack_load<Float3Type>(stack, a_stack_offset);
  const Float3Type b = stack_load<Float3Type>(stack, b_stack_offset);
  Float3Type c = make_zero<Float3Type>();
  const FloatType param1 = stack_load<FloatType>(stack, param1_stack_offset);

  /* 3 Vector Operators */
  if (type == NODE_VECTOR_MATH_WRAP || type == NODE_VECTOR_MATH_FACEFORWARD ||
      type == NODE_VECTOR_MATH_MULTIPLY_ADD)
  {
    const uint4 extra_node = read_node(kg, &offset);
    c = stack_load<Float3Type>(stack, extra_node.x);
  }

  FloatType value = make_zero<FloatType>();
  Float3Type vector = make_zero<Float3Type>();
  svm_vector_math(&value, &vector, (NodeVectorMathType)type, a, b, c, param1);

  if (stack_valid(value_stack_offset)) {
    stack_store(stack, value_stack_offset, value);
  }
  if (stack_valid(vector_stack_offset)) {
    stack_store(stack, vector_stack_offset, vector);
  }

  return offset;
}

CCL_NAMESPACE_END
