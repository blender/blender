/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

template<bool in_volume_segment>
ccl_device_inline bool point_light_sample(const ccl_global KernelLight *klight,
                                          const float2 rand,
                                          const float3 P,
                                          ccl_private LightSample *ls)
{
  float3 center = klight->co;
  float radius = klight->spot.radius;
  /* disk oriented normal */
  const float3 lightN = normalize(P - center);
  ls->P = center;

  if (radius > 0.0f) {
    ls->P += disk_light_sample(lightN, rand) * radius;
  }
  ls->pdf = klight->spot.invarea;

  ls->D = normalize_len(ls->P - P, &ls->t);
  /* we set the light normal to the outgoing direction to support texturing */
  ls->Ng = -ls->D;

  ls->eval_fac = M_1_PI_F * 0.25f * klight->spot.invarea;
  if (!in_volume_segment && ls->eval_fac == 0.0f) {
    return false;
  }

  float2 uv = map_to_sphere(ls->Ng);
  ls->u = uv.x;
  ls->v = uv.y;
  ls->pdf *= lamp_light_pdf(lightN, -ls->D, ls->t);
  return true;
}

ccl_device_forceinline void point_light_update_position(const ccl_global KernelLight *klight,
                                                        ccl_private LightSample *ls,
                                                        const float3 P)
{
  ls->D = normalize_len(ls->P - P, &ls->t);
  ls->Ng = -ls->D;

  float2 uv = map_to_sphere(ls->Ng);
  ls->u = uv.x;
  ls->v = uv.y;

  float invarea = klight->spot.invarea;
  ls->eval_fac = (0.25f * M_1_PI_F) * invarea;
  ls->pdf = invarea;
}

ccl_device_inline bool point_light_intersect(const ccl_global KernelLight *klight,
                                             const ccl_private Ray *ccl_restrict ray,
                                             ccl_private float *t)
{
  /* Sphere light (aka, aligned disk light). */
  const float3 lightP = klight->co;
  const float radius = klight->spot.radius;
  if (radius == 0.0f) {
    return false;
  }

  /* disk oriented normal */
  const float3 lightN = normalize(ray->P - lightP);
  float3 P;
  return ray_disk_intersect(ray->P, ray->D, ray->tmin, ray->tmax, lightP, lightN, radius, &P, t);
}

ccl_device_inline bool point_light_sample_from_intersection(
    const ccl_global KernelLight *klight,
    ccl_private const Intersection *ccl_restrict isect,
    const float3 ray_P,
    const float3 ray_D,
    ccl_private LightSample *ccl_restrict ls)
{
  const float3 lighN = normalize(ray_P - klight->co);

  /* We set the light normal to the outgoing direction to support texturing. */
  ls->Ng = -ls->D;

  float invarea = klight->spot.invarea;
  ls->eval_fac = (0.25f * M_1_PI_F) * invarea;
  ls->pdf = invarea;

  if (ls->eval_fac == 0.0f) {
    return false;
  }

  float2 uv = map_to_sphere(ls->Ng);
  ls->u = uv.x;
  ls->v = uv.y;

  /* compute pdf */
  if (ls->t != FLT_MAX) {
    ls->pdf *= lamp_light_pdf(lighN, -ls->D, ls->t);
  }
  else {
    ls->pdf = 0.f;
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
  float min_distance;
  point_to_centroid = safe_normalize_len(centroid - P, &min_distance);

  const float radius = klight->spot.radius;
  const float hypotenus = sqrtf(sqr(radius) + sqr(min_distance));
  cos_theta_u = min_distance / hypotenus;

  distance = make_float2(hypotenus, min_distance);

  return true;
}

CCL_NAMESPACE_END
