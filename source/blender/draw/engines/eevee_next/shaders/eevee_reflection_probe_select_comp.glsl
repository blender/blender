/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Select spherical probes to be considered during render and copy irradiance data from the
 * irradiance cache from each spherical probe location except for the world probe.
 */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_volume_eval_lib.glsl)

void main()
{
  int idx = int(gl_GlobalInvocationID.x);
  if (idx >= reflection_probe_count) {
    return;
  }

  SphericalHarmonicL1 sh;
  if (idx == reflection_probe_count - 1) {
    sh = lightprobe_irradiance_world();
  }
  else {
    vec3 probe_center = reflection_probe_buf[idx].location;
    sh = lightprobe_irradiance_sample(probe_center);
  }

  float clamp_indirect_sh = uniform_buf.clamp.surface_indirect;
  sh = spherical_harmonics_clamp(sh, clamp_indirect_sh);

  reflection_probe_buf[idx].low_freq_light = reflection_probes_extract_low_freq(sh);
}
