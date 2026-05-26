/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_lightprobe_volume.bsl.hh"
#include "eevee_uniform.bsl.hh"

namespace eevee::lightprobe::sphere {

struct Cull {
  [[storage(0, read_write)]] SphereProbeData (&lightprobe_sphere_buf)[SPHERE_PROBE_MAX];

  [[push_constant]] const int lightprobe_sphere_count;
};

/**
 * Select spherical probes to be considered during render and copy irradiance data from the
 * irradiance cache from each spherical probe location except for the world probe.
 */
[[compute, local_size(SPHERE_PROBE_SELECT_GROUP_SIZE)]]
void cull([[resource_table]] Cull &srt,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const LightprobeVolumeRenderData &volumes,
          [[global_invocation_id]] const uint3 global_id)
{
  int idx = int(global_id.x);
  if (idx >= srt.lightprobe_sphere_count) {
    return;
  }

  SphericalHarmonicL1<float4> sh;
  if (idx == srt.lightprobe_sphere_count - 1) {
    sh = volumes.world();
  }
  else {
    float3 probe_center = srt.lightprobe_sphere_buf[idx].location;
    sh = volumes.sample_probe_no_dithered_no_biases(probe_center);
  }

  float clamp_indirect_sh = uni.uniform_buf.clamp.surface_indirect;
  sh = spherical_harmonics::clamp_energy(sh, clamp_indirect_sh);

  srt.lightprobe_sphere_buf[idx].low_freq_light = lightprobe::sphere::extract_low_freq_lighting(
      sh);
}

}  // namespace eevee::lightprobe::sphere

PipelineCompute eevee_lightprobe_sphere_select(eevee::lightprobe::sphere::cull);
