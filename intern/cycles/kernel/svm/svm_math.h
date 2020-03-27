/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

ccl_device void svm_node_math(KernelGlobals *kg,
                              ShaderData *sd,
                              float *stack,
                              uint type,
                              uint inputs_stack_offsets,
                              uint result_stack_offset,
                              int *offset)
{
  uint a_stack_offset, b_stack_offset, c_stack_offset;
  svm_unpack_node_uchar3(inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &c_stack_offset);

  float a = stack_load_float(stack, a_stack_offset);
  float b = stack_load_float(stack, b_stack_offset);
  float c = stack_load_float(stack, c_stack_offset);
  float result = svm_math((NodeMathType)type, a, b, c);

  stack_store_float(stack, result_stack_offset, result);
}

ccl_device void svm_node_vector_math(KernelGlobals *kg,
                                     ShaderData *sd,
                                     float *stack,
                                     uint type,
                                     uint inputs_stack_offsets,
                                     uint outputs_stack_offsets,
                                     int *offset)
{
  uint value_stack_offset, vector_stack_offset;
  uint a_stack_offset, b_stack_offset, scale_stack_offset;
  svm_unpack_node_uchar3(
      inputs_stack_offsets, &a_stack_offset, &b_stack_offset, &scale_stack_offset);
  svm_unpack_node_uchar2(outputs_stack_offsets, &value_stack_offset, &vector_stack_offset);

  float3 a = stack_load_float3(stack, a_stack_offset);
  float3 b = stack_load_float3(stack, b_stack_offset);
  float3 c = make_float3(0.0f, 0.0f, 0.0f);
  float scale = stack_load_float(stack, scale_stack_offset);

  float value;
  float3 vector;

  /* 3 Vector Operators */
  if (type == NODE_VECTOR_MATH_WRAP) {
    uint4 extra_node = read_node(kg, offset);
    c = stack_load_float3(stack, extra_node.x);
  }

  svm_vector_math(&value, &vector, (NodeVectorMathType)type, a, b, c, scale);

  if (stack_valid(value_stack_offset))
    stack_store_float(stack, value_stack_offset, value);
  if (stack_valid(vector_stack_offset))
    stack_store_float3(stack, vector_stack_offset, vector);
}

CCL_NAMESPACE_END
