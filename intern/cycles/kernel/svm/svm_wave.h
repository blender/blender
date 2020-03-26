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

/* Wave */

ccl_device_noinline_cpu float svm_wave(NodeWaveType type,
                                       NodeWaveBandsDirection bands_dir,
                                       NodeWaveRingsDirection rings_dir,
                                       NodeWaveProfile profile,
                                       float3 p,
                                       float distortion,
                                       float detail,
                                       float dscale,
                                       float droughness,
                                       float phase)
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

  if (distortion != 0.0f)
    n += distortion * (fractal_noise_3d(p * dscale, detail, droughness) * 2.0f - 1.0f);

  if (profile == NODE_WAVE_PROFILE_SIN) {
    return 0.5f + 0.5f * sinf(n - M_PI_2_F);
  }
  else if (profile == NODE_WAVE_PROFILE_SAW) {
    n /= M_2PI_F;
    return n - floorf(n);
  }
  else { /* NODE_WAVE_PROFILE_TRI */
    n /= M_2PI_F;
    return fabsf(n - floorf(n + 0.5f)) * 2.0f;
  }
}

ccl_device void svm_node_tex_wave(
    KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
  uint4 node2 = read_node(kg, offset);
  uint4 node3 = read_node(kg, offset);

  /* RNA properties */
  uint type_offset, bands_dir_offset, rings_dir_offset, profile_offset;
  /* Inputs, Outputs */
  uint co_offset, scale_offset, distortion_offset, detail_offset, dscale_offset, droughness_offset,
      phase_offset;
  uint color_offset, fac_offset;

  svm_unpack_node_uchar4(
      node.y, &type_offset, &bands_dir_offset, &rings_dir_offset, &profile_offset);
  svm_unpack_node_uchar3(node.z, &co_offset, &scale_offset, &distortion_offset);
  svm_unpack_node_uchar4(
      node.w, &detail_offset, &dscale_offset, &droughness_offset, &phase_offset);
  svm_unpack_node_uchar2(node2.x, &color_offset, &fac_offset);

  float3 co = stack_load_float3(stack, co_offset);
  float scale = stack_load_float_default(stack, scale_offset, node2.y);
  float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
  float detail = stack_load_float_default(stack, detail_offset, node2.w);
  float dscale = stack_load_float_default(stack, dscale_offset, node3.x);
  float droughness = stack_load_float_default(stack, droughness_offset, node3.y);
  float phase = stack_load_float_default(stack, phase_offset, node3.z);

  float f = svm_wave((NodeWaveType)type_offset,
                     (NodeWaveBandsDirection)bands_dir_offset,
                     (NodeWaveRingsDirection)rings_dir_offset,
                     (NodeWaveProfile)profile_offset,
                     co * scale,
                     distortion,
                     detail,
                     dscale,
                     droughness,
                     phase);

  if (stack_valid(fac_offset))
    stack_store_float(stack, fac_offset, f);
  if (stack_valid(color_offset))
    stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END
