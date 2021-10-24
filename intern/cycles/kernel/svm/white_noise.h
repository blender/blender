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

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_tex_white_noise(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint dimensions,
                                                  uint inputs_stack_offsets,
                                                  uint ouptuts_stack_offsets)
{
  uint vector_stack_offset, w_stack_offset, value_stack_offset, color_stack_offset;
  svm_unpack_node_uchar2(inputs_stack_offsets, &vector_stack_offset, &w_stack_offset);
  svm_unpack_node_uchar2(ouptuts_stack_offsets, &value_stack_offset, &color_stack_offset);

  float3 vector = stack_load_float3(stack, vector_stack_offset);
  float w = stack_load_float(stack, w_stack_offset);

  if (stack_valid(color_stack_offset)) {
    float3 color;
    switch (dimensions) {
      case 1:
        color = hash_float_to_float3(w);
        break;
      case 2:
        color = hash_float2_to_float3(make_float2(vector.x, vector.y));
        break;
      case 3:
        color = hash_float3_to_float3(vector);
        break;
      case 4:
        color = hash_float4_to_float3(make_float4(vector.x, vector.y, vector.z, w));
        break;
      default:
        color = make_float3(1.0f, 0.0f, 1.0f);
        kernel_assert(0);
        break;
    }
    stack_store_float3(stack, color_stack_offset, color);
  }

  if (stack_valid(value_stack_offset)) {
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
}

CCL_NAMESPACE_END
