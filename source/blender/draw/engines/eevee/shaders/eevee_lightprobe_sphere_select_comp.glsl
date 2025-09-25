/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Select spherical probes to be considered during render and copy irradiance data from the
 * irradiance cache from each spherical probe location except for the world probe.
 */

#include "infos/eevee_lightprobe_sphere_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE_SELECT
#  undef SPHERE_PROBE
#endif
COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_sphere_select)

#include "eevee_lightprobe_sphere_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"

void main()
{
  int idx = int(gl_GlobalInvocationID.x);
  if (idx >= lightprobe_sphere_count) {
    return;
  }

  SphericalHarmonicL1 sh;
  if (idx == lightprobe_sphere_count - 1) {
    sh = lightprobe_volume_world();
  }
  else {
    float3 probe_center = lightprobe_sphere_buf[idx].location;
    sh = lightprobe_volume_sample(probe_center);
  }

  float clamp_indirect_sh = uniform_buf.clamp.surface_indirect;
  sh = spherical_harmonics_clamp(sh, clamp_indirect_sh);

  lightprobe_sphere_buf[idx].low_freq_light = lightprobe_spheres_extract_low_freq(sh);
}
