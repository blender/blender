/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_wireframe(float size, out float fac)
{
  vec3 barys = g_data.barycentric_coords.xyy;
  barys.z = 1.0 - barys.x - barys.y;

  size *= 0.5;
  vec3 s = step(-size, -barys * g_data.barycentric_dists);

  fac = max(s.x, max(s.y, s.z));
}

void node_wireframe_screenspace(float size, out float fac)
{
  vec3 barys = g_data.barycentric_coords.xyy;
  barys.z = 1.0 - barys.x - barys.y;

#ifdef GPU_FRAGMENT_SHADER
  size *= (1.0 / 3.0);
  vec3 dx = dFdx(barys);
  vec3 dy = dFdy(barys);
  vec3 deltas = sqrt(dx * dx + dy * dy);

  vec3 s = step(-deltas * size, -barys);

  fac = max(s.x, max(s.y, s.z));
#else
  fac = 1.0;
#endif
}
