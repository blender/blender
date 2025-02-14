/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_float2.h"
#include "util/math_float3.h"
#include "util/math_float4.h"

CCL_NAMESPACE_BEGIN

/* Ray Intersection */

ccl_device bool ray_sphere_intersect(const float3 ray_P,
                                     const float3 ray_D,
                                     const float ray_tmin,
                                     const float ray_tmax,
                                     const float3 sphere_P,
                                     const float sphere_radius,
                                     ccl_private float3 *isect_P,
                                     ccl_private float *isect_t)
{
  const float3 d_vec = sphere_P - ray_P;
  const float r_sq = sphere_radius * sphere_radius;
  const float d_sq = dot(d_vec, d_vec);
  const float d_cos_theta = dot(d_vec, ray_D);

  if (d_sq > r_sq && d_cos_theta < 0.0f) {
    /* Ray origin outside sphere and points away from sphere. */
    return false;
  }

  const float d_sin_theta_sq = len_squared(d_vec - d_cos_theta * ray_D);

  if (d_sin_theta_sq > r_sq) {
    /* Closest point on ray outside sphere. */
    return false;
  }

  /* Law of cosines. */
  const float t = d_cos_theta - copysignf(sqrtf(r_sq - d_sin_theta_sq), d_sq - r_sq);

  if (t > ray_tmin && t < ray_tmax) {
    *isect_t = t;
    *isect_P = ray_P + ray_D * t;
    return true;
  }

  return false;
}

ccl_device bool ray_aligned_disk_intersect(const float3 ray_P,
                                           const float3 ray_D,
                                           const float ray_tmin,
                                           const float ray_tmax,
                                           const float3 disk_P,
                                           const float disk_radius,
                                           ccl_private float3 *isect_P,
                                           ccl_private float *isect_t)
{
  /* Aligned disk normal. */
  float disk_t;
  const float3 disk_N = normalize_len(ray_P - disk_P, &disk_t);
  const float div = dot(ray_D, disk_N);
  if (UNLIKELY(div == 0.0f)) {
    return false;
  }
  /* Compute t to intersection point. */
  const float t = -disk_t / div;
  if (!(t > ray_tmin && t < ray_tmax)) {
    return false;
  }
  /* Test if within radius. */
  const float3 P = ray_P + ray_D * t;
  if (len_squared(P - disk_P) > disk_radius * disk_radius) {
    return false;
  }
  *isect_P = P;
  *isect_t = t;
  return true;
}

ccl_device bool ray_disk_intersect(const float3 ray_P,
                                   const float3 ray_D,
                                   const float ray_tmin,
                                   const float ray_tmax,
                                   const float3 disk_P,
                                   const float3 disk_N,
                                   const float disk_radius,
                                   ccl_private float3 *isect_P,
                                   ccl_private float *isect_t)
{
  const float3 vp = ray_P - disk_P;
  const float dp = dot(vp, disk_N);
  const float cos_angle = dot(disk_N, -ray_D);
  if (dp * cos_angle > 0.f)  // front of light
  {
    const float t = dp / cos_angle;
    if (t < 0.f) { /* Ray points away from the light. */
      return false;
    }
    const float3 P = ray_P + t * ray_D;
    const float3 T = P - disk_P;

    if (dot(T, T) < sqr(disk_radius) && (t > ray_tmin && t < ray_tmax)) {
      *isect_P = ray_P + t * ray_D;
      *isect_t = t;
      return true;
    }
  }
  return false;
}

/* Custom rcp, cross and dot implementations that match Embree bit for bit. */
ccl_device_forceinline float ray_triangle_reciprocal(const float x)
{
#ifdef __KERNEL_NEON__
  /* Move scalar to vector register and do rcp. */
  __m128 a = {0};
  a = vsetq_lane_f32(x, a, 0);
  float32x4_t rt_rcp = vrecpeq_f32(a);
  rt_rcp = vmulq_f32(vrecpsq_f32(a, rt_rcp), rt_rcp);
  rt_rcp = vmulq_f32(vrecpsq_f32(a, rt_rcp), rt_rcp);
  return vgetq_lane_f32(rt_rcp, 0);
#elif defined(__KERNEL_SSE__)
  const __m128 a = _mm_set_ss(x);
  const __m128 r = _mm_rcp_ss(a);

#  ifdef __KERNEL_AVX2_
  return _mm_cvtss_f32(_mm_mul_ss(r, _mm_fnmadd_ss(r, a, _mm_set_ss(2.0f))));
#  else
  return _mm_cvtss_f32(_mm_mul_ss(r, _mm_sub_ss(_mm_set_ss(2.0f), _mm_mul_ss(r, a))));
#  endif
#else
  return 1.0f / x;
#endif
}

ccl_device_inline float ray_triangle_dot(const float3 a, const float3 b)
{
#if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  return madd(make_float4(a.x),
              make_float4(b.x),
              madd(make_float4(a.y), make_float4(b.y), make_float4(a.z) * make_float4(b.z)))[0];
#else
  return a.x * b.x + a.y * b.y + a.z * b.z;
#endif
}

ccl_device_inline float3 ray_triangle_cross(const float3 a, const float3 b)
{
#if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  return make_float3(
      msub(make_float4(a.y), make_float4(b.z), make_float4(a.z) * make_float4(b.y))[0],
      msub(make_float4(a.z), make_float4(b.x), make_float4(a.x) * make_float4(b.z))[0],
      msub(make_float4(a.x), make_float4(b.y), make_float4(a.y) * make_float4(b.x))[0]);
#else
  return make_float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
#endif
}

ccl_device_forceinline bool ray_triangle_intersect(const float3 ray_P,
                                                   const float3 ray_D,
                                                   const float ray_tmin,
                                                   const float ray_tmax,
                                                   const float3 tri_a,
                                                   const float3 tri_b,
                                                   const float3 tri_c,
                                                   ccl_private float *isect_u,
                                                   ccl_private float *isect_v,
                                                   ccl_private float *isect_t)
{
  /* This implementation matches the Plücker coordinates triangle intersection
   * in Embree. */

  /* Calculate vertices relative to ray origin. */
  const float3 v0 = tri_a - ray_P;
  const float3 v1 = tri_b - ray_P;
  const float3 v2 = tri_c - ray_P;

  /* Calculate triangle edges. */
  const float3 e0 = v2 - v0;
  const float3 e1 = v0 - v1;
  const float3 e2 = v1 - v2;

  /* Perform edge tests. */
  const float U = ray_triangle_dot(ray_triangle_cross(e0, v2 + v0), ray_D);
  const float V = ray_triangle_dot(ray_triangle_cross(e1, v0 + v1), ray_D);
  const float W = ray_triangle_dot(ray_triangle_cross(e2, v1 + v2), ray_D);

  const float UVW = U + V + W;
  const float eps = FLT_EPSILON * fabsf(UVW);
  const float minUVW = min(U, min(V, W));
  const float maxUVW = max(U, max(V, W));

  if (!(minUVW >= -eps || maxUVW <= eps)) {
    return false;
  }

  /* Calculate geometry normal and denominator. */
  const float3 Ng1 = ray_triangle_cross(e1, e0);
  const float3 Ng = Ng1 + Ng1;
  const float den = dot(Ng, ray_D);
  /* Avoid division by 0. */
  if (UNLIKELY(den == 0.0f)) {
    return false;
  }

  /* Perform depth test. */
  const float T = dot(v0, Ng);
  const float t = T / den;
  if (!(t >= ray_tmin && t <= ray_tmax)) {
    return false;
  }

  const float rcp_uvw = (fabsf(UVW) < 1e-18f) ? 0.0f : ray_triangle_reciprocal(UVW);
  *isect_u = min(U * rcp_uvw, 1.0f);
  *isect_v = min(V * rcp_uvw, 1.0f);
  *isect_t = t;
  return true;
}

ccl_device_forceinline bool ray_triangle_intersect_self(const float3 ray_P,
                                                        const float3 ray_D,
                                                        const float3 verts[3])
{
  /* Matches logic in ray_triangle_intersect, self intersection test to validate
   * if a ray is going to hit self or might incorrectly hit a neighboring triangle. */

  /* Calculate vertices relative to ray origin. */
  const float3 v0 = verts[0] - ray_P;
  const float3 v1 = verts[1] - ray_P;
  const float3 v2 = verts[2] - ray_P;

  /* Calculate triangle edges. */
  const float3 e0 = v2 - v0;
  const float3 e1 = v0 - v1;
  const float3 e2 = v1 - v2;

  /* Perform edge tests. */
  const float U = ray_triangle_dot(ray_triangle_cross(v2 + v0, e0), ray_D);
  const float V = ray_triangle_dot(ray_triangle_cross(v0 + v1, e1), ray_D);
  const float W = ray_triangle_dot(ray_triangle_cross(v1 + v2, e2), ray_D);

  const float eps = FLT_EPSILON * fabsf(U + V + W);
  const float minUVW = min(U, min(V, W));
  const float maxUVW = max(U, max(V, W));

  /* Note the extended epsilon compared to ray_triangle_intersect, to account
   * for intersections with neighboring triangles that have an epsilon. */
  return (minUVW >= eps || maxUVW <= -eps);
}

/* Tests for an intersection between a ray and a quad defined by
 * its midpoint, normal and sides.
 * If ellipse is true, hits outside the ellipse that's enclosed by the
 * quad are rejected.
 */
ccl_device bool ray_quad_intersect(const float3 ray_P,
                                   const float3 ray_D,
                                   const float ray_tmin,
                                   const float ray_tmax,
                                   const float3 quad_P,
                                   const float3 inv_quad_u,
                                   const float3 inv_quad_v,
                                   const float3 quad_n,
                                   ccl_private float3 *isect_P,
                                   ccl_private float *isect_t,
                                   ccl_private float *isect_u,
                                   ccl_private float *isect_v,
                                   bool ellipse)
{
  /* Perform intersection test. */
  const float t = -(dot(ray_P, quad_n) - dot(quad_P, quad_n)) / dot(ray_D, quad_n);
  if (!(t > ray_tmin && t < ray_tmax)) {
    return false;
  }
  const float3 hit = ray_P + t * ray_D;
  const float3 inplane = hit - quad_P;
  const float u = dot(inplane, inv_quad_u);
  if (u < -0.5f || u > 0.5f) {
    return false;
  }
  const float v = dot(inplane, inv_quad_v);
  if (v < -0.5f || v > 0.5f) {
    return false;
  }
  if (ellipse && (u * u + v * v > 0.25f)) {
    return false;
  }
  /* Store the result. */
  /* TODO(sergey): Check whether we can avoid some checks here. */
  if (isect_P != nullptr) {
    *isect_P = hit;
  }
  if (isect_t != nullptr) {
    *isect_t = t;
  }

  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  if (isect_u != nullptr) {
    *isect_u = v + 0.5f;
  }
  if (isect_v != nullptr) {
    *isect_v = -u - v;
  }

  return true;
}

/* Find the ray segment that lies in the same side as the normal `N` of the plane.
 * `P` is the vector pointing from any point on the plane to the ray origin. */
ccl_device bool ray_plane_intersect(const float3 N,
                                    const float3 P,
                                    const float3 ray_D,
                                    ccl_private Interval<float> *t_range)
{
  const float DN = dot(ray_D, N);

  /* Distance from P to the plane. */
  const float t = -dot(P, N) / DN;

  /* Limit the range to the positive side. */
  if (DN > 0.0f) {
    t_range->min = fmaxf(t_range->min, t);
  }
  else {
    t_range->max = fminf(t_range->max, t);
  }

  return !t_range->is_empty();
}

/* Find the ray segment inside an axis-aligned bounding box. */
ccl_device bool ray_aabb_intersect(const float3 bbox_min,
                                   const float3 bbox_max,
                                   const float3 ray_P,
                                   const float3 ray_D,
                                   ccl_private Interval<float> *t_range)
{
  const float3 inv_ray_D = reciprocal(ray_D);

  /* Absolute distances to lower and upper box coordinates; */
  const float3 t_lower = (bbox_min - ray_P) * inv_ray_D;
  const float3 t_upper = (bbox_max - ray_P) * inv_ray_D;

  /* The four t-intervals (for x-/y-/z-slabs, and ray p(t)). */
  const float4 tmins = make_float4(min(t_lower, t_upper), t_range->min);
  const float4 tmaxes = make_float4(max(t_lower, t_upper), t_range->max);

  /* Max of mins and min of maxes. */
  const float tmin = reduce_max(tmins);
  const float tmax = reduce_min(tmaxes);

  *t_range = {tmin, tmax};

  return !t_range->is_empty();
}

/* Find the segment of a ray defined by P + D * t that lies inside a cylinder defined by
 * (x / len_u)^2 + (y / len_v)^2 = 1. */
ccl_device_inline bool ray_infinite_cylinder_intersect(const float3 P,
                                                       const float3 D,
                                                       const float len_u,
                                                       const float len_v,
                                                       ccl_private Interval<float> *t_range)
{
  /* Convert to a 2D problem. */
  const float2 inv_len = 1.0f / make_float2(len_u, len_v);
  float2 P_proj = make_float2(P) * inv_len;
  const float2 D_proj = make_float2(D) * inv_len;

  /* Solve quadratic equation a*t^2 + 2b*t + c = 0. */
  const float a = dot(D_proj, D_proj);
  float b = dot(P_proj, D_proj);

  /* Move ray origin closer to the cylinder to prevent precision issue when the ray is far away. */
  const float t_mid = -b / a;
  P_proj += D_proj * t_mid;

  /* Recompute b from the shifted origin. */
  b = dot(P_proj, D_proj);
  const float c = dot(P_proj, P_proj) - 1.0f;

  float tmin;
  float tmax;
  const bool valid = solve_quadratic(a, 2.0f * b, c, tmin, tmax);

  *t_range = intervals_intersection(*t_range, {tmin + t_mid, tmax + t_mid});

  return valid && !t_range->is_empty();
}

/* *
 * Find the ray segment inside a single-sided cone.
 *
 * \param axis: a unit-length direction around which the cone has a circular symmetry
 * \param P: the vector pointing from the cone apex to the ray origin
 * \param D: the direction of the ray, does not need to have unit-length
 * \param cos_angle_sq: `sqr(cos(half_aperture_of_the_cone))`
 * \param t_range: the ray segment that lies inside the cone
 * \return whether the intersection exists and is in the provided range
 *
 * See https://www.geometrictools.com/Documentation/IntersectionLineCone.pdf for illustration
 */
ccl_device_inline bool ray_cone_intersect(const float3 axis,
                                          const float3 P,
                                          float3 D,
                                          const float cos_angle_sq,
                                          ccl_private Interval<float> *t_range)
{
  if (cos_angle_sq < 1e-4f) {
    /* The cone is nearly a plane. */
    return ray_plane_intersect(axis, P, D, t_range);
  }

  const float inv_len = inversesqrtf(len_squared(D));
  D *= inv_len;

  const float AD = dot(axis, D);
  const float AP = dot(axis, P);

  const float a = sqr(AD) - cos_angle_sq;
  const float b = 2.0f * (AD * AP - cos_angle_sq * dot(D, P));
  const float c = sqr(AP) - cos_angle_sq * dot(P, P);

  float tmin = 0.0f;
  float tmax = FLT_MAX;
  bool valid = solve_quadratic(a, b, c, tmin, tmax);

  /* Check if the intersections are in the same hemisphere as the cone. */
  const bool tmin_valid = AP + tmin * AD > 0.0f;
  const bool tmax_valid = AP + tmax * AD > 0.0f;

  valid &= (tmin_valid || tmax_valid);

  if (!tmax_valid) {
    tmax = tmin;
    tmin = 0.0f;
  }
  else if (!tmin_valid) {
    tmin = tmax;
    tmax = FLT_MAX;
  }

  *t_range = intervals_intersection(*t_range, {tmin * inv_len, tmax * inv_len});

  return valid && !t_range->is_empty();
}

CCL_NAMESPACE_END
