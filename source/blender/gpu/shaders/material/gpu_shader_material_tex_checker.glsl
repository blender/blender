/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_tex_checker(
    float3 co, float4 color1, float4 color2, float scale, out float4 color, out float fac)
{
  float3 p = co * scale;

  /* Prevent precision issues on unit coordinates. */
  p = (p + 0.000001f) * 0.999999f;

  int xi = int(abs(floor(p.x)));
  int yi = int(abs(floor(p.y)));
  int zi = int(abs(floor(p.z)));

  bool check = ((mod(xi, 2) == mod(yi, 2)) == bool(mod(zi, 2)));

  color = check ? color1 : color2;
  fac = check ? 1.0f : 0.0f;
}
