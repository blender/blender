/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* NOTE: svm_ramp.h, svm_ramp_util.h and node_ramp_util.h must stay consistent */

ccl_device_inline float fetch_float(KernelGlobals kg, const int offset)
{
  const uint4 node = kernel_data_fetch(svm_nodes, offset);
  return __uint_as_float(node.x);
}

ccl_device_inline float float_ramp_lookup(KernelGlobals kg,
                                          const int offset,
                                          float f,
                                          bool interpolate,
                                          bool extrapolate,
                                          const int table_size)
{
  if ((f < 0.0f || f > 1.0f) && extrapolate) {
    float t0;
    float dy;
    if (f < 0.0f) {
      t0 = fetch_float(kg, offset);
      dy = t0 - fetch_float(kg, offset + 1);
      f = -f;
    }
    else {
      t0 = fetch_float(kg, offset + table_size - 1);
      dy = t0 - fetch_float(kg, offset + table_size - 2);
      f = f - 1.0f;
    }
    return t0 + dy * f * (table_size - 1);
  }

  f = saturatef(f) * (table_size - 1);

  /* clamp int as well in case of NaN */
  const int i = clamp(float_to_int(f), 0, table_size - 1);
  const float t = f - (float)i;

  float a = fetch_float(kg, offset + i);

  if (interpolate && t > 0.0f) {
    a = (1.0f - t) * a + t * fetch_float(kg, offset + i + 1);
  }

  return a;
}

ccl_device_inline float4 rgb_ramp_lookup(KernelGlobals kg,
                                         const int offset,
                                         float f,
                                         bool interpolate,
                                         bool extrapolate,
                                         const int table_size)
{
  if ((f < 0.0f || f > 1.0f) && extrapolate) {
    float4 t0;
    float4 dy;
    if (f < 0.0f) {
      t0 = fetch_node_float(kg, offset);
      dy = t0 - fetch_node_float(kg, offset + 1);
      f = -f;
    }
    else {
      t0 = fetch_node_float(kg, offset + table_size - 1);
      dy = t0 - fetch_node_float(kg, offset + table_size - 2);
      f = f - 1.0f;
    }
    return t0 + dy * f * (table_size - 1);
  }

  f = saturatef(f) * (table_size - 1);

  /* clamp int as well in case of NaN */
  const int i = clamp(float_to_int(f), 0, table_size - 1);
  const float t = f - (float)i;

  float4 a = fetch_node_float(kg, offset + i);

  if (interpolate && t > 0.0f) {
    a = (1.0f - t) * a + t * fetch_node_float(kg, offset + i + 1);
  }

  return a;
}

ccl_device_noinline int svm_node_rgb_ramp(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          const uint4 node,
                                          int offset)
{
  uint fac_offset;
  uint color_offset;
  uint alpha_offset;
  const uint interpolate = node.z;

  svm_unpack_node_uchar3(node.y, &fac_offset, &color_offset, &alpha_offset);

  const uint table_size = read_node(kg, &offset).x;

  const float fac = stack_load_float(stack, fac_offset);
  const float4 color = rgb_ramp_lookup(kg, offset, fac, interpolate, false, table_size);

  if (stack_valid(color_offset)) {
    stack_store_float3(stack, color_offset, make_float3(color));
  }
  if (stack_valid(alpha_offset)) {
    stack_store_float(stack, alpha_offset, color.w);
  }

  offset += table_size;
  return offset;
}

ccl_device_noinline int svm_node_curves(KernelGlobals kg,
                                        ccl_private ShaderData *sd,
                                        ccl_private float *stack,
                                        const uint4 node,
                                        int offset)
{
  uint fac_offset;
  uint color_offset;
  uint out_offset;
  uint extrapolate;
  svm_unpack_node_uchar4(node.y, &fac_offset, &color_offset, &out_offset, &extrapolate);

  const uint table_size = read_node(kg, &offset).x;

  const float fac = stack_load_float(stack, fac_offset);
  float3 color = stack_load_float3(stack, color_offset);

  const float min_x = __int_as_float(node.z);
  const float max_x = __int_as_float(node.w);
  const float range_x = max_x - min_x;
  const float3 relpos = (color - make_float3(min_x, min_x, min_x)) / range_x;

  const float r = rgb_ramp_lookup(kg, offset, relpos.x, true, extrapolate, table_size).x;
  const float g = rgb_ramp_lookup(kg, offset, relpos.y, true, extrapolate, table_size).y;
  const float b = rgb_ramp_lookup(kg, offset, relpos.z, true, extrapolate, table_size).z;

  color = (1.0f - fac) * color + fac * make_float3(r, g, b);
  stack_store_float3(stack, out_offset, color);

  offset += table_size;
  return offset;
}

ccl_device_noinline int svm_node_curve(KernelGlobals kg,
                                       ccl_private ShaderData *sd,
                                       ccl_private float *stack,
                                       const uint4 node,
                                       int offset)
{
  uint fac_offset;
  uint value_in_offset;
  uint out_offset;
  uint extrapolate;
  svm_unpack_node_uchar4(node.y, &fac_offset, &value_in_offset, &out_offset, &extrapolate);

  const uint table_size = read_node(kg, &offset).x;

  const float fac = stack_load_float(stack, fac_offset);
  float in = stack_load_float(stack, value_in_offset);

  const float min = __int_as_float(node.z);
  const float max = __int_as_float(node.w);
  const float range = max - min;
  const float relpos = (in - min) / range;

  const float v = float_ramp_lookup(kg, offset, relpos, true, extrapolate, table_size);

  in = (1.0f - fac) * in + fac * v;
  stack_store_float(stack, out_offset, in);

  offset += table_size;
  return offset;
}

CCL_NAMESPACE_END
