/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/fractal_noise.h"

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

template<typename T>
ccl_device float noise_select(T p,
                              float detail,
                              float roughness,
                              float lacunarity,
                              float offset,
                              float gain,
                              int type,
                              bool normalize)
{
  switch ((NodeNoiseType)type) {
    case NODE_NOISE_MULTIFRACTAL: {
      return noise_multi_fractal(p, detail, roughness, lacunarity);
    }
    case NODE_NOISE_FBM: {
      return noise_fbm(p, detail, roughness, lacunarity, normalize);
    }
    case NODE_NOISE_HYBRID_MULTIFRACTAL: {
      return noise_hybrid_multi_fractal(p, detail, roughness, lacunarity, offset, gain);
    }
    case NODE_NOISE_RIDGED_MULTIFRACTAL: {
      return noise_ridged_multi_fractal(p, detail, roughness, lacunarity, offset, gain);
    }
    case NODE_NOISE_HETERO_TERRAIN: {
      return noise_hetero_terrain(p, detail, roughness, lacunarity, offset);
    }
    default: {
      kernel_assert(0);
      return 0.0;
    }
  }
}

ccl_device void noise_texture_1d(float co,
                                 float detail,
                                 float roughness,
                                 float lacunarity,
                                 float offset,
                                 float gain,
                                 float distortion,
                                 int type,
                                 bool normalize,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float p = co;
  if (distortion != 0.0f) {
    p += snoise_1d(p + random_float_offset(0.0f)) * distortion;
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float_offset(1.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float_offset(2.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device void noise_texture_2d(float2 co,
                                 float detail,
                                 float roughness,
                                 float lacunarity,
                                 float offset,
                                 float gain,
                                 float distortion,
                                 int type,
                                 bool normalize,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float2 p = co;
  if (distortion != 0.0f) {
    p += make_float2(snoise_2d(p + random_float2_offset(0.0f)) * distortion,
                     snoise_2d(p + random_float2_offset(1.0f)) * distortion);
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float2_offset(2.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float2_offset(3.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device void noise_texture_3d(float3 co,
                                 float detail,
                                 float roughness,
                                 float lacunarity,
                                 float offset,
                                 float gain,
                                 float distortion,
                                 int type,
                                 bool normalize,
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

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float3_offset(3.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float3_offset(4.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device void noise_texture_4d(float4 co,
                                 float detail,
                                 float roughness,
                                 float lacunarity,
                                 float offset,
                                 float gain,
                                 float distortion,
                                 int type,
                                 bool normalize,
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

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float4_offset(4.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float4_offset(5.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device_noinline int svm_node_tex_noise(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           uint offsets1,
                                           uint offsets2,
                                           uint offsets3,
                                           int node_offset)
{
  uint vector_stack_offset, w_stack_offset, scale_stack_offset, detail_stack_offset;
  uint roughness_stack_offset, lacunarity_stack_offset, offset_stack_offset, gain_stack_offset;
  uint distortion_stack_offset, value_stack_offset, color_stack_offset;

  svm_unpack_node_uchar4(
      offsets1, &vector_stack_offset, &w_stack_offset, &scale_stack_offset, &detail_stack_offset);
  svm_unpack_node_uchar4(offsets2,
                         &roughness_stack_offset,
                         &lacunarity_stack_offset,
                         &offset_stack_offset,
                         &gain_stack_offset);
  svm_unpack_node_uchar3(
      offsets3, &distortion_stack_offset, &value_stack_offset, &color_stack_offset);

  uint4 defaults1 = read_node(kg, &node_offset);
  uint4 defaults2 = read_node(kg, &node_offset);
  uint4 properties = read_node(kg, &node_offset);

  uint dimensions = properties.x;
  uint type = properties.y;
  uint normalize = properties.z;

  float3 vector = stack_load_float3(stack, vector_stack_offset);
  float w = stack_load_float_default(stack, w_stack_offset, defaults1.x);
  float scale = stack_load_float_default(stack, scale_stack_offset, defaults1.y);
  float detail = stack_load_float_default(stack, detail_stack_offset, defaults1.z);
  float roughness = stack_load_float_default(stack, roughness_stack_offset, defaults1.w);
  float lacunarity = stack_load_float_default(stack, lacunarity_stack_offset, defaults2.x);
  float offset = stack_load_float_default(stack, offset_stack_offset, defaults2.y);
  float gain = stack_load_float_default(stack, gain_stack_offset, defaults2.z);
  float distortion = stack_load_float_default(stack, distortion_stack_offset, defaults2.w);

  detail = clamp(detail, 0.0f, 15.0f);
  roughness = fmaxf(roughness, 0.0f);

  vector *= scale;
  w *= scale;

  float value;
  float3 color;
  switch (dimensions) {
    case 1:
      noise_texture_1d(w,
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       type,
                       normalize,
                       stack_valid(color_stack_offset),
                       &value,
                       &color);
      break;
    case 2:
      noise_texture_2d(make_float2(vector.x, vector.y),
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       type,
                       normalize,
                       stack_valid(color_stack_offset),
                       &value,
                       &color);
      break;
    case 3:
      noise_texture_3d(vector,
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       type,
                       normalize,
                       stack_valid(color_stack_offset),
                       &value,
                       &color);
      break;
    case 4:
      noise_texture_4d(make_float4(vector.x, vector.y, vector.z, w),
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       type,
                       normalize,
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
  return node_offset;
}

CCL_NAMESPACE_END
