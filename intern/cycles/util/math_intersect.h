/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

/* Custom rcp, cross and dot implementations that match Embree bit for bit. */
ccl_device_forceinline float ray_triangle_rcp(const float x)
{
#ifdef __KERNEL_NEON__
  /* Move scalar to vector register and do rcp. */
  __m128 a;
  a[0] = x;
  float32x4_t reciprocal = vrecpeq_f32(a);
  reciprocal = vmulq_f32(vrecpsq_f32(a, reciprocal), reciprocal);
  reciprocal = vmulq_f32(vrecpsq_f32(a, reciprocal), reciprocal);
  return reciprocal[0];
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
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
  return madd(make_float4(a.x),
              make_float4(b.x),
              madd(make_float4(a.y), make_float4(b.y), make_float4(a.z) * make_float4(b.z)))[0];
#else
  return a.x * b.x + a.y * b.y + a.z * b.z;
#endif
}

ccl_device_inline float3 ray_triangle_cross(const float3 a, const float3 b)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
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

  const float rcp_uvw = (fabsf(UVW) < 1e-18f) ? 0.0f : ray_triangle_rcp(UVW);
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
ccl_device bool ray_quad_intersect(float3 ray_P,
                                   float3 ray_D,
                                   float ray_tmin,
                                   float ray_tmax,
                                   float3 quad_P,
                                   float3 inv_quad_u,
                                   float3 inv_quad_v,
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
  if (isect_P != NULL)
    *isect_P = hit;
  if (isect_t != NULL)
    *isect_t = t;

  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  if (isect_u != NULL)
    *isect_u = v + 0.5f;
  if (isect_v != NULL)
    *isect_v = -u - v;

  return true;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INTERSECT_H__ */
