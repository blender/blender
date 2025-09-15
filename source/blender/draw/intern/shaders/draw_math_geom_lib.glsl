/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * This should ultimately be rewritten inside a gpu_shader lib with higher quality standard.
 */

#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

/* ---------------------------------------------------------------------- */
/** \name Math intersection & projection functions.
 * \{ */

float4 plane_from_quad(float3 v0, float3 v1, float3 v2, float3 v3)
{
  float3 nor = normalize(cross(v2 - v1, v0 - v1) + cross(v0 - v3, v2 - v3));
  return float4(nor, -dot(nor, v2));
}

float4 plane_from_tri(float3 v0, float3 v1, float3 v2)
{
  float3 nor = normalize(cross(v2 - v1, v0 - v1));
  return float4(nor, -dot(nor, v2));
}

float point_plane_projection_dist(float3 line_origin, float3 plane_origin, float3 plane_normal)
{
  return dot(plane_normal, plane_origin - line_origin);
}

float point_line_projection_dist(float2 point, float2 line_origin, float2 line_normal)
{
  return dot(line_normal, line_origin - point);
}

float line_plane_intersect_dist(float3 line_origin,
                                float3 line_direction,
                                float3 plane_origin,
                                float3 plane_normal)
{
  return dot(plane_normal, plane_origin - line_origin) / dot(plane_normal, line_direction);
}

float line_plane_intersect_dist(float3 line_origin, float3 line_direction, float4 plane)
{
  float3 plane_co = plane.xyz * (-plane.w / length_squared(plane.xyz));
  float3 h = line_origin - plane_co;
  return -dot(plane.xyz, h) / dot(plane.xyz, line_direction);
}

float3 line_plane_intersect(float3 line_origin,
                            float3 line_direction,
                            float3 plane_origin,
                            float3 plane_normal)
{
  float dist = line_plane_intersect_dist(line_origin, line_direction, plane_origin, plane_normal);
  return line_origin + line_direction * dist;
}

float3 line_plane_intersect(float3 line_origin, float3 line_direction, float4 plane)
{
  float dist = line_plane_intersect_dist(line_origin, line_direction, plane);
  return line_origin + line_direction * dist;
}

float line_aligned_plane_intersect_dist(float3 line_origin,
                                        float3 line_direction,
                                        float3 plane_origin)
{
  /* aligned plane normal */
  float3 L = plane_origin - line_origin;
  float disk_dist = length(L);
  float3 plane_normal = -normalize(L);
  return -disk_dist / dot(plane_normal, line_direction);
}

float3 line_aligned_plane_intersect(float3 line_origin, float3 line_direction, float3 plane_origin)
{
  float dist = line_aligned_plane_intersect_dist(line_origin, line_direction, plane_origin);
  if (dist < 0) {
    /* if intersection is behind we fake the intersection to be
     * really far and (hopefully) not inside the radius of interest */
    dist = 1e16f;
  }
  return line_origin + line_direction * dist;
}

/**
 * Returns intersection distance between the unit sphere and the line
 * with the assumption that \a line_origin is contained in the unit sphere.
 * It will always returns the farthest intersection.
 */
float line_unit_sphere_intersect_dist(float3 line_origin, float3 line_direction)
{
  float a = dot(line_direction, line_direction);
  float b = dot(line_direction, line_origin);
  float c = dot(line_origin, line_origin) - 1;

  float dist = 1e15f;
  float determinant = b * b - a * c;
  if (determinant >= 0) {
    dist = (sqrt(determinant) - b) / a;
  }

  return dist;
}

/**
 * Returns minimum intersection distance between the unit box and the line
 * with the assumption that \a line_origin is contained in the unit box.
 * In other words, it will always returns the farthest intersection.
 */
float line_unit_box_intersect_dist(float3 line_origin, float3 line_direction)
{
  /* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
   */
  float3 first_plane = (float3(1.0f) - line_origin) / line_direction;
  float3 second_plane = (float3(-1.0f) - line_origin) / line_direction;
  float3 farthest_plane = max(first_plane, second_plane);
  return reduce_min(farthest_plane);
}

float line_unit_box_intersect_dist_safe(float3 line_origin, float3 line_direction)
{
  float3 safe_line_direction = max(float3(1e-8f), abs(line_direction)) *
                               select(float3(1.0f),
                                      -float3(1.0f),
                                      lessThan(line_direction, float3(0.0f)));
  return line_unit_box_intersect_dist(line_origin, safe_line_direction);
}

/**
 * Same as line_unit_box_intersect_dist but for 2D case.
 */
float line_unit_square_intersect_dist(float2 line_origin, float2 line_direction)
{
  float2 first_plane = (float2(1.0f) - line_origin) / line_direction;
  float2 second_plane = (float2(-1.0f) - line_origin) / line_direction;
  float2 farthest_plane = max(first_plane, second_plane);
  return reduce_min(farthest_plane);
}

float line_unit_square_intersect_dist_safe(float2 line_origin, float2 line_direction)
{
  float2 safe_line_direction = max(float2(1e-8f), abs(line_direction)) *
                               select(float2(1.0f),
                                      -float2(1.0f),
                                      lessThan(line_direction, float2(0.0f)));
  return line_unit_square_intersect_dist(line_origin, safe_line_direction);
}

/**
 * Returns clipping distance (intersection with the nearest plane) with the given axis-aligned
 * bound box along \a line_direction.
 * Safe even if \a line_direction is degenerate.
 * It assumes that an intersection exists (i.e: that \a line_direction points towards the AABB).
 */
float line_aabb_clipping_dist(float3 line_origin,
                              float3 line_direction,
                              float3 aabb_min,
                              float3 aabb_max)
{
  float3 safe_dir = select(
      line_direction, float3(1e-5f), lessThan(abs(line_direction), float3(1e-5f)));
  float3 dir_inv = 1.0f / safe_dir;

  float3 first_plane = (aabb_min - line_origin) * dir_inv;
  float3 second_plane = (aabb_max - line_origin) * dir_inv;
  float3 nearest_plane = min(first_plane, second_plane);
  return reduce_max(nearest_plane);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Other useful functions.
 * \{ */

void make_orthonormal_basis(float3 N, out float3 T, out float3 B)
{
  float3 up_vector = abs(N.z) < 0.99999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
  T = normalize(cross(up_vector, N));
  B = cross(N, T);
}

/** \} */
