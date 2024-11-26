/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_node_socket_info.hh"

#include "gpu_shader_math_matrix_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_node_socket_inst)

/* Values in `eNodeSocketDisplayShape` in DNA_node_types.h. Keep in sync. */
#define SOCK_DISPLAY_SHAPE_CIRCLE 0
#define SOCK_DISPLAY_SHAPE_SQUARE 1
#define SOCK_DISPLAY_SHAPE_DIAMOND 2
#define SOCK_DISPLAY_SHAPE_CIRCLE_DOT 3
#define SOCK_DISPLAY_SHAPE_SQUARE_DOT 4
#define SOCK_DISPLAY_SHAPE_DIAMOND_DOT 5

/* Calculates a squared distance field of a square. */
float square_sdf(vec2 absCo, float half_width_x, float half_width_y)
{
  vec2 extruded_co = absCo - vec2(half_width_x, half_width_y);
  vec2 clamped_extruded_co = vec2(max(0.0, extruded_co.x), max(0.0, extruded_co.y));

  float exterior_distance_squared = dot(clamped_extruded_co, clamped_extruded_co);

  float interior_distance = min(max(extruded_co.x, extruded_co.y), 0.0);
  float interior_distance_squared = interior_distance * interior_distance;

  return exterior_distance_squared - interior_distance_squared;
}

vec2 rotate_45(vec2 co)
{
  return from_rotation(Angle(M_PI * 0.25)) * co;
}

/* Calculates an upper and lower limit for an anti-aliased cutoff of the squared distance. */
vec2 calculate_thresholds(float threshold)
{
  /* Use the absolute on one of the factors to preserve the sign. */
  float inner_threshold = (threshold - 0.5 * AAsize) * abs(threshold - 0.5 * AAsize);
  float outer_threshold = (threshold + 0.5 * AAsize) * abs(threshold + 0.5 * AAsize);
  return vec2(inner_threshold, outer_threshold);
}

void main()
{
  vec2 absUV = abs(uv);
  vec2 co = vec2(max(absUV.x - extrusion.x, 0.0), max(absUV.y - extrusion.y, 0.0));

  float distance_squared = 0.0;
  float alpha_threshold = 0.0;
  float dot_threshold = -1.0;

  const float circle_radius = 0.5;
  const float square_radius = 0.5 / sqrt(2.0 / M_PI) * M_SQRT1_2;
  const float diamond_radius = 0.5 / sqrt(2.0 / M_PI) * M_SQRT1_2;
  const float corner_rounding = 0.0;

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
      float square_radius = square_radius - corner_rounding;
      distance_squared = square_sdf(co, square_radius, square_radius);
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_SQUARE_DOT: {
      float square_radius = square_radius - corner_rounding;
      distance_squared = square_sdf(co, square_radius, square_radius);
      alpha_threshold = corner_rounding;
      dot_threshold = finalDotRadius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND: {
      float diamond_radius = diamond_radius - corner_rounding;
      distance_squared = square_sdf(abs(rotate_45(co)), diamond_radius, diamond_radius);
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND_DOT: {
      float diamond_radius = diamond_radius - corner_rounding;
      distance_squared = square_sdf(abs(rotate_45(co)), diamond_radius, diamond_radius);
      alpha_threshold = corner_rounding;
      dot_threshold = finalDotRadius;
      break;
    }
  }

  vec2 alpha_thresholds = calculate_thresholds(alpha_threshold);
  vec2 outline_thresholds = calculate_thresholds(alpha_threshold - finalOutlineThickness);
  vec2 dot_thresholds = calculate_thresholds(dot_threshold);

  float alpha_mask = smoothstep(alpha_thresholds[1], alpha_thresholds[0], distance_squared);
  float dot_mask = smoothstep(dot_thresholds[1], dot_thresholds[0], dot(co, co));
  float outline_mask = smoothstep(outline_thresholds[0], outline_thresholds[1], distance_squared) +
                       dot_mask;

  fragColor = mix(finalColor, finalOutlineColor, outline_mask);
  fragColor.a *= alpha_mask;
}
