/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_node_socket_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_node_socket_inst)

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

/* Values in `eNodeSocketDisplayShape` in DNA_node_types.h. Keep in sync. */
#define SOCK_DISPLAY_SHAPE_CIRCLE 0
#define SOCK_DISPLAY_SHAPE_SQUARE 1
#define SOCK_DISPLAY_SHAPE_DIAMOND 2
#define SOCK_DISPLAY_SHAPE_CIRCLE_DOT 3
#define SOCK_DISPLAY_SHAPE_SQUARE_DOT 4
#define SOCK_DISPLAY_SHAPE_DIAMOND_DOT 5
#define SOCK_DISPLAY_SHAPE_LINE 6
#define SOCK_DISPLAY_SHAPE_VOLUME_GRID 7
#define SOCK_DISPLAY_SHAPE_LIST 8

/* Calculates a squared distance field of a square. */
float square_sdf(float2 absCo, float2 half_size)
{
  float2 extruded_co = absCo - half_size;
  float2 clamped_extruded_co = float2(max(0.0f, extruded_co.x), max(0.0f, extruded_co.y));

  float exterior_distance_squared = dot(clamped_extruded_co, clamped_extruded_co);

  float interior_distance = min(max(extruded_co.x, extruded_co.y), 0.0f);
  float interior_distance_squared = interior_distance * interior_distance;

  return exterior_distance_squared - interior_distance_squared;
}

float2 rotate_45(float2 co)
{
  return from_rotation(AngleRadian(M_PI * 0.25f)) * co;
}

/* Calculates an upper and lower limit for an anti-aliased cutoff of the squared distance. */
float2 calculate_thresholds(float threshold)
{
  /* Use the absolute on one of the factors to preserve the sign. */
  float inner_threshold = (threshold - 0.5f * AAsize) * abs(threshold - 0.5f * AAsize);
  float outer_threshold = (threshold + 0.5f * AAsize) * abs(threshold + 0.5f * AAsize);
  return float2(inner_threshold, outer_threshold);
}

void main()
{
  float2 absUV = abs(uv);
  float2 co = float2(max(absUV.x - extrusion.x, 0.0f), max(absUV.y - extrusion.y, 0.0f));

  float distance_squared = 0.0f;
  float alpha_threshold = 0.0f;
  float dot_threshold = -1.0f;

  constexpr float circle_radius = 0.5f;
  const float square_radius = 0.5f / sqrt(2.0f / M_PI) * M_SQRT1_2;
  const float diamond_radius = 0.5f / sqrt(2.0f / M_PI) * M_SQRT1_2;
  constexpr float corner_rounding = 0.0f;

  switch (finalShape) {
    default:
    case SOCK_DISPLAY_SHAPE_CIRCLE: {
      distance_squared = dot(co, co);
      alpha_threshold = circle_radius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_CIRCLE_DOT: {
      distance_squared = dot(co, co);
      alpha_threshold = circle_radius;
      dot_threshold = finalDotRadius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_SQUARE: {
      distance_squared = square_sdf(co, float2(square_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_SQUARE_DOT: {
      distance_squared = square_sdf(co, float2(square_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      dot_threshold = finalDotRadius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND: {
      distance_squared = square_sdf(abs(rotate_45(co)), float2(diamond_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND_DOT: {
      distance_squared = square_sdf(abs(rotate_45(co)), float2(diamond_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      dot_threshold = finalDotRadius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_LINE: {
      distance_squared = square_sdf(co, float2(square_radius * 0.75, square_radius * 1.4));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_VOLUME_GRID: {
      constexpr float rect_side_length = 0.25f;
      const float2 oversize = float2(0.0f, square_radius * 1.4) / 2.5f;
      const float2 rect_corner = max(float2(rect_side_length), extrusion / 2.0f + oversize) +
                                 finalOutlineThickness / 4.0f;
      const float2 mirrored_uv = abs(abs(uv) - rect_corner);
      distance_squared = square_sdf(mirrored_uv, rect_corner + finalOutlineThickness / 2.0f);
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_LIST: {
      constexpr float2 rect_side_length = float2(0.5f, 0.25f);
      const float2 oversize = float2(0.0f, square_radius * 1.4) / 2.5f;
      const float2 rect_corner = max(rect_side_length, extrusion / 2.0f + oversize) +
                                 finalOutlineThickness / 4.0f;
      const float2 mirrored_uv = float2(
          abs(uv.x), abs(abs(abs(uv.y) - rect_corner.y / 1.5f) - rect_corner.y / 1.5f));
      distance_squared = square_sdf(
          mirrored_uv, (rect_corner + finalOutlineThickness / 2.0f) / float2(1.0f, 1.5f));
      break;
    }
  }

  float2 alpha_thresholds = calculate_thresholds(alpha_threshold);
  float2 outline_thresholds = calculate_thresholds(alpha_threshold - finalOutlineThickness);
  float2 dot_thresholds = calculate_thresholds(dot_threshold);

  float alpha_mask = smoothstep(alpha_thresholds[1], alpha_thresholds[0], distance_squared);
  float dot_mask = smoothstep(dot_thresholds[1], dot_thresholds[0], dot(co, co));
  float outline_mask = smoothstep(outline_thresholds[0], outline_thresholds[1], distance_squared) +
                       dot_mask;

  fragColor = mix(finalColor, finalOutlineColor, outline_mask);
  fragColor.a *= alpha_mask;
}
