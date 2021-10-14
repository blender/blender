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

/* The following offset functions generate random offsets to be added to texture
 * coordinates to act as a seed since the noise functions don't have seed values.
 * A seed value is needed for generating distortion textures and color outputs.
 * The offset's components are in the range [100, 200], not too high to cause
 * bad precision and not too small to be noticeable. We use float seed because
 * OSL only support float hashes.
 */

ccl_device_inline float random_float_offset(float seed)
{
  return 100.0f + hash_float_to_float(seed) * 100.0f;
}

ccl_device_inline float2 random_float2_offset(float seed)
{
  return make_float2(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f);
}

ccl_device_inline float3 random_float3_offset(float seed)
{
  return make_float3(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f);
}

ccl_device_inline float4 random_float4_offset(float seed)
{
  return make_float4(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 3.0f)) * 100.0f);
}

ccl_device void noise_texture_1d(float co,
                                 float detail,
                                 float roughness,
                                 float distortion,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float p = co;
  if (distortion != 0.0f) {
    p += snoise_1d(p + random_float_offset(0.0f)) * distortion;
  }

  *value = fractal_noise_1d(p, detail, roughness);
  if (color_is_needed) {
    *color = make_float3(*value,
                         fractal_noise_1d(p + random_float_offset(1.0f), detail, roughness),
                         fractal_noise_1d(p + random_float_offset(2.0f), detail, roughness));
  }
}

ccl_device void noise_texture_2d(float2 co,
                                 float detail,
                                 float roughness,
                                 float distortion,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float2 p = co;
  if (distortion != 0.0f) {
    p += make_float2(snoise_2d(p + random_float2_offset(0.0f)) * distortion,
                     snoise_2d(p + random_float2_offset(1.0f)) * distortion);
  }

  *value = fractal_noise_2d(p, detail, roughness);
  if (color_is_needed) {
    *color = make_float3(*value,
                         fractal_noise_2d(p + random_float2_offset(2.0f), detail, roughness),
                         fractal_noise_2d(p + random_float2_offset(3.0f), detail, roughness));
  }
}

ccl_device void noise_texture_3d(float3 co,
                                 float detail,
                                 float roughness,
                                 float distortion,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float3 p = co;
  if (distortion != 0.0f) {
    p += make_float3(snoise_3d(p + random_float3_offset(0.0f)) * distortion,
                     snoise_3d(p + random_float3_offset(1.0f)) * distortion,
                     snoise_3d(p + random_float3_offset(2.0f)) * distortion);
  }

  *value = fractal_noise_3d(p, detail, roughness);
  if (color_is_needed) {
    *color = make_float3(*value,
                         fractal_noise_3d(p + random_float3_offset(3.0f), detail, roughness),
                         fractal_noise_3d(p + random_float3_offset(4.0f), detail, roughness));
  }
}

ccl_device void noise_texture_4d(float4 co,
                                 float detail,
                                 float roughness,
                                 float distortion,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float4 p = co;
  if (distortion != 0.0f) {
    p += make_float4(snoise_4d(p + random_float4_offset(0.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(1.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(2.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(3.0f)) * distortion);
  }

  *value = fractal_noise_4d(p, detail, roughness);
  if (color_is_needed) {
    *color = make_float3(*value,
                         fractal_noise_4d(p + random_float4_offset(4.0f), detail, roughness),
                         fractal_noise_4d(p + random_float4_offset(5.0f), detail, roughness));
  }
}

ccl_device_noinline int svm_node_tex_noise(ccl_global const KernelGlobals *kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           uint dimensions,
                                           uint offsets1,
                                           uint offsets2,
                                           int offset)
{
  uint vector_stack_offset, w_stack_offset, scale_stack_offset;
  uint detail_stack_offset, roughness_stack_offset, distortion_stack_offset;
  uint value_stack_offset, color_stack_offset;

  svm_unpack_node_uchar4(
      offsets1, &vector_stack_offset, &w_stack_offset, &scale_stack_offset, &detail_stack_offset);
  svm_unpack_node_uchar4(offsets2,
                         &roughness_stack_offset,
                         &distortion_stack_offset,
                         &value_stack_offset,
                         &color_stack_offset);

  uint4 defaults1 = read_node(kg, &offset);
  uint4 defaults2 = read_node(kg, &offset);

  float3 vector = stack_load_float3(stack, vector_stack_offset);
  float w = stack_load_float_default(stack, w_stack_offset, defaults1.x);
  float scale = stack_load_float_default(stack, scale_stack_offset, defaults1.y);
  float detail = stack_load_float_default(stack, detail_stack_offset, defaults1.z);
  float roughness = stack_load_float_default(stack, roughness_stack_offset, defaults1.w);
  float distortion = stack_load_float_default(stack, distortion_stack_offset, defaults2.x);

  vector *= scale;
  w *= scale;

  float value;
  float3 color;
  switch (dimensions) {
    case 1:
      noise_texture_1d(
          w, detail, roughness, distortion, stack_valid(color_stack_offset), &value, &color);
      break;
    case 2:
      noise_texture_2d(make_float2(vector.x, vector.y),
                       detail,
                       roughness,
                       distortion,
                       stack_valid(color_stack_offset),
                       &value,
                       &color);
      break;
    case 3:
      noise_texture_3d(
          vector, detail, roughness, distortion, stack_valid(color_stack_offset), &value, &color);
      break;
    case 4:
      noise_texture_4d(make_float4(vector.x, vector.y, vector.z, w),
                       detail,
                       roughness,
                       distortion,
                       stack_valid(color_stack_offset),
                       &value,
                       &color);
      break;
    default:
      kernel_assert(0);
  }

  if (stack_valid(value_stack_offset)) {
    stack_store_float(stack, value_stack_offset, value);
  }
  if (stack_valid(color_stack_offset)) {
    stack_store_float3(stack, color_stack_offset, color);
  }
  return offset;
}

CCL_NAMESPACE_END
