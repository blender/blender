/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_stretching_area)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

float3 weight_to_rgb(float weight)
{
  float3 rgb;
  float blend = ((weight / 2.0f) + 0.5f);

  if (weight <= 0.25f) { /* blue->cyan */
    rgb[0] = 0.0f;
    rgb[1] = blend * weight * 4.0f;
    rgb[2] = blend;
  }
  else if (weight <= 0.50f) { /* cyan->green */
    rgb[0] = 0.0f;
    rgb[1] = blend;
    rgb[2] = blend * (1.0f - ((weight - 0.25f) * 4.0f));
  }
  else if (weight <= 0.75f) { /* green->yellow */
    rgb[0] = blend * ((weight - 0.50f) * 4.0f);
    rgb[1] = blend;
    rgb[2] = 0.0f;
  }
  else if (weight <= 1.0f) { /* yellow->red */
    rgb[0] = blend;
    rgb[1] = blend * (1.0f - ((weight - 0.75f) * 4.0f));
    rgb[2] = 0.0f;
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    rgb[0] = 1.0f;
    rgb[1] = 0.0f;
    rgb[2] = 1.0f;
  }

  return rgb;
}

#define M_PI 3.1415926535897932f

float2 angle_to_v2(float angle)
{
  return float2(cos(angle), sin(angle));
}

/* Adapted from BLI_math_vector.h */
float angle_normalized_v2v2(float2 v1, float2 v2)
{
  v1 = normalize(v1 * aspect);
  v2 = normalize(v2 * aspect);
  /* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
  bool q = (dot(v1, v2) >= 0.0f);
  float2 v = (q) ? (v1 - v2) : (v1 + v2);
  float a = 2.0f * asin(length(v) / 2.0f);
  return (q) ? a : M_PI - a;
}

float area_ratio_to_stretch(float ratio, float tot_ratio)
{
  ratio *= tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

void main()
{
  float3 world_pos = float3(pos, 0.0f);
  gl_Position = drw_point_world_to_homogenous(world_pos);

#ifdef STRETCH_ANGLE
  float2 v1 = angle_to_v2(uv_angles.x * M_PI);
  float2 v2 = angle_to_v2(uv_angles.y * M_PI);
  float uv_angle = angle_normalized_v2v2(v1, v2) / M_PI;
  float stretch = 1.0f - abs(uv_angle - angle);
  stretch = stretch;
  stretch = 1.0f - stretch * stretch;
#else
  float stretch = 1.0f - area_ratio_to_stretch(ratio, total_area_ratio);

#endif

  final_color = float4(weight_to_rgb(stretch), stretch_opacity);
}
