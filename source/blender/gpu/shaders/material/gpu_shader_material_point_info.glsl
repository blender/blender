/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)

void node_point_info(out vec3 position, out float radius, out float random)
{
#ifdef POINTCLOUD_SHADER
  position = pointPosition;
  radius = pointRadius;
  random = wang_hash_noise(uint(pointID));
#else
  position = vec3(0.0, 0.0, 0.0);
  radius = 0.0;
  random = 0.0;
#endif
}
