/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Sum all spherical harmonic coefficients extracting during remapping to octahedral map.
 * Dispatch only one thread-group that sums. */

#include "infos/eevee_lightprobe_sphere_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_sphere_irradiance)

#include "eevee_lightprobe_sphere_lib.glsl"
#include "eevee_lightprobe_sphere_mapping_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"

shared float4 local_sh_coefs[gl_WorkGroupSize.x][4];

void main()
{
  SphericalHarmonicL1 sh;
  sh.L0.M0 = float4(0.0f);
  sh.L1.Mn1 = float4(0.0f);
  sh.L1.M0 = float4(0.0f);
  sh.L1.Mp1 = float4(0.0f);

  /* First sum onto the local memory. */
  uint valid_data_len = probe_remap_dispatch_size.x * probe_remap_dispatch_size.y;
  constexpr uint iter_count = uint(SPHERE_PROBE_MAX_HARMONIC) / gl_WorkGroupSize.x;
  for (uint i = 0; i < iter_count; i++) {
    uint index = gl_WorkGroupSize.x * i + gl_LocalInvocationIndex;
    if (index >= valid_data_len) {
      break;
    }
    SphericalHarmonicL1 sh_sample;
    sh_sample.L0.M0 = in_sh[index].L0_M0;
    sh_sample.L1.Mn1 = in_sh[index].L1_Mn1;
    sh_sample.L1.M0 = in_sh[index].L1_M0;
    sh_sample.L1.Mp1 = in_sh[index].L1_Mp1;
    sh = spherical_harmonics_add(sh, sh_sample);
  }

  /* Then sum across invocations. */
  const uint local_index = gl_LocalInvocationIndex;
  local_sh_coefs[local_index][0] = sh.L0.M0;
  local_sh_coefs[local_index][1] = sh.L1.Mn1;
  local_sh_coefs[local_index][2] = sh.L1.M0;
  local_sh_coefs[local_index][3] = sh.L1.Mp1;

  /* Parallel sum. */
  constexpr uint group_size = gl_WorkGroupSize.x;
  uint stride = group_size / 2;
  for (int i = 0; i < 10; i++) {
    barrier();
    if (local_index < stride) {
      for (int i = 0; i < 4; i++) {
        local_sh_coefs[local_index][i] += local_sh_coefs[local_index + stride][i];
      }
    }
    stride /= 2;
  }

  barrier();
  if (gl_LocalInvocationIndex == 0u) {
    out_sh.L0_M0 = local_sh_coefs[0][0];
    out_sh.L1_Mn1 = local_sh_coefs[0][1];
    out_sh.L1_M0 = local_sh_coefs[0][2];
    out_sh.L1_Mp1 = local_sh_coefs[0][3];
  }
}
