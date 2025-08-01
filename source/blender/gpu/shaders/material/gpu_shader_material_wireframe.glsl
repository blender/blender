/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_wireframe(float size, out float fac)
{
  float3 barys = g_data.barycentric_coords.xyy;
  barys.z = 1.0f - barys.x - barys.y;

  size *= 0.5f;
  float3 s = step(-size, -barys * g_data.barycentric_dists);

  fac = max(s.x, max(s.y, s.z));
}

void node_wireframe_screenspace(float size, out float fac)
{
  float3 barys = g_data.barycentric_coords.xyy;
  barys.z = 1.0f - barys.x - barys.y;

#ifdef GPU_FRAGMENT_SHADER
  size *= (1.0f / 3.0f);
  float3 dx = gpu_dfdx(barys) * derivative_scale_get();
  float3 dy = gpu_dfdy(barys) * derivative_scale_get();
  float3 deltas = sqrt(dx * dx + dy * dy);

  float3 s = step(-deltas * size, -barys);

  fac = max(s.x, max(s.y, s.z));
#else
  fac = 1.0f;
#endif
}
