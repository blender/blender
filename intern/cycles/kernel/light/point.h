/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool point_light_sample(const ccl_global KernelLight *klight,
                                          const float2 rand,
                                          const float3 P,
                                          const float3 N,
                                          const int shader_flags,
                                          ccl_private LightSample *ls)
{
  float3 lightN = P - klight->co;
  const float d_sq = len_squared(lightN);
  const float d = sqrtf(d_sq);
  lightN /= d;

  const float r_sq = sqr(klight->spot.radius);

  float cos_theta;
  if (d_sq > r_sq) {
    const float one_minus_cos = sin_sqr_to_one_minus_cos(r_sq / d_sq);
    sample_uniform_cone_concentric(-lightN, one_minus_cos, rand, &cos_theta, &ls->D, &ls->pdf);
  }
  else {
    const bool has_transmission = (shader_flags & SD_BSDF_HAS_TRANSMISSION);
    if (has_transmission) {
      ls->D = sample_uniform_sphere(rand);
      ls->pdf = M_1_2PI_F * 0.5f;
    }
    else {
      sample_cos_hemisphere(N, rand, &ls->D, &ls->pdf);
    }
    cos_theta = -dot(ls->D, lightN);
  }

  /* Law of cosines. */
  ls->t = d * cos_theta - copysignf(safe_sqrtf(r_sq - d_sq + d_sq * sqr(cos_theta)), d_sq - r_sq);

  ls->P = P + ls->D * ls->t;

  ls->eval_fac = M_1_PI_F * 0.25f * klight->spot.invarea;
  if (r_sq == 0) {
    /* Use intensity instead of radiance for point light. */
    ls->eval_fac /= sqr(ls->t);
    /* `ls->Ng` is not well-defined for point light, so use the incoming direction instead.  */
    ls->Ng = -ls->D;
  }
  else {
    ls->Ng = normalize(ls->P - klight->co);
    /* Remap sampled point onto the sphere to prevent precision issues with small radius. */
    ls->P = ls->Ng * klight->spot.radius + klight->co;
  }

  const Transform itfm = klight->itfm;
  const float2 uv = map_to_sphere(transform_direction(&itfm, ls->Ng));
  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  ls->u = uv.y;
  ls->v = 1.0f - uv.x - uv.y;

  return true;
}

ccl_device_forceinline float point_light_pdf(
    const float d_sq, const float r_sq, const float3 N, const float3 D, const uint32_t path_flag)
{
  if (d_sq > r_sq) {
    return M_1_2PI_F / sin_sqr_to_one_minus_cos(r_sq / d_sq);
  }

  const bool has_transmission = (path_flag & PATH_RAY_MIS_HAD_TRANSMISSION);
  return has_transmission ? M_1_2PI_F * 0.5f : pdf_cos_hemisphere(N, D);
}

ccl_device_forceinline void point_light_mnee_sample_update(const ccl_global KernelLight *klight,
                                                           ccl_private LightSample *ls,
                                                           const float3 P,
                                                           const float3 N,
                                                           const uint32_t path_flag)
{
  ls->D = normalize_len(ls->P - P, &ls->t);

  const float radius = klight->spot.radius;

  if (radius > 0) {
    const float d_sq = len_squared(P - klight->co);
    const float r_sq = sqr(radius);
    const float t_sq = sqr(ls->t);

    ls->pdf = point_light_pdf(d_sq, r_sq, N, ls->D, path_flag);

    /* NOTE : preserve pdf in area measure. */
    ls->pdf *= 0.5f * fabsf(d_sq - r_sq - t_sq) / (radius * ls->t * t_sq);

    ls->Ng = normalize(ls->P - klight->co);
  }
  else {
    ls->eval_fac = M_1_PI_F * 0.25f * klight->spot.invarea;

    ls->Ng = -ls->D;

    /* PDF does not change. */
  }

  const Transform itfm = klight->itfm;
  const float2 uv = map_to_sphere(transform_direction(&itfm, ls->Ng));
  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  ls->u = uv.y;
  ls->v = 1.0f - uv.x - uv.y;
}

ccl_device_inline bool point_light_intersect(const ccl_global KernelLight *klight,
                                             const ccl_private Ray *ccl_restrict ray,
                                             ccl_private float *t)
{
  const float radius = klight->spot.radius;
  if (radius == 0.0f) {
    return false;
  }

  float3 P;
  return ray_sphere_intersect(ray->P, ray->D, ray->tmin, ray->tmax, klight->co, radius, &P, t);
}

ccl_device_inline bool point_light_sample_from_intersection(
    const ccl_global KernelLight *klight,
    ccl_private const Intersection *ccl_restrict isect,
    const float3 ray_P,
    const float3 ray_D,
    const float3 N,
    const uint32_t path_flag,
    ccl_private LightSample *ccl_restrict ls)
{
  ls->eval_fac = M_1_PI_F * 0.25f * klight->spot.invarea;

  const float radius = klight->spot.radius;

  ls->Ng = radius > 0 ? normalize(ls->P - klight->co) : -ray_D;

  const Transform itfm = klight->itfm;
  const float2 uv = map_to_sphere(transform_direction(&itfm, ls->Ng));
  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  ls->u = uv.y;
  ls->v = 1.0f - uv.x - uv.y;

  if (ls->t == FLT_MAX) {
    ls->pdf = 0.0f;
  }
  else {
    ls->pdf = point_light_pdf(len_squared(ray_P - klight->co), sqr(radius), N, ray_D, path_flag);
  }

  return true;
}

template<bool in_volume_segment>
ccl_device_forceinline bool point_light_tree_parameters(const ccl_global KernelLight *klight,
                                                        const float3 centroid,
                                                        const float3 P,
                                                        ccl_private float &cos_theta_u,
                                                        ccl_private float2 &distance,
                                                        ccl_private float3 &point_to_centroid)
{
  if (in_volume_segment) {
    cos_theta_u = 1.0f; /* Any value in [-1, 1], irrelevant since theta = 0 */
    return true;
  }

  float dist_point_to_centroid;
  point_to_centroid = safe_normalize_len(centroid - P, &dist_point_to_centroid);

  const float radius = klight->spot.radius;
  if (dist_point_to_centroid > radius) {
    /* Equivalent to a disk light with the same angular span. */
    cos_theta_u = cos_from_sin(radius / dist_point_to_centroid);
    distance = dist_point_to_centroid * make_float2(1.0f / cos_theta_u, 1.0f);
  }
  else {
    /* Similar to background light. */
    cos_theta_u = -1.0f;
    /* HACK: pack radiance scaling in the distance. */
    distance = one_float2() * radius / M_SQRT2_F;
  }

  return true;
}

CCL_NAMESPACE_END
