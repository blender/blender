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
  uint a_stack_offset, b_stack_offset;
  decode_node_uchar4(inputs_stack_offsets, &a_stack_offset, &b_stack_offset, NULL, NULL);

  float a = stack_load_float(stack, a_stack_offset);
  float b = stack_load_float(stack, b_stack_offset);
  float result = svm_math((NodeMathType)type, a, b);

  stack_store_float(stack, result_stack_offset, result);
}

ccl_device void svm_node_vector_math(KernelGlobals *kg,
                                     ShaderData *sd,
                                     float *stack,
                                     uint itype,
                                     uint v1_offset,
                                     uint v2_offset,
                                     int *offset)
{
  NodeVectorMath type = (NodeVectorMath)itype;
  float3 v1 = stack_load_float3(stack, v1_offset);
  float3 v2 = stack_load_float3(stack, v2_offset);
  float f;
  float3 v;

  svm_vector_math(&f, &v, type, v1, v2);

  uint4 node1 = read_node(kg, offset);

  if (stack_valid(node1.y))
    stack_store_float(stack, node1.y, f);
  if (stack_valid(node1.z))
    stack_store_float3(stack, node1.z, v);
}

CCL_NAMESPACE_END
