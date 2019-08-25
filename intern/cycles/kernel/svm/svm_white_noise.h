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

ccl_device void svm_node_tex_white_noise(KernelGlobals *kg,
                                         ShaderData *sd,
                                         float *stack,
                                         uint dimensions,
                                         uint inputs_stack_offsets,
                                         uint value_stack_offset,
                                         int *offset)
{
  uint vector_stack_offset, w_stack_offset;
  decode_node_uchar4(inputs_stack_offsets, &vector_stack_offset, &w_stack_offset, NULL, NULL);

  float3 vector = stack_load_float3(stack, vector_stack_offset);
  float w = stack_load_float(stack, w_stack_offset);

  float value;
  switch (dimensions) {
    case 1:
      value = hash_float_to_float(w);
      break;
    case 2:
      value = hash_float2_to_float(make_float2(vector.x, vector.y));
      break;
    case 3:
      value = hash_float3_to_float(vector);
      break;
    case 4:
      value = hash_float4_to_float(make_float4(vector.x, vector.y, vector.z, w));
      break;
    default:
      value = 0.0f;
      kernel_assert(0);
      break;
  }
  stack_store_float(stack, value_stack_offset, value);
}

CCL_NAMESPACE_END
