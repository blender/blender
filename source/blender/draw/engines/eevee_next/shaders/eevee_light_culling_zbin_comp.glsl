/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Create the Z-bins from Z-sorted lights.
 * Perform min-max operation in LDS memory for speed.
 * For this reason, we only dispatch 1 thread group.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

/* Fits the limit of 32KB. */
shared uint zbin_max[CULLING_ZBIN_COUNT];
shared uint zbin_min[CULLING_ZBIN_COUNT];

void main()
{
  const uint zbin_iter = CULLING_ZBIN_COUNT / gl_WorkGroupSize.x;
  const uint zbin_local = gl_LocalInvocationID.x * zbin_iter;

  uint src_index = gl_GlobalInvocationID.x;

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
    vec3 P = light_buf[index]._position;
    /* TODO(fclem): Could have better bounds for spot and area lights. */
    float radius = light_buf[index].influence_radius_max;
    float z_dist = dot(cameraForward, P) - dot(cameraForward, cameraPos);
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
