/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

vec3 weight_to_rgb(float weight)
{
  vec3 r_rgb;
  float blend = ((weight / 2.0) + 0.5);

  if (weight <= 0.25) { /* blue->cyan */
    r_rgb[0] = 0.0;
    r_rgb[1] = blend * weight * 4.0;
    r_rgb[2] = blend;
  }
  else if (weight <= 0.50) { /* cyan->green */
    r_rgb[0] = 0.0;
    r_rgb[1] = blend;
    r_rgb[2] = blend * (1.0 - ((weight - 0.25) * 4.0));
  }
  else if (weight <= 0.75) { /* green->yellow */
    r_rgb[0] = blend * ((weight - 0.50) * 4.0);
    r_rgb[1] = blend;
    r_rgb[2] = 0.0;
  }
  else if (weight <= 1.0) { /* yellow->red */
    r_rgb[0] = blend;
    r_rgb[1] = blend * (1.0 - ((weight - 0.75) * 4.0));
    r_rgb[2] = 0.0;
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    r_rgb[0] = 1.0;
    r_rgb[1] = 0.0;
    r_rgb[2] = 1.0;
  }

  return r_rgb;
}

#define M_PI 3.1415926535897932

vec2 angle_to_v2(float angle)
{
  return vec2(cos(angle), sin(angle));
}

/* Adapted from BLI_math_vector.h */
float angle_normalized_v2v2(vec2 v1, vec2 v2)
{
  v1 = normalize(v1 * aspect);
  v2 = normalize(v2 * aspect);
  /* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
  bool q = (dot(v1, v2) >= 0.0);
  vec2 v = (q) ? (v1 - v2) : (v1 + v2);
  float a = 2.0 * asin(length(v) / 2.0);
  return (q) ? a : M_PI - a;
}

float area_ratio_to_stretch(float ratio, float tot_ratio)
{
  ratio *= tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

void main()
{
  vec3 world_pos = point_object_to_world(vec3(pos, 0.0));
  gl_Position = point_world_to_ndc(world_pos);

#ifdef STRETCH_ANGLE
  vec2 v1 = angle_to_v2(uv_angles.x * M_PI);
  vec2 v2 = angle_to_v2(uv_angles.y * M_PI);
  float uv_angle = angle_normalized_v2v2(v1, v2) / M_PI;
  float stretch = 1.0 - abs(uv_angle - angle);
  stretch = stretch;
  stretch = 1.0 - stretch * stretch;
#else
  float stretch = 1.0 - area_ratio_to_stretch(ratio, totalAreaRatio);

#endif

  finalColor = vec4(weight_to_rgb(stretch), stretch_opacity);
}
