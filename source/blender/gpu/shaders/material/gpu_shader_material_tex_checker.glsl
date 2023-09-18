/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_tex_checker(
    vec3 co, vec4 color1, vec4 color2, float scale, out vec4 color, out float fac)
{
  vec3 p = co * scale;

  /* Prevent precision issues on unit coordinates. */
  p = (p + 0.000001) * 0.999999;

  int xi = int(abs(floor(p.x)));
  int yi = int(abs(floor(p.y)));
  int zi = int(abs(floor(p.z)));

  bool check = ((mod(xi, 2) == mod(yi, 2)) == bool(mod(zi, 2)));

  color = check ? color1 : color2;
  fac = check ? 1.0 : 0.0;
}
