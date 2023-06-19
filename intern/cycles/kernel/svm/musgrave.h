/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/noise.h"

CCL_NAMESPACE_BEGIN

/* 1D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

ccl_device_noinline_cpu float noise_musgrave_fBm_1d(float co,
                                                    float H,
                                                    float lacunarity,
                                                    float octaves)
{
  float p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value += snoise_1d(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * snoise_1d(p) * pwr;
  }

  return value;
}

/* 1D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

ccl_device_noinline_cpu float noise_musgrave_multi_fractal_1d(float co,
                                                              float H,
                                                              float lacunarity,
                                                              float octaves)
{
  float p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value *= (pwr * snoise_1d(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_1d(p) + 1.0f); /* correct? */
  }

  return value;
}

/* 1D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hetero_terrain_1d(
    float co, float H, float lacunarity, float octaves, float offset)
{
  float p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_1d(p);
  p *= lacunarity;

  for (int i = 1; i < float_to_int(octaves); i++) {
    float increment = (snoise_1d(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (snoise_1d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* 1D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hybrid_multi_fractal_1d(
    float co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float p = co;
  float pwHL = powf(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i < float_to_int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (snoise_1d(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (snoise_1d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

/* 1D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_ridged_multi_fractal_1d(
    float co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(snoise_1d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i < float_to_int(octaves); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_1d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 2D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

ccl_device_noinline_cpu float noise_musgrave_fBm_2d(float2 co,
                                                    float H,
                                                    float lacunarity,
                                                    float octaves)
{
  float2 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value += snoise_2d(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * snoise_2d(p) * pwr;
  }

  return value;
}

/* 2D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

ccl_device_noinline_cpu float noise_musgrave_multi_fractal_2d(float2 co,
                                                              float H,
                                                              float lacunarity,
                                                              float octaves)
{
  float2 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value *= (pwr * snoise_2d(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_2d(p) + 1.0f); /* correct? */
  }

  return value;
}

/* 2D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hetero_terrain_2d(
    float2 co, float H, float lacunarity, float octaves, float offset)
{
  float2 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_2d(p);
  p *= lacunarity;

  for (int i = 1; i < float_to_int(octaves); i++) {
    float increment = (snoise_2d(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (snoise_2d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* 2D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hybrid_multi_fractal_2d(
    float2 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float2 p = co;
  float pwHL = powf(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i < float_to_int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (snoise_2d(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (snoise_2d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

/* 2D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_ridged_multi_fractal_2d(
    float2 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float2 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(snoise_2d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i < float_to_int(octaves); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_2d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 3D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

ccl_device_noinline_cpu float noise_musgrave_fBm_3d(float3 co,
                                                    float H,
                                                    float lacunarity,
                                                    float octaves)
{
  float3 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value += snoise_3d(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * snoise_3d(p) * pwr;
  }

  return value;
}

/* 3D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

ccl_device_noinline_cpu float noise_musgrave_multi_fractal_3d(float3 co,
                                                              float H,
                                                              float lacunarity,
                                                              float octaves)
{
  float3 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value *= (pwr * snoise_3d(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_3d(p) + 1.0f); /* correct? */
  }

  return value;
}

/* 3D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hetero_terrain_3d(
    float3 co, float H, float lacunarity, float octaves, float offset)
{
  float3 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_3d(p);
  p *= lacunarity;

  for (int i = 1; i < float_to_int(octaves); i++) {
    float increment = (snoise_3d(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (snoise_3d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* 3D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hybrid_multi_fractal_3d(
    float3 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float3 p = co;
  float pwHL = powf(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i < float_to_int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (snoise_3d(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (snoise_3d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

/* 3D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_ridged_multi_fractal_3d(
    float3 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float3 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(snoise_3d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i < float_to_int(octaves); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_3d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 4D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

ccl_device_noinline_cpu float noise_musgrave_fBm_4d(float4 co,
                                                    float H,
                                                    float lacunarity,
                                                    float octaves)
{
  float4 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value += snoise_4d(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * snoise_4d(p) * pwr;
  }

  return value;
}

/* 4D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

ccl_device_noinline_cpu float noise_musgrave_multi_fractal_4d(float4 co,
                                                              float H,
                                                              float lacunarity,
                                                              float octaves)
{
  float4 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  float pwHL = powf(lacunarity, -H);

  for (int i = 0; i < float_to_int(octaves); i++) {
    value *= (pwr * snoise_4d(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_4d(p) + 1.0f); /* correct? */
  }

  return value;
}

/* 4D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hetero_terrain_4d(
    float4 co, float H, float lacunarity, float octaves, float offset)
{
  float4 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_4d(p);
  p *= lacunarity;

  for (int i = 1; i < float_to_int(octaves); i++) {
    float increment = (snoise_4d(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (snoise_4d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* 4D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_hybrid_multi_fractal_4d(
    float4 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float4 p = co;
  float pwHL = powf(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i < float_to_int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (snoise_4d(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (snoise_4d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

/* 4D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

ccl_device_noinline_cpu float noise_musgrave_ridged_multi_fractal_4d(
    float4 co, float H, float lacunarity, float octaves, float offset, float gain)
{
  float4 p = co;
  float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(snoise_4d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i < float_to_int(octaves); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_4d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

ccl_device_noinline int svm_node_tex_musgrave(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              uint offsets1,
                                              uint offsets2,
                                              uint offsets3,
                                              int offset)
{
  uint type, dimensions, co_stack_offset, w_stack_offset;
  uint scale_stack_offset, detail_stack_offset, dimension_stack_offset, lacunarity_stack_offset;
  uint offset_stack_offset, gain_stack_offset, fac_stack_offset;

  svm_unpack_node_uchar4(offsets1, &type, &dimensions, &co_stack_offset, &w_stack_offset);
  svm_unpack_node_uchar4(offsets2,
                         &scale_stack_offset,
                         &detail_stack_offset,
                         &dimension_stack_offset,
                         &lacunarity_stack_offset);
  svm_unpack_node_uchar3(offsets3, &offset_stack_offset, &gain_stack_offset, &fac_stack_offset);

  uint4 defaults1 = read_node(kg, &offset);
  uint4 defaults2 = read_node(kg, &offset);

  float3 co = stack_load_float3(stack, co_stack_offset);
  float w = stack_load_float_default(stack, w_stack_offset, defaults1.x);
  float scale = stack_load_float_default(stack, scale_stack_offset, defaults1.y);
  float detail = stack_load_float_default(stack, detail_stack_offset, defaults1.z);
  float dimension = stack_load_float_default(stack, dimension_stack_offset, defaults1.w);
  float lacunarity = stack_load_float_default(stack, lacunarity_stack_offset, defaults2.x);
  float foffset = stack_load_float_default(stack, offset_stack_offset, defaults2.y);
  float gain = stack_load_float_default(stack, gain_stack_offset, defaults2.z);

  dimension = fmaxf(dimension, 1e-5f);
  detail = clamp(detail, 0.0f, 15.0f);
  lacunarity = fmaxf(lacunarity, 1e-5f);

  float fac;

  switch (dimensions) {
    case 1: {
      float p = w * scale;
      switch ((NodeMusgraveType)type) {
        case NODE_MUSGRAVE_MULTIFRACTAL:
          fac = noise_musgrave_multi_fractal_1d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_FBM:
          fac = noise_musgrave_fBm_1d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_HYBRID_MULTIFRACTAL:
          fac = noise_musgrave_hybrid_multi_fractal_1d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_RIDGED_MULTIFRACTAL:
          fac = noise_musgrave_ridged_multi_fractal_1d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_HETERO_TERRAIN:
          fac = noise_musgrave_hetero_terrain_1d(p, dimension, lacunarity, detail, foffset);
          break;
        default:
          fac = 0.0f;
      }
      break;
    }
    case 2: {
      float2 p = make_float2(co.x, co.y) * scale;
      switch ((NodeMusgraveType)type) {
        case NODE_MUSGRAVE_MULTIFRACTAL:
          fac = noise_musgrave_multi_fractal_2d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_FBM:
          fac = noise_musgrave_fBm_2d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_HYBRID_MULTIFRACTAL:
          fac = noise_musgrave_hybrid_multi_fractal_2d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_RIDGED_MULTIFRACTAL:
          fac = noise_musgrave_ridged_multi_fractal_2d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_HETERO_TERRAIN:
          fac = noise_musgrave_hetero_terrain_2d(p, dimension, lacunarity, detail, foffset);
          break;
        default:
          fac = 0.0f;
      }
      break;
    }
    case 3: {
      float3 p = co * scale;
      switch ((NodeMusgraveType)type) {
        case NODE_MUSGRAVE_MULTIFRACTAL:
          fac = noise_musgrave_multi_fractal_3d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_FBM:
          fac = noise_musgrave_fBm_3d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_HYBRID_MULTIFRACTAL:
          fac = noise_musgrave_hybrid_multi_fractal_3d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_RIDGED_MULTIFRACTAL:
          fac = noise_musgrave_ridged_multi_fractal_3d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_HETERO_TERRAIN:
          fac = noise_musgrave_hetero_terrain_3d(p, dimension, lacunarity, detail, foffset);
          break;
        default:
          fac = 0.0f;
      }
      break;
    }
    case 4: {
      float4 p = make_float4(co.x, co.y, co.z, w) * scale;
      switch ((NodeMusgraveType)type) {
        case NODE_MUSGRAVE_MULTIFRACTAL:
          fac = noise_musgrave_multi_fractal_4d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_FBM:
          fac = noise_musgrave_fBm_4d(p, dimension, lacunarity, detail);
          break;
        case NODE_MUSGRAVE_HYBRID_MULTIFRACTAL:
          fac = noise_musgrave_hybrid_multi_fractal_4d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_RIDGED_MULTIFRACTAL:
          fac = noise_musgrave_ridged_multi_fractal_4d(
              p, dimension, lacunarity, detail, foffset, gain);
          break;
        case NODE_MUSGRAVE_HETERO_TERRAIN:
          fac = noise_musgrave_hetero_terrain_4d(p, dimension, lacunarity, detail, foffset);
          break;
        default:
          fac = 0.0f;
      }
      break;
    }
    default:
      fac = 0.0f;
  }

  stack_store_float(stack, fac_stack_offset, fac);
  return offset;
}

CCL_NAMESPACE_END
