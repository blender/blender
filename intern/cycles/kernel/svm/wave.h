/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/fractal_noise.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Wave */

ccl_device_noinline_cpu float svm_wave(NodeWaveType type,
                                       NodeWaveBandsDirection bands_dir,
                                       NodeWaveRingsDirection rings_dir,
                                       NodeWaveProfile profile,
                                       float3 p,
                                       const float distortion,
                                       const float detail,
                                       const float dscale,
                                       const float droughness,
                                       const float phase)
{
  /* Prevent precision issues on unit coordinates. */
  p = (p + 0.000001f) * 0.999999f;

  float n;

  if (type == NODE_WAVE_BANDS) {
    if (bands_dir == NODE_WAVE_BANDS_DIRECTION_X) {
      n = p.x * 20.0f;
    }
    else if (bands_dir == NODE_WAVE_BANDS_DIRECTION_Y) {
      n = p.y * 20.0f;
    }
    else if (bands_dir == NODE_WAVE_BANDS_DIRECTION_Z) {
      n = p.z * 20.0f;
    }
    else { /* NODE_WAVE_BANDS_DIRECTION_DIAGONAL */
      n = (p.x + p.y + p.z) * 10.0f;
    }
  }
  else { /* NODE_WAVE_RINGS */
    float3 rp = p;
    if (rings_dir == NODE_WAVE_RINGS_DIRECTION_X) {
      rp *= make_float3(0.0f, 1.0f, 1.0f);
    }
    else if (rings_dir == NODE_WAVE_RINGS_DIRECTION_Y) {
      rp *= make_float3(1.0f, 0.0f, 1.0f);
    }
    else if (rings_dir == NODE_WAVE_RINGS_DIRECTION_Z) {
      rp *= make_float3(1.0f, 1.0f, 0.0f);
    }
    /* else: NODE_WAVE_RINGS_DIRECTION_SPHERICAL */

    n = len(rp) * 20.0f;
  }

  n += phase;

  if (distortion != 0.0f) {
    n += distortion * (noise_fbm(p * dscale, detail, droughness, 2.0f, true) * 2.0f - 1.0f);
  }

  if (profile == NODE_WAVE_PROFILE_SIN) {
    return 0.5f + 0.5f * sinf(n - M_PI_2_F);
  }
  if (profile == NODE_WAVE_PROFILE_SAW) {
    n /= M_2PI_F;
    return n - floorf(n);
  }
  /* NODE_WAVE_PROFILE_TRI */
  n /= M_2PI_F;
  return fabsf(n - floorf(n + 0.5f)) * 2.0f;
}

ccl_device_noinline int svm_node_tex_wave(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          const uint4 node,
                                          int offset)
{
  const uint4 node2 = read_node(kg, &offset);
  const uint4 node3 = read_node(kg, &offset);

  /* RNA properties */
  uint type_offset;
  uint bands_dir_offset;
  uint rings_dir_offset;
  uint profile_offset;
  /* Inputs, Outputs */
  uint co_offset;
  uint scale_offset;
  uint distortion_offset;
  uint detail_offset;
  uint dscale_offset;
  uint droughness_offset;
  uint phase_offset;
  uint color_offset;
  uint fac_offset;

  svm_unpack_node_uchar4(
      node.y, &type_offset, &bands_dir_offset, &rings_dir_offset, &profile_offset);
  svm_unpack_node_uchar3(node.z, &co_offset, &scale_offset, &distortion_offset);
  svm_unpack_node_uchar4(
      node.w, &detail_offset, &dscale_offset, &droughness_offset, &phase_offset);
  svm_unpack_node_uchar2(node2.x, &color_offset, &fac_offset);

  const float3 co = stack_load_float3(stack, co_offset);
  const float scale = stack_load_float_default(stack, scale_offset, node2.y);
  const float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
  const float detail = stack_load_float_default(stack, detail_offset, node2.w);
  const float dscale = stack_load_float_default(stack, dscale_offset, node3.x);
  const float droughness = stack_load_float_default(stack, droughness_offset, node3.y);
  const float phase = stack_load_float_default(stack, phase_offset, node3.z);

  const float f = svm_wave((NodeWaveType)type_offset,
                           (NodeWaveBandsDirection)bands_dir_offset,
                           (NodeWaveRingsDirection)rings_dir_offset,
                           (NodeWaveProfile)profile_offset,
                           co * scale,
                           distortion,
                           detail,
                           dscale,
                           droughness,
                           phase);

  if (stack_valid(fac_offset)) {
    stack_store_float(stack, fac_offset, f);
  }
  if (stack_valid(color_offset)) {
    stack_store_float3(stack, color_offset, make_float3(f, f, f));
  }
  return offset;
}

CCL_NAMESPACE_END
