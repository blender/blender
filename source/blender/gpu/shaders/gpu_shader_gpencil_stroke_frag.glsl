/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(USE_GEOMETRY_SHADER) || defined(USE_GEOMETRY_IFACE_COLOR)
vec4 fragment_in_color()
{
  return geometry_out.mColor;
}

vec2 fragment_in_tex_coord()
{
  return geometry_out.mTexCoord;
}
#else
vec4 fragment_in_color()
{
  return geometry_in.finalColor;
}

vec2 fragment_in_tex_coord()
{
  return vec2(0.5);
}
#endif

void main()
{
  const vec2 center = vec2(0, 0.5);
  vec4 tColor = fragment_in_color();
  /* if alpha < 0, then encap */
  if (tColor.a < 0) {
    tColor.a = tColor.a * -1.0;
    float dist = length(fragment_in_tex_coord() - center);
    if (dist > 0.25) {
      discard;
    }
  }
  /* Solid */
  fragColor = tColor;
}
