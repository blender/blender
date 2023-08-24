/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* -------------------------------------------------------------------- */
/** \name YCoCg
 * \{ */

vec3 colorspace_YCoCg_from_scene_linear(vec3 rgb_color)
{
  const mat3 colorspace_tx = transpose(mat3(vec3(1, 2, 1),     /* Y */
                                            vec3(2, 0, -2),    /* Co */
                                            vec3(-1, 2, -1))); /* Cg */
  return colorspace_tx * rgb_color;
}

vec4 colorspace_YCoCg_from_scene_linear(vec4 rgba_color)
{
  return vec4(colorspace_YCoCg_from_scene_linear(rgba_color.rgb), rgba_color.a);
}

vec3 colorspace_scene_linear_from_YCoCg(vec3 ycocg_color)
{
  float Y = ycocg_color.x;
  float Co = ycocg_color.y;
  float Cg = ycocg_color.z;

  vec3 rgb_color;
  rgb_color.r = Y + Co - Cg;
  rgb_color.g = Y + Cg;
  rgb_color.b = Y - Co - Cg;
  return rgb_color * 0.25;
}

vec4 colorspace_scene_linear_from_YCoCg(vec4 ycocg_color)
{
  return vec4(colorspace_scene_linear_from_YCoCg(ycocg_color.rgb), ycocg_color.a);
}

/** \} */
