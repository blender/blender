/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Brick */

ccl_device_inline float brick_noise(uint n) /* fast integer noise */
{
  uint nn;
  n = (n + 1013) & 0x7fffffff;
  n = (n >> 13) ^ n;
  nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
  return 0.5f * ((float)nn / 1073741824.0f);
}

ccl_device_noinline_cpu float2 svm_brick(const float3 p,
                                         const float mortar_size,
                                         const float mortar_smooth,
                                         const float bias,
                                         float brick_width,
                                         const float row_height,
                                         const float offset_amount,
                                         const int offset_frequency,
                                         const float squash_amount,
                                         const int squash_frequency)
{
  int bricknum;
  int rownum;
  float offset = 0.0f;
  float x;
  float y;

  rownum = floor_to_int(p.y / row_height);

  if (offset_frequency && squash_frequency) {
    brick_width *= (rownum % squash_frequency) ? 1.0f : squash_amount;           /* squash */
    offset = (rownum % offset_frequency) ? 0.0f : (brick_width * offset_amount); /* offset */
  }

  bricknum = floor_to_int((p.x + offset) / brick_width);

  x = (p.x + offset) - brick_width * bricknum;
  y = p.y - row_height * rownum;

  const float tint = saturatef((brick_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias));
  float min_dist = min(min(x, y), min(brick_width - x, row_height - y));

  float mortar;
  if (min_dist >= mortar_size) {
    mortar = 0.0f;
  }
  else if (mortar_smooth == 0.0f) {
    mortar = 1.0f;
  }
  else {
    min_dist = 1.0f - min_dist / mortar_size;
    mortar = smoothstepf(min_dist / mortar_smooth);
  }

  return make_float2(tint, mortar);
}

ccl_device_noinline int svm_node_tex_brick(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint4 node,
                                           int offset)
{
  const uint4 node2 = read_node(kg, &offset);
  const uint4 node3 = read_node(kg, &offset);
  const uint4 node4 = read_node(kg, &offset);

  /* Input and Output Sockets */
  uint co_offset;
  uint color1_offset;
  uint color2_offset;
  uint mortar_offset;
  uint scale_offset;
  uint mortar_size_offset;
  uint bias_offset;
  uint brick_width_offset;
  uint row_height_offset;
  uint color_offset;
  uint fac_offset;
  uint mortar_smooth_offset;

  /* RNA properties */
  uint offset_frequency;
  uint squash_frequency;

  svm_unpack_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &mortar_offset);
  svm_unpack_node_uchar4(
      node.z, &scale_offset, &mortar_size_offset, &bias_offset, &brick_width_offset);
  svm_unpack_node_uchar4(
      node.w, &row_height_offset, &color_offset, &fac_offset, &mortar_smooth_offset);

  svm_unpack_node_uchar2(node2.x, &offset_frequency, &squash_frequency);

  const float3 co = stack_load_float3(stack, co_offset);

  float3 color1 = stack_load_float3(stack, color1_offset);
  const float3 color2 = stack_load_float3(stack, color2_offset);
  const float3 mortar = stack_load_float3(stack, mortar_offset);

  const float scale = stack_load_float_default(stack, scale_offset, node2.y);
  const float mortar_size = stack_load_float_default(stack, mortar_size_offset, node2.z);
  const float mortar_smooth = stack_load_float_default(stack, mortar_smooth_offset, node4.x);
  const float bias = stack_load_float_default(stack, bias_offset, node2.w);
  const float brick_width = stack_load_float_default(stack, brick_width_offset, node3.x);
  const float row_height = stack_load_float_default(stack, row_height_offset, node3.y);
  const float offset_amount = __int_as_float(node3.z);
  const float squash_amount = __int_as_float(node3.w);

  const float2 f2 = svm_brick(co * scale,
                              mortar_size,
                              mortar_smooth,
                              bias,
                              brick_width,
                              row_height,
                              offset_amount,
                              offset_frequency,
                              squash_amount,
                              squash_frequency);

  const float tint = f2.x;
  const float f = f2.y;

  if (f != 1.0f) {
    const float facm = 1.0f - tint;
    color1 = facm * color1 + tint * color2;
  }

  if (stack_valid(color_offset)) {
    stack_store_float3(stack, color_offset, color1 * (1.0f - f) + mortar * f);
  }
  if (stack_valid(fac_offset)) {
    stack_store_float(stack, fac_offset, f);
  }
  return offset;
}

CCL_NAMESPACE_END
