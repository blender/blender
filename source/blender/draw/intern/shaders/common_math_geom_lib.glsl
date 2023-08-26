/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Math intersection & projection functions.
 * \{ */

vec4 plane_from_quad(vec3 v0, vec3 v1, vec3 v2, vec3 v3)
{
  vec3 nor = normalize(cross(v2 - v1, v0 - v1) + cross(v0 - v3, v2 - v3));
  return vec4(nor, -dot(nor, v2));
}

vec4 plane_from_tri(vec3 v0, vec3 v1, vec3 v2)
{
  vec3 nor = normalize(cross(v2 - v1, v0 - v1));
  return vec4(nor, -dot(nor, v2));
}

float point_plane_projection_dist(vec3 line_origin, vec3 plane_origin, vec3 plane_normal)
{
  return dot(plane_normal, plane_origin - line_origin);
}

float point_line_projection_dist(vec2 point, vec2 line_origin, vec2 line_normal)
{
  return dot(line_normal, line_origin - point);
}

float line_plane_intersect_dist(vec3 line_origin,
                                vec3 line_direction,
                                vec3 plane_origin,
                                vec3 plane_normal)
{
  return dot(plane_normal, plane_origin - line_origin) / dot(plane_normal, line_direction);
}

float line_plane_intersect_dist(vec3 line_origin, vec3 line_direction, vec4 plane)
{
  vec3 plane_co = plane.xyz * (-plane.w / len_squared(plane.xyz));
  vec3 h = line_origin - plane_co;
  return -dot(plane.xyz, h) / dot(plane.xyz, line_direction);
}

vec3 line_plane_intersect(vec3 line_origin,
                          vec3 line_direction,
                          vec3 plane_origin,
                          vec3 plane_normal)
{
  float dist = line_plane_intersect_dist(line_origin, line_direction, plane_origin, plane_normal);
  return line_origin + line_direction * dist;
}

vec3 line_plane_intersect(vec3 line_origin, vec3 line_direction, vec4 plane)
{
  float dist = line_plane_intersect_dist(line_origin, line_direction, plane);
  return line_origin + line_direction * dist;
}

float line_aligned_plane_intersect_dist(vec3 line_origin, vec3 line_direction, vec3 plane_origin)
{
  /* aligned plane normal */
  vec3 L = plane_origin - line_origin;
  float disk_dist = length(L);
  vec3 plane_normal = -normalize(L);
  return -disk_dist / dot(plane_normal, line_direction);
}

vec3 line_aligned_plane_intersect(vec3 line_origin, vec3 line_direction, vec3 plane_origin)
{
  float dist = line_aligned_plane_intersect_dist(line_origin, line_direction, plane_origin);
  if (dist < 0) {
    /* if intersection is behind we fake the intersection to be
     * really far and (hopefully) not inside the radius of interest */
    dist = 1e16;
  }
  return line_origin + line_direction * dist;
}

/**
 * Returns intersection distance between the unit sphere and the line
 * with the assumption that \a line_origin is contained in the unit sphere.
 * It will always returns the farthest intersection.
 */
float line_unit_sphere_intersect_dist(vec3 line_origin, vec3 line_direction)
{
  float a = dot(line_direction, line_direction);
  float b = dot(line_direction, line_origin);
  float c = dot(line_origin, line_origin) - 1;

  float dist = 1e15;
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
float line_unit_box_intersect_dist(vec3 line_origin, vec3 line_direction)
{
  /* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
   */
  vec3 first_plane = (vec3(1.0) - line_origin) / line_direction;
  vec3 second_plane = (vec3(-1.0) - line_origin) / line_direction;
  vec3 farthest_plane = max(first_plane, second_plane);

  return min_v3(farthest_plane);
}

float line_unit_box_intersect_dist_safe(vec3 line_origin, vec3 line_direction)
{
  vec3 safe_line_direction = max(vec3(1e-8), abs(line_direction)) *
                             select(vec3(1.0), -vec3(1.0), lessThan(line_direction, vec3(0.0)));
  return line_unit_box_intersect_dist(line_origin, safe_line_direction);
}

/**
 * Same as line_unit_box_intersect_dist but for 2D case.
 */
float line_unit_square_intersect_dist(vec2 line_origin, vec2 line_direction)
{
  vec2 first_plane = (vec2(1.0) - line_origin) / line_direction;
  vec2 second_plane = (vec2(-1.0) - line_origin) / line_direction;
  vec2 farthest_plane = max(first_plane, second_plane);

  return min_v2(farthest_plane);
}

float line_unit_square_intersect_dist_safe(vec2 line_origin, vec2 line_direction)
{
  vec2 safe_line_direction = max(vec2(1e-8), abs(line_direction)) *
                             select(vec2(1.0), -vec2(1.0), lessThan(line_direction, vec2(0.0)));
  return line_unit_square_intersect_dist(line_origin, safe_line_direction);
}

/**
 * Returns clipping distance (intersection with the nearest plane) with the given axis-aligned
 * bound box along \a line_direction.
 * Safe even if \a line_direction is degenerate.
 * It assumes that an intersection exists (i.e: that \a line_direction points towards the AABB).
 */
float line_aabb_clipping_dist(vec3 line_origin, vec3 line_direction, vec3 aabb_min, vec3 aabb_max)
{
  vec3 safe_dir = select(line_direction, vec3(1e-5), lessThan(abs(line_direction), vec3(1e-5)));
  vec3 dir_inv = 1.0 / safe_dir;

  vec3 first_plane = (aabb_min - line_origin) * dir_inv;
  vec3 second_plane = (aabb_max - line_origin) * dir_inv;
  vec3 nearest_plane = min(first_plane, second_plane);
  return max_v3(nearest_plane);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Other useful functions.
 * \{ */

void make_orthonormal_basis(vec3 N, out vec3 T, out vec3 B)
{
  vec3 up_vector = abs(N.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  T = normalize(cross(up_vector, N));
  B = cross(N, T);
}

/* ---- Encode / Decode Normal buffer data ---- */
/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec2 normal_encode(vec3 n, vec3 view)
{
  float p = sqrt(n.z * 8.0 + 8.0);
  return n.xy / p + 0.5;
}

vec3 normal_decode(vec2 enc, vec3 view)
{
  vec2 fenc = enc * 4.0 - 2.0;
  float f = dot(fenc, fenc);
  float g = sqrt(1.0 - f / 4.0);
  vec3 n;
  n.xy = fenc * g;
  n.z = 1 - f / 2;
  return n;
}

vec3 tangent_to_world(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return T * vector.x + B * vector.y + N * vector.z;
}

vec3 world_to_tangent(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return vec3(dot(T, vector), dot(B, vector), dot(N, vector));
}

/** \} */
