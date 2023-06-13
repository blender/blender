/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_math(KernelGlobals kg,
                                       ccl_private ShaderData *sd,
                                       ccl_private float *stack,
                                       uint type,
                                       uint inputs_stack_offsets,
                                       uint result_stack_offset)
{
  uint a_stack_offset, b_stack_offset, c_stack_offset;
  svm_unpack_node_uchar3(inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &c_stack_offset);

  float a = stack_load_float(stack, a_stack_offset);
  float b = stack_load_float(stack, b_stack_offset);
  float c = stack_load_float(stack, c_stack_offset);
  float result = svm_math((NodeMathType)type, a, b, c);

  stack_store_float(stack, result_stack_offset, result);
}

ccl_device_noinline int svm_node_vector_math(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             uint type,
                                             uint inputs_stack_offsets,
                                             uint outputs_stack_offsets,
                                             int offset)
{
  uint value_stack_offset, vector_stack_offset;
  uint a_stack_offset, b_stack_offset, param1_stack_offset;
  svm_unpack_node_uchar3(
      inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &param1_stack_offset);
  svm_unpack_node_uchar2(outputs_stack_offsets, &value_stack_offset, &vector_stack_offset);

  float3 a = stack_load_float3(stack, a_stack_offset);
  float3 b = stack_load_float3(stack, b_stack_offset);
  float3 c = make_float3(0.0f, 0.0f, 0.0f);
  float param1 = stack_load_float(stack, param1_stack_offset);

  float value;
  float3 vector;

  /* 3 Vector Operators */
  if (type == NODE_VECTOR_MATH_WRAP || type == NODE_VECTOR_MATH_FACEFORWARD ||
      type == NODE_VECTOR_MATH_MULTIPLY_ADD)
  {
    uint4 extra_node = read_node(kg, &offset);
    c = stack_load_float3(stack, extra_node.x);
  }

  svm_vector_math(&value, &vector, (NodeVectorMathType)type, a, b, c, param1);

  if (stack_valid(value_stack_offset))
    stack_store_float(stack, value_stack_offset, value);
  if (stack_valid(vector_stack_offset))
    stack_store_float3(stack, vector_stack_offset, vector);
  return offset;
}

CCL_NAMESPACE_END
