/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

void node_point_info(out float3 position, out float radius, out float random)
{
#ifdef MAT_GEOM_POINTCLOUD
  /* EEVEE-Next case. */
  position = pointcloud_interp.position;
  radius = pointcloud_interp.radius;
  random = wang_hash_noise(uint(pointcloud_interp_flat.id));
#elif defined(POINTCLOUD_SHADER)
  /* EEVEE-Legacy case. */
  position = pointPosition;
  radius = pointRadius;
  random = wang_hash_noise(uint(pointID));
#else
  position = float3(0.0f, 0.0f, 0.0f);
  radius = 0.0f;
  random = 0.0f;
#endif
}
