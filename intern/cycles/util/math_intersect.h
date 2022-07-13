/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_MATH_INTERSECT_H__
#define __UTIL_MATH_INTERSECT_H__

CCL_NAMESPACE_BEGIN

/* Ray Intersection */

ccl_device bool ray_sphere_intersect(float3 ray_P,
                                     float3 ray_D,
                                     float ray_tmin,
                                     float ray_tmax,
                                     float3 sphere_P,
                                     float sphere_radius,
                                     ccl_private float3 *isect_P,
                                     ccl_private float *isect_t)
{
  const float3 d = sphere_P - ray_P;
  const float radiussq = sphere_radius * sphere_radius;
  const float tsq = dot(d, d);

  if (tsq > radiussq) {
    /* Ray origin outside sphere. */
    const float tp = dot(d, ray_D);
    if (tp < 0.0f) {
      /* Ray  points away from sphere. */
      return false;
    }
    const float dsq = tsq - tp * tp; /* Pythagoras. */
    if (dsq > radiussq) {
      /* Closest point on ray outside sphere. */
      return false;
    }
    const float t = tp - sqrtf(radiussq - dsq); /* pythagoras */
    if (t > ray_tmin && t < ray_tmax) {
      *isect_t = t;
      *isect_P = ray_P + ray_D * t;
      return true;
    }
  }
  return false;
}

ccl_device bool ray_aligned_disk_intersect(float3 ray_P,
                                           float3 ray_D,
                                           float ray_tmin,
                                           float ray_tmax,
                                           float3 disk_P,
                                           float disk_radius,
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
  float3 P = ray_P + ray_D * t;
  if (len_squared(P - disk_P) > disk_radius * disk_radius) {
    return false;
  }
  *isect_P = P;
  *isect_t = t;
  return true;
}

ccl_device bool ray_disk_intersect(float3 ray_P,
                                   float3 ray_D,
                                   float ray_tmin,
                                   float ray_tmax,
                                   float3 disk_P,
                                   float3 disk_N,
                                   float disk_radius,
                                   ccl_private float3 *isect_P,
                                   ccl_private float *isect_t)
{
  const float3 vp = ray_P - disk_P;
  const float dp = dot(vp, disk_N);
  const float cos_angle = dot(disk_N, -ray_D);
  if (dp * cos_angle > 0.f)  // front of light
  {
    float t = dp / cos_angle;
    if (t < 0.f) { /* Ray points away from the light. */
      return false;
    }
    float3 P = ray_P + t * ray_D;
    float3 T = P - disk_P;

    if (dot(T, T) < sqr(disk_radius) && (t > ray_tmin && t < ray_tmax)) {
      *isect_P = ray_P + t * ray_D;
      *isect_t = t;
      return true;
    }
  }
  return false;
}

ccl_device_forceinline bool ray_triangle_intersect(float3 ray_P,
                                                   float3 ray_dir,
                                                   float ray_tmin,
                                                   float ray_tmax,
                                                   const float3 tri_a,
                                                   const float3 tri_b,
                                                   const float3 tri_c,
                                                   ccl_private float *isect_u,
                                                   ccl_private float *isect_v,
                                                   ccl_private float *isect_t)
{
#define dot3(a, b) dot(a, b)
  const float3 P = ray_P;
  const float3 dir = ray_dir;

  /* Calculate vertices relative to ray origin. */
  const float3 v0 = tri_c - P;
  const float3 v1 = tri_a - P;
  const float3 v2 = tri_b - P;

  /* Calculate triangle edges. */
  const float3 e0 = v2 - v0;
  const float3 e1 = v0 - v1;
  const float3 e2 = v1 - v2;

  /* Perform edge tests. */
  const float U = dot(cross(v2 + v0, e0), ray_dir);
  const float V = dot(cross(v0 + v1, e1), ray_dir);
  const float W = dot(cross(v1 + v2, e2), ray_dir);

  const float minUVW = min(U, min(V, W));
  const float maxUVW = max(U, max(V, W));

  if (minUVW < 0.0f && maxUVW > 0.0f) {
    return false;
  }

  /* Calculate geometry normal and denominator. */
  const float3 Ng1 = cross(e1, e0);
  // const Vec3vfM Ng1 = stable_triangle_normal(e2,e1,e0);
  const float3 Ng = Ng1 + Ng1;
  const float den = dot3(Ng, dir);
  /* Avoid division by 0. */
  if (UNLIKELY(den == 0.0f)) {
    return false;
  }

  /* Perform depth test. */
  const float T = dot3(v0, Ng);
  const float t = T / den;
  if (!(t >= ray_tmin && t <= ray_tmax)) {
    return false;
  }

  *isect_u = U / den;
  *isect_v = V / den;
  *isect_t = t;
  return true;

#undef dot3
}

/* Tests for an intersection between a ray and a quad defined by
 * its midpoint, normal and sides.
 * If ellipse is true, hits outside the ellipse that's enclosed by the
 * quad are rejected.
 */
ccl_device bool ray_quad_intersect(float3 ray_P,
                                   float3 ray_D,
                                   float ray_tmin,
                                   float ray_tmax,
                                   float3 quad_P,
                                   float3 quad_u,
                                   float3 quad_v,
                                   float3 quad_n,
                                   ccl_private float3 *isect_P,
                                   ccl_private float *isect_t,
                                   ccl_private float *isect_u,
                                   ccl_private float *isect_v,
                                   bool ellipse)
{
  /* Perform intersection test. */
  float t = -(dot(ray_P, quad_n) - dot(quad_P, quad_n)) / dot(ray_D, quad_n);
  if (!(t > ray_tmin && t < ray_tmax)) {
    return false;
  }
  const float3 hit = ray_P + t * ray_D;
  const float3 inplane = hit - quad_P;
  const float u = dot(inplane, quad_u) / dot(quad_u, quad_u);
  if (u < -0.5f || u > 0.5f) {
    return false;
  }
  const float v = dot(inplane, quad_v) / dot(quad_v, quad_v);
  if (v < -0.5f || v > 0.5f) {
    return false;
  }
  if (ellipse && (u * u + v * v > 0.25f)) {
    return false;
  }
  /* Store the result. */
  /* TODO(sergey): Check whether we can avoid some checks here. */
  if (isect_P != NULL)
    *isect_P = hit;
  if (isect_t != NULL)
    *isect_t = t;
  if (isect_u != NULL)
    *isect_u = u + 0.5f;
  if (isect_v != NULL)
    *isect_v = v + 0.5f;
  return true;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INTERSECT_H__ */
