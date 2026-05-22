/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_lightprobe_sphere_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE_SELECT
#  undef SPHERE_PROBE
#endif

#include "eevee_lightprobe_sphere_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"

namespace eevee::lightprobe::sphere {

struct Cull {
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_volume_probe_data;

  [[storage(0, read_write)]] SphereProbeData (&lightprobe_sphere_buf)[SPHERE_PROBE_MAX];

  [[push_constant]] const int lightprobe_sphere_count;
};

/**
 * Select spherical probes to be considered during render and copy irradiance data from the
 * irradiance cache from each spherical probe location except for the world probe.
 */
[[compute, local_size(SPHERE_PROBE_SELECT_GROUP_SIZE)]]
void cull([[resource_table]] Cull &srt,
          [[global_invocation_id]] const uint3 global_id,
          [[local_invocation_id]] const uint3 local_id,
          [[local_invocation_index]] const uint local_index)
{
  int idx = int(global_id.x);
  if (idx >= srt.lightprobe_sphere_count) {
    return;
  }

  SphericalHarmonicL1<float4> sh;
  if (idx == srt.lightprobe_sphere_count - 1) {
    sh = lightprobe_volume_world();
  }
  else {
    float3 probe_center = srt.lightprobe_sphere_buf[idx].location;
    sh = lightprobe_volume_sample(probe_center);
  }

  float clamp_indirect_sh = uniform_buf.clamp.surface_indirect;
  sh = spherical_harmonics::clamp_energy(sh, clamp_indirect_sh);

  srt.lightprobe_sphere_buf[idx].low_freq_light = lightprobe_spheres_extract_low_freq(sh);
}

}  // namespace eevee::lightprobe::sphere

PipelineCompute eevee_lightprobe_sphere_select(eevee::lightprobe::sphere::cull);
