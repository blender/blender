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

ccl_device_noinline int svm_node_vector_math(KernelGlobals kg,
                                             ccl_private float *stack,
                                             const uint type,
                                             const uint inputs_stack_offsets,
                                             const uint outputs_stack_offsets,
                                             int offset,
                                             const bool derivative)
{
  uint value_stack_offset;
  uint vector_stack_offset;
  uint a_stack_offset;
  uint b_stack_offset;
  uint param1_stack_offset;
  svm_unpack_node_uchar3(
      inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &param1_stack_offset);
  svm_unpack_node_uchar2(outputs_stack_offsets, &value_stack_offset, &vector_stack_offset);

  const dual3 a = stack_load_float3(stack, a_stack_offset, derivative);
  const dual3 b = stack_load_float3(stack, b_stack_offset, derivative);
  dual3 c;
  const dual1 param1 = stack_load_float(stack, param1_stack_offset, derivative);

  /* 3 Vector Operators */
  if (type == NODE_VECTOR_MATH_WRAP || type == NODE_VECTOR_MATH_FACEFORWARD ||
      type == NODE_VECTOR_MATH_MULTIPLY_ADD)
  {
    const uint4 extra_node = read_node(kg, &offset);
    c = stack_load_float3(stack, extra_node.x, derivative);
  }

  dual1 value;
  dual3 vector;
  if (derivative) {
    svm_vector_math(&value, &vector, (NodeVectorMathType)type, a, b, c, param1);
  }
  else {
    svm_vector_math(
        &value.val, &vector.val, (NodeVectorMathType)type, a.val, b.val, c.val, param1.val);
  }

  if (stack_valid(value_stack_offset)) {
    stack_store_float(stack, value_stack_offset, value, derivative);
  }
  if (stack_valid(vector_stack_offset)) {
    stack_store_float3(stack, vector_stack_offset, vector, derivative);
  }

  return offset;
}

CCL_NAMESPACE_END
