/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_common_hash.glsl"

[[node]]
void node_point_info(float3 &position, float &radius, float &random)
{
  /* TODO(fclem): EEVEE implementation leaking. */
#ifdef MAT_GEOM_POINTCLOUD
  position = pointcloud_interp.position;
  radius = pointcloud_interp.radius;
  random = wang_hash_noise(uint(pointcloud_interp_flat.id));
#else
  position = float3(0.0f, 0.0f, 0.0f);
  radius = 0.0f;
  random = 0.0f;
#endif
}
