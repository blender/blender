/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

/* Importance sampling.
 *
 * An Area-Preserving Parametrization for Spherical Rectangles.
 * Carlos Urena et al.
 *
 * NOTE: light_p is modified when sample_coord is true. */
ccl_device_inline float area_light_rect_sample(float3 P,
                                               ccl_private float3 *light_p,
                                               const float3 axis_u,
                                               const float len_u,
                                               const float3 axis_v,
                                               const float len_v,
                                               float randu,
                                               float randv,
                                               bool sample_coord)
{
  /* In our name system we're using P for the center, which is o in the paper. */
  float3 corner = *light_p - axis_u * len_u * 0.5f - axis_v * len_v * 0.5f;
  /* Compute local reference system R. */
  float3 x = axis_u;
  float3 y = axis_v;
  float3 z = cross(x, y);
  /* Compute rectangle coords in local reference system. */
  float3 dir = corner - P;
  float z0 = dot(dir, z);
  /* Flip 'z' to make it point against Q. */
  if (z0 > 0.0f) {
    z *= -1.0f;
    z0 *= -1.0f;
  }
  float x0 = dot(dir, x);
  float y0 = dot(dir, y);
  float x1 = x0 + len_u;
  float y1 = y0 + len_v;
  /* Compute internal angles (gamma_i). */
  float4 diff = make_float4(x0, y1, x1, y0) - make_float4(x1, y0, x0, y1);
  float4 nz = make_float4(y0, x1, y1, x0) * diff;
  nz = nz / sqrt(z0 * z0 * diff * diff + nz * nz);
  float g0 = safe_acosf(-nz.x * nz.y);
  float g1 = safe_acosf(-nz.y * nz.z);
  float g2 = safe_acosf(-nz.z * nz.w);
  float g3 = safe_acosf(-nz.w * nz.x);
  /* Compute predefined constants. */
  float b0 = nz.x;
  float b1 = nz.z;
  float b0sq = b0 * b0;
  float k = M_2PI_F - g2 - g3;
  /* Compute solid angle from internal angles. */
  float S = g0 + g1 - k;

  if (sample_coord) {
    /* Compute cu. */
    float au = randu * S + k;
    float fu = (cosf(au) * b0 - b1) / sinf(au);
    float cu = 1.0f / sqrtf(fu * fu + b0sq) * (fu > 0.0f ? 1.0f : -1.0f);
    cu = clamp(cu, -1.0f, 1.0f);
    /* Compute xu. */
    float xu = -(cu * z0) / max(sqrtf(1.0f - cu * cu), 1e-7f);
    xu = clamp(xu, x0, x1);
    /* Compute yv. */
    float z0sq = z0 * z0;
    float y0sq = y0 * y0;
    float y1sq = y1 * y1;
    float d = sqrtf(xu * xu + z0sq);
    float h0 = y0 / sqrtf(d * d + y0sq);
    float h1 = y1 / sqrtf(d * d + y1sq);
    float hv = h0 + randv * (h1 - h0), hv2 = hv * hv;
    float yv = (hv2 < 1.0f - 1e-6f) ? (hv * d) / sqrtf(1.0f - hv2) : y1;

    /* Transform (xu, yv, z0) to world coords. */
    *light_p = P + xu * x + yv * y + z0 * z;
  }

  /* return pdf */
  if (S != 0.0f)
    return 1.0f / S;
  else
    return 0.0f;
}

/* Light spread. */

ccl_device float area_light_spread_attenuation(const float3 D,
                                               const float3 lightNg,
                                               const float tan_spread,
                                               const float normalize_spread)
{
  /* Model a soft-box grid, computing the ratio of light not hidden by the
   * slats of the grid at a given angle. (see D10594). */
  const float cos_a = -dot(D, lightNg);
  const float sin_a = safe_sqrtf(1.0f - sqr(cos_a));
  const float tan_a = sin_a / cos_a;
  return max((1.0f - (tan_spread * tan_a)) * normalize_spread, 0.0f);
}

/* Compute subset of area light that actually has an influence on the shading point, to
 * reduce noise with low spread. */
ccl_device bool area_light_spread_clamp_area_light(const float3 P,
                                                   const float3 lightNg,
                                                   ccl_private float3 *lightP,
                                                   const float3 axis_u,
                                                   ccl_private float *len_u,
                                                   const float3 axis_v,
                                                   ccl_private float *len_v,
                                                   const float tan_spread)
{
  /* Closest point in area light plane and distance to that plane. */
  const float3 closest_P = P - dot(lightNg, P - *lightP) * lightNg;
  const float t = len(closest_P - P);

  /* Radius of circle on area light that actually affects the shading point. */
  const float radius = t / tan_spread;

  /* Local uv coordinates of closest point. */
  const float closest_u = dot(axis_u, closest_P - *lightP);
  const float closest_v = dot(axis_v, closest_P - *lightP);

  /* Compute rectangle encompassing the circle that affects the shading point,
   * clamped to the bounds of the area light. */
  const float min_u = max(closest_u - radius, -*len_u * 0.5f);
  const float max_u = min(closest_u + radius, *len_u * 0.5f);
  const float min_v = max(closest_v - radius, -*len_v * 0.5f);
  const float max_v = min(closest_v + radius, *len_v * 0.5f);

  /* Skip if rectangle is empty. */
  if (min_u >= max_u || min_v >= max_v) {
    return false;
  }

  /* Compute new area light center position and axes from rectangle in local
   * uv coordinates. */
  const float new_center_u = 0.5f * (min_u + max_u);
  const float new_center_v = 0.5f * (min_v + max_v);
  *len_u = max_u - min_u;
  *len_v = max_v - min_v;

  *lightP = *lightP + new_center_u * axis_u + new_center_v * axis_v;

  return true;
}

/* Common API. */

template<bool in_volume_segment>
ccl_device_inline bool area_light_sample(const ccl_global KernelLight *klight,
                                         const float randu,
                                         const float randv,
                                         const float3 P,
                                         ccl_private LightSample *ls)
{
  ls->P = klight->co;

  const float3 axis_u = klight->area.axis_u;
  const float3 axis_v = klight->area.axis_v;
  const float len_u = klight->area.len_u;
  const float len_v = klight->area.len_v;
  float3 Ng = klight->area.dir;
  float invarea = fabsf(klight->area.invarea);
  bool is_round = (klight->area.invarea < 0.0f);

  if (!in_volume_segment) {
    if (dot(ls->P - P, Ng) > 0.0f) {
      return false;
    }
  }

  float3 inplane;

  if (is_round || in_volume_segment) {
    inplane = ellipse_sample(axis_u * len_u * 0.5f, axis_v * len_v * 0.5f, randu, randv);
    ls->P += inplane;
    ls->pdf = invarea;
  }
  else {
    inplane = ls->P;

    float sample_len_u = len_u;
    float sample_len_v = len_v;

    if (!in_volume_segment && klight->area.tan_spread > 0.0f) {
      if (!area_light_spread_clamp_area_light(P,
                                              Ng,
                                              &ls->P,
                                              axis_u,
                                              &sample_len_u,
                                              axis_v,
                                              &sample_len_v,
                                              klight->area.tan_spread)) {
        return false;
      }
    }

    ls->pdf = area_light_rect_sample(
        P, &ls->P, axis_u, sample_len_u, axis_v, sample_len_v, randu, randv, true);
    inplane = ls->P - inplane;
  }

  const float light_u = dot(inplane, axis_u) / len_u;
  const float light_v = dot(inplane, axis_v) / len_v;

  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  ls->u = light_v + 0.5f;
  ls->v = -light_u - light_v;

  ls->Ng = Ng;
  ls->D = normalize_len(ls->P - P, &ls->t);

  ls->eval_fac = 0.25f * invarea;

  if (klight->area.tan_spread > 0.0f) {
    /* Area Light spread angle attenuation */
    ls->eval_fac *= area_light_spread_attenuation(
        ls->D, ls->Ng, klight->area.tan_spread, klight->area.normalize_spread);
  }

  if (is_round) {
    ls->pdf *= lamp_light_pdf(Ng, -ls->D, ls->t);
  }

  return true;
}

ccl_device_forceinline void area_light_update_position(const ccl_global KernelLight *klight,
                                                       ccl_private LightSample *ls,
                                                       const float3 P)
{
  const float invarea = fabsf(klight->area.invarea);
  ls->D = normalize_len(ls->P - P, &ls->t);
  ls->pdf = invarea;

  if (klight->area.tan_spread > 0.f) {
    ls->eval_fac = 0.25f * invarea;
    ls->eval_fac *= area_light_spread_attenuation(
        ls->D, ls->Ng, klight->area.tan_spread, klight->area.normalize_spread);
  }
}

ccl_device_inline bool area_light_intersect(const ccl_global KernelLight *klight,
                                            const ccl_private Ray *ccl_restrict ray,
                                            ccl_private float *t,
                                            ccl_private float *u,
                                            ccl_private float *v)
{
  /* Area light. */
  const float invarea = fabsf(klight->area.invarea);
  const bool is_round = (klight->area.invarea < 0.0f);
  if (invarea == 0.0f) {
    return false;
  }

  const float3 inv_extent_u = klight->area.axis_u / klight->area.len_u;
  const float3 inv_extent_v = klight->area.axis_v / klight->area.len_v;
  const float3 Ng = klight->area.dir;

  /* One sided. */
  if (dot(ray->D, Ng) >= 0.0f) {
    return false;
  }

  const float3 light_P = klight->co;

  float3 P;
  return ray_quad_intersect(ray->P,
                            ray->D,
                            ray->tmin,
                            ray->tmax,
                            light_P,
                            inv_extent_u,
                            inv_extent_v,
                            Ng,
                            &P,
                            t,
                            u,
                            v,
                            is_round);
}

ccl_device_inline bool area_light_sample_from_intersection(
    const ccl_global KernelLight *klight,
    ccl_private const Intersection *ccl_restrict isect,
    const float3 ray_P,
    const float3 ray_D,
    ccl_private LightSample *ccl_restrict ls)
{

  /* area light */
  float invarea = fabsf(klight->area.invarea);

  float3 Ng = klight->area.dir;
  float3 light_P = klight->co;

  ls->u = isect->u;
  ls->v = isect->v;
  ls->D = ray_D;
  ls->Ng = Ng;

  const bool is_round = (klight->area.invarea < 0.0f);
  if (is_round) {
    ls->pdf = invarea * lamp_light_pdf(Ng, -ray_D, ls->t);
  }
  else {
    const float3 axis_u = klight->area.axis_u;
    const float3 axis_v = klight->area.axis_v;
    float sample_len_u = klight->area.len_u;
    float sample_len_v = klight->area.len_v;

    if (klight->area.tan_spread > 0.0f) {
      if (!area_light_spread_clamp_area_light(ray_P,
                                              Ng,
                                              &light_P,
                                              axis_u,
                                              &sample_len_u,
                                              axis_v,
                                              &sample_len_v,
                                              klight->area.tan_spread)) {
        return false;
      }
    }

    ls->pdf = area_light_rect_sample(
        ray_P, &light_P, axis_u, sample_len_u, axis_v, sample_len_v, 0, 0, false);
  }
  ls->eval_fac = 0.25f * invarea;

  if (klight->area.tan_spread > 0.0f) {
    /* Area Light spread angle attenuation */
    ls->eval_fac *= area_light_spread_attenuation(
        ls->D, ls->Ng, klight->area.tan_spread, klight->area.normalize_spread);
    if (ls->eval_fac == 0.0f) {
      return false;
    }
  }

  return true;
}

template<bool in_volume_segment>
ccl_device_forceinline bool area_light_tree_parameters(const ccl_global KernelLight *klight,
                                                       const float3 centroid,
                                                       const float3 P,
                                                       const float3 N,
                                                       const float3 bcone_axis,
                                                       ccl_private float &cos_theta_u,
                                                       ccl_private float2 &distance,
                                                       ccl_private float3 &point_to_centroid)
{
  if (!in_volume_segment) {
    /* TODO: a cheap substitute for minimal distance between point and primitive. Does it
     * worth the overhead to compute the accurate minimal distance? */
    float min_distance;
    point_to_centroid = safe_normalize_len(centroid - P, &min_distance);
    distance = make_float2(min_distance, min_distance);
  }

  const float3 extentu = klight->area.axis_u * klight->area.len_u;
  const float3 extentv = klight->area.axis_v * klight->area.len_v;
  for (int i = 0; i < 4; i++) {
    const float3 corner = ((i & 1) - 0.5f) * extentu + 0.5f * ((i & 2) - 1) * extentv + centroid;
    float distance_point_to_corner;
    const float3 point_to_corner = safe_normalize_len(corner - P, &distance_point_to_corner);
    cos_theta_u = fminf(cos_theta_u, dot(point_to_centroid, point_to_corner));
    if (!in_volume_segment) {
      distance.x = fmaxf(distance.x, distance_point_to_corner);
    }
  }

  const bool front_facing = dot(bcone_axis, point_to_centroid) < 0;
  const bool shape_above_surface = dot(N, centroid - P) + fabsf(dot(N, extentu)) +
                                       fabsf(dot(N, extentv)) >
                                   0;
  const bool in_volume = is_zero(N);

  return (front_facing && shape_above_surface) || in_volume;
}

CCL_NAMESPACE_END
