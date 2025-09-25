/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Create the Z-bins from Z-sorted lights.
 * Perform min-max operation in LDS memory for speed.
 * For this reason, we only dispatch 1 thread group.
 */

#include "infos/eevee_light_culling_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_light_culling_zbin)

#include "draw_view_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"

void main()
{
  constexpr uint zbin_iter = CULLING_ZBIN_COUNT / gl_WorkGroupSize.x;
  const uint zbin_local = gl_LocalInvocationID.x * zbin_iter;

  for (uint i = 0u, l = zbin_local; i < zbin_iter; i++, l++) {
    zbin_max[l] = 0x0u;
    zbin_min[l] = ~0x0u;
  }
  barrier();

  uint light_iter = divide_ceil(light_cull_buf.visible_count, gl_WorkGroupSize.x);
  for (uint i = 0u; i < light_iter; i++) {
    uint index = i * gl_WorkGroupSize.x + gl_LocalInvocationID.x;
    if (index >= light_cull_buf.visible_count) {
      continue;
    }
    LightData light = light_buf[index];
    float3 P = light_position_get(light);
    /* TODO(fclem): Could have better bounds for spot and area lights. */
    float radius = light_local_data_get(light).influence_radius_max;
    float z_dist = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());
    int z_min = culling_z_to_zbin(
        light_cull_buf.zbin_scale, light_cull_buf.zbin_bias, z_dist + radius);
    int z_max = culling_z_to_zbin(
        light_cull_buf.zbin_scale, light_cull_buf.zbin_bias, z_dist - radius);
    z_min = clamp(z_min, 0, CULLING_ZBIN_COUNT - 1);
    z_max = clamp(z_max, 0, CULLING_ZBIN_COUNT - 1);
    /* Register to Z bins. */
    for (int z = z_min; z <= z_max; z++) {
      atomicMin(zbin_min[z], index);
      atomicMax(zbin_max[z], index);
    }
  }
  barrier();

  /* Write result to Z-bins buffer. Pack min & max into 1 `uint`. */
  for (uint i = 0u, l = zbin_local; i < zbin_iter; i++, l++) {
    out_zbin_buf[l] = (zbin_max[l] << 16u) | (zbin_min[l] & 0xFFFFu);
  }
}
