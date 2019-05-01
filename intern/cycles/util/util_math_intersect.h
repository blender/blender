/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_MATH_INTERSECT_H__
#define __UTIL_MATH_INTERSECT_H__

CCL_NAMESPACE_BEGIN

/* Ray Intersection */

ccl_device bool ray_sphere_intersect(float3 ray_P,
                                     float3 ray_D,
                                     float ray_t,
                                     float3 sphere_P,
                                     float sphere_radius,
                                     float3 *isect_P,
                                     float *isect_t)
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
    const float dsq = tsq - tp * tp; /* pythagoras */
    if (dsq > radiussq) {
      /* Closest point on ray outside sphere. */
      return false;
    }
    const float t = tp - sqrtf(radiussq - dsq); /* pythagoras */
    if (t < ray_t) {
      *isect_t = t;
      *isect_P = ray_P + ray_D * t;
      return true;
    }
  }
  return false;
}

ccl_device bool ray_aligned_disk_intersect(float3 ray_P,
                                           float3 ray_D,
                                           float ray_t,
                                           float3 disk_P,
                                           float disk_radius,
                                           float3 *isect_P,
                                           float *isect_t)
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
  if (t < 0.0f || t > ray_t) {
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

ccl_device_forceinline bool ray_triangle_intersect(float3 ray_P,
                                                   float3 ray_dir,
                                                   float ray_t,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
                                                   const ssef *ssef_verts,
#else
                                                   const float3 tri_a,
                                                   const float3 tri_b,
                                                   const float3 tri_c,
#endif
                                                   float *isect_u,
                                                   float *isect_v,
                                                   float *isect_t)
{
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  typedef ssef float3;
  const float3 tri_a(ssef_verts[0]);
  const float3 tri_b(ssef_verts[1]);
  const float3 tri_c(ssef_verts[2]);
  const float3 P(ray_P);
  const float3 dir(ray_dir);
#else
#  define dot3(a, b) dot(a, b)
  const float3 P = ray_P;
  const float3 dir = ray_dir;
#endif

  /* Calculate vertices relative to ray origin. */
  const float3 v0 = tri_c - P;
  const float3 v1 = tri_a - P;
  const float3 v2 = tri_b - P;

  /* Calculate triangle edges. */
  const float3 e0 = v2 - v0;
  const float3 e1 = v0 - v1;
  const float3 e2 = v1 - v2;

  /* Perform edge tests. */
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const float3 crossU = cross(v2 + v0, e0);
  const float3 crossV = cross(v0 + v1, e1);
  const float3 crossW = cross(v1 + v2, e2);

  ssef crossX(crossU);
  ssef crossY(crossV);
  ssef crossZ(crossW);
  ssef zero = _mm_setzero_ps();
  _MM_TRANSPOSE4_PS(crossX, crossY, crossZ, zero);

  const ssef dirX(ray_dir.x);
  const ssef dirY(ray_dir.y);
  const ssef dirZ(ray_dir.z);

  ssef UVWW = madd(crossX, dirX, madd(crossY, dirY, crossZ * dirZ));
#else  /* __KERNEL_SSE2__ */
  const float U = dot(cross(v2 + v0, e0), ray_dir);
  const float V = dot(cross(v0 + v1, e1), ray_dir);
  const float W = dot(cross(v1 + v2, e2), ray_dir);
#endif /* __KERNEL_SSE2__ */

#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  int uvw_sign = movemask(UVWW) & 0x7;
  if (uvw_sign != 0) {
    if (uvw_sign != 0x7) {
      return false;
    }
  }
#else
  const float minUVW = min(U, min(V, W));
  const float maxUVW = max(U, max(V, W));

  if (minUVW < 0.0f && maxUVW > 0.0f) {
    return false;
  }
#endif

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
  const int sign_den = (__float_as_int(den) & 0x80000000);
  const float sign_T = xor_signmask(T, sign_den);
  if ((sign_T < 0.0f) || (sign_T > ray_t * xor_signmask(den, sign_den))) {
    return false;
  }

  const float inv_den = 1.0f / den;
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  UVWW *= inv_den;
  _mm_store_ss(isect_u, UVWW);
  _mm_store_ss(isect_v, shuffle<1, 1, 3, 3>(UVWW));
#else
  *isect_u = U * inv_den;
  *isect_v = V * inv_den;
#endif
  *isect_t = T * inv_den;
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
                                   float ray_mint,
                                   float ray_maxt,
                                   float3 quad_P,
                                   float3 quad_u,
                                   float3 quad_v,
                                   float3 quad_n,
                                   float3 *isect_P,
                                   float *isect_t,
                                   float *isect_u,
                                   float *isect_v,
                                   bool ellipse)
{
  /* Perform intersection test. */
  float t = -(dot(ray_P, quad_n) - dot(quad_P, quad_n)) / dot(ray_D, quad_n);
  if (t < ray_mint || t > ray_maxt) {
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
