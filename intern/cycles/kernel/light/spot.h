/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

ccl_device float spot_light_attenuation(const ccl_global KernelSpotLight *spot, float3 ray)
{
  const float3 scaled_ray = safe_normalize(
      make_float3(dot(ray, spot->axis_u), dot(ray, spot->axis_v), dot(ray, spot->dir)) /
      spot->len);

  return smoothstepf((scaled_ray.z - spot->cos_half_spot_angle) * spot->spot_smooth);
}

template<bool in_volume_segment>
ccl_device_inline bool spot_light_sample(const ccl_global KernelLight *klight,
                                         const float2 rand,
                                         const float3 P,
                                         ccl_private LightSample *ls)
{
  ls->P = klight->co;

  const float3 center = klight->co;
  const float radius = klight->spot.radius;
  /* disk oriented normal */
  const float3 lightN = normalize(P - center);
  ls->P = center;

  if (radius > 0.0f) {
    /* disk light */
    ls->P += disk_light_sample(lightN, rand) * radius;
  }

  const float invarea = klight->spot.invarea;
  ls->pdf = invarea;

  ls->D = normalize_len(ls->P - P, &ls->t);
  /* we set the light normal to the outgoing direction to support texturing */
  ls->Ng = -ls->D;

  ls->eval_fac = (0.25f * M_1_PI_F) * invarea;

  /* spot light attenuation */
  ls->eval_fac *= spot_light_attenuation(&klight->spot, -ls->D);
  if (!in_volume_segment && ls->eval_fac == 0.0f) {
    return false;
  }

  float2 uv = map_to_sphere(ls->Ng);
  ls->u = uv.x;
  ls->v = uv.y;

  ls->pdf *= lamp_light_pdf(lightN, -ls->D, ls->t);
  return true;
}

ccl_device_forceinline void spot_light_mnee_sample_update(const ccl_global KernelLight *klight,
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
  /* NOTE : preserve pdf in area measure. */
  ls->pdf = invarea;

  /* spot light attenuation */
  ls->eval_fac *= spot_light_attenuation(&klight->spot, ls->Ng);
}

ccl_device_inline bool spot_light_intersect(const ccl_global KernelLight *klight,
                                            const ccl_private Ray *ccl_restrict ray,
                                            ccl_private float *t)
{
  /* Spot/Disk light. */
  const float3 lightP = klight->co;
  const float radius = klight->spot.radius;
  if (radius == 0.0f) {
    return false;
  }
  /* disk oriented normal */
  const float3 lightN = normalize(ray->P - lightP);
  /* One sided. */
  if (dot(ray->D, lightN) >= 0.0f) {
    return false;
  }

  float3 P;
  return ray_disk_intersect(ray->P, ray->D, ray->tmin, ray->tmax, lightP, lightN, radius, &P, t);
}

ccl_device_inline bool spot_light_sample_from_intersection(
    const ccl_global KernelLight *klight,
    ccl_private const Intersection *ccl_restrict isect,
    const float3 ray_P,
    const float3 ray_D,
    ccl_private LightSample *ccl_restrict ls)
{
  /* the normal of the oriented disk */
  const float3 lightN = normalize(ray_P - klight->co);
  /* We set the light normal to the outgoing direction to support texturing. */
  ls->Ng = -ls->D;

  float invarea = klight->spot.invarea;
  ls->eval_fac = (0.25f * M_1_PI_F) * invarea;
  ls->pdf = invarea;

  /* spot light attenuation */
  ls->eval_fac *= spot_light_attenuation(&klight->spot, -ls->D);

  if (ls->eval_fac == 0.0f) {
    return false;
  }

  float2 uv = map_to_sphere(ls->Ng);
  ls->u = uv.x;
  ls->v = uv.y;

  /* compute pdf */
  if (ls->t != FLT_MAX) {
    ls->pdf *= lamp_light_pdf(lightN, -ls->D, ls->t);
  }
  else {
    ls->pdf = 0.f;
  }

  return true;
}

template<bool in_volume_segment>
ccl_device_forceinline bool spot_light_tree_parameters(const ccl_global KernelLight *klight,
                                                       const float3 centroid,
                                                       const float3 P,
                                                       ccl_private float &cos_theta_u,
                                                       ccl_private float2 &distance,
                                                       ccl_private float3 &point_to_centroid)
{
  float min_distance;
  const float3 point_to_centroid_ = safe_normalize_len(centroid - P, &min_distance);

  const float radius = klight->spot.radius;
  const float hypotenus = sqrtf(sqr(radius) + sqr(min_distance));
  cos_theta_u = min_distance / hypotenus;

  if (in_volume_segment) {
    return true;
  }

  distance = make_float2(hypotenus, min_distance);
  point_to_centroid = point_to_centroid_;

  return true;
}

CCL_NAMESPACE_END
