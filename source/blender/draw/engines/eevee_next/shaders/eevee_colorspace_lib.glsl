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

/**
 * Clamp components to avoid black square artifacts if a pixel goes NaN or negative.
 * Threshold is arbitrary.
 */
vec4 colorspace_safe_color(vec4 c)
{
  return clamp(c, vec4(0.0), vec4(1e20));
}
vec3 colorspace_safe_color(vec3 c)
{
  return clamp(c, vec3(0.0), vec3(1e20));
}

/**
 * Clamp all components to the specified maximum and avoid color shifting.
 */
vec3 colorspace_brightness_clamp_max(vec3 color, float max_value)
{
  float luma = max(1e-8, max(max(color.r, color.g), color.b));
  if (luma < 1e-8) {
    return color;
  }
  return color * (1.0 - max(0.0, luma - max_value) / luma);
}
vec4 colorspace_brightness_clamp_max(vec4 color, float max_value)
{
  return vec4(colorspace_brightness_clamp_max(color.rgb, max_value), color.a);
}
