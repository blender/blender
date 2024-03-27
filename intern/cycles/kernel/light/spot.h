/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

/* Transform vector to spot light's local coordinate system. */
ccl_device float3 spot_light_to_local(const ccl_global KernelLight *klight, const float3 ray)
{
  const Transform itfm = klight->itfm;
  float3 transformed_ray = safe_normalize(transform_direction(&itfm, ray));
  transformed_ray.z = -transformed_ray.z;

  return transformed_ray;
}

/* Compute spot light attenuation of a ray given in local coordinate system. */
ccl_device float spot_light_attenuation(const ccl_global KernelSpotLight *spot, const float3 ray)
{
  return smoothstepf((ray.z - spot->cos_half_spot_angle) * spot->spot_smooth);
}

ccl_device void spot_light_uv(const float3 ray,
                              const float half_cot_half_spot_angle,
                              ccl_private float *u,
                              ccl_private float *v)
{
  /* Ensures that the spot light projects the full image regardless of the spot angle. */
  const float factor = half_cot_half_spot_angle / ray.z;

  /* NOTE: Return barycentric coordinates in the same notation as Embree and OptiX. */
  *u = ray.y * factor + 0.5f;
  *v = -(ray.x + ray.y) * factor;
}

template<bool in_volume_segment>
ccl_device_inline bool spot_light_sample(const ccl_global KernelLight *klight,
                                         const float2 rand,
                                         const float3 P,
                                         const float3 N,
                                         const int shader_flags,
                                         ccl_private LightSample *ls)
{
  const float r_sq = sqr(klight->spot.radius);

  float3 lightN = P - klight->co;
  const float d_sq = len_squared(lightN);
  const float d = sqrtf(d_sq);
  lightN /= d;

  ls->eval_fac = klight->spot.eval_fac;

  if (klight->spot.is_sphere) {
    /* Spherical light geometry. */
    float cos_theta;
    ls->t = FLT_MAX;
    if (d_sq > r_sq) {
      /* Outside sphere. */
      const float one_minus_cos_half_spot_spread = 1.0f - klight->spot.cos_half_larger_spread;
      const float one_minus_cos_half_angle = sin_sqr_to_one_minus_cos(r_sq / d_sq);

      if (in_volume_segment || one_minus_cos_half_angle < one_minus_cos_half_spot_spread) {
        /* Sample visible part of the sphere. */
        ls->D = sample_uniform_cone(-lightN, one_minus_cos_half_angle, rand, &cos_theta, &ls->pdf);
      }
      else {
        /* Sample spread cone. */
        ls->D = sample_uniform_cone(
            -klight->spot.dir, one_minus_cos_half_spot_spread, rand, &cos_theta, &ls->pdf);

        if (!ray_sphere_intersect(
                P, ls->D, 0.0f, FLT_MAX, klight->co, klight->spot.radius, &ls->P, &ls->t))
        {
          /* Sampled direction does not intersect with the light. */
          return false;
        }
      }
    }
    else {
      /* Inside sphere. */
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

    /* Attenuation. */
    const float3 local_ray = spot_light_to_local(klight, -ls->D);
    if (d_sq > r_sq) {
      ls->eval_fac *= spot_light_attenuation(&klight->spot, local_ray);
    }
    if (!in_volume_segment && ls->eval_fac == 0.0f) {
      return false;
    }

    if (ls->t == FLT_MAX) {
      /* Law of cosines. */
      ls->t = d * cos_theta -
              copysignf(safe_sqrtf(r_sq - d_sq + d_sq * sqr(cos_theta)), d_sq - r_sq);
      ls->P = P + ls->D * ls->t;
    }
    else {
      /* Already computed when sampling the spread cone. */
    }

    /* Remap sampled point onto the sphere to prevent precision issues with small radius. */
    ls->Ng = normalize(ls->P - klight->co);
    ls->P = ls->Ng * klight->spot.radius + klight->co;

    /* Texture coordinates. */
    spot_light_uv(local_ray, klight->spot.half_cot_half_spot_angle, &ls->u, &ls->v);
  }
  else {
    /* Point light with ad-hoc radius based on oriented disk. */
    ls->P = klight->co;
    if (r_sq > 0.0f) {
      ls->P += disk_light_sample(lightN, rand) * klight->spot.radius;
    }

    ls->D = normalize_len(ls->P - P, &ls->t);
    ls->Ng = -ls->D;

    /* Attenuation. */
    const float3 local_ray = spot_light_to_local(klight, -ls->D);
    ls->eval_fac *= spot_light_attenuation(&klight->spot, local_ray);
    if (!in_volume_segment && ls->eval_fac == 0.0f) {
      return false;
    }

    /* PDF. */
    const float invarea = (r_sq > 0.0f) ? 1.0f / (r_sq * M_PI_F) : 1.0f;
    ls->pdf = invarea * light_pdf_area_to_solid_angle(lightN, -ls->D, ls->t);

    /* Texture coordinates. */
    spot_light_uv(local_ray, klight->spot.half_cot_half_spot_angle, &ls->u, &ls->v);
  }

  return true;
}

ccl_device_forceinline float spot_light_pdf(const ccl_global KernelSpotLight *spot,
                                            const float d_sq,
                                            const float r_sq,
                                            const float3 N,
                                            const float3 D,
                                            const uint32_t path_flag)
{
  if (d_sq > r_sq) {
    return M_1_2PI_F /
           min(sin_sqr_to_one_minus_cos(r_sq / d_sq), 1.0f - spot->cos_half_larger_spread);
  }

  const bool has_transmission = (path_flag & PATH_RAY_MIS_HAD_TRANSMISSION);
  return has_transmission ? M_1_2PI_F * 0.5f : pdf_cos_hemisphere(N, D);
}

ccl_device_forceinline void spot_light_mnee_sample_update(const ccl_global KernelLight *klight,
                                                          ccl_private LightSample *ls,
                                                          const float3 P,
                                                          const float3 N,
                                                          const uint32_t path_flag)
{
  ls->D = normalize_len(ls->P - P, &ls->t);

  ls->eval_fac = klight->spot.eval_fac;

  const float radius = klight->spot.radius;
  bool use_attenuation = true;

  if (klight->spot.is_sphere) {
    const float d_sq = len_squared(P - klight->co);
    const float r_sq = sqr(radius);
    const float t_sq = sqr(ls->t);

    /* NOTE : preserve pdf in area measure. */
    const float jacobian_solid_angle_to_area = 0.5f * fabsf(d_sq - r_sq - t_sq) /
                                               (radius * ls->t * t_sq);
    ls->pdf = spot_light_pdf(&klight->spot, d_sq, r_sq, N, ls->D, path_flag) *
              jacobian_solid_angle_to_area;

    ls->Ng = normalize(ls->P - klight->co);

    use_attenuation = (d_sq > r_sq);
  }
  else {
    /* NOTE : preserve pdf in area measure. */
    ls->pdf = ls->eval_fac * 4.0f * M_PI_F;

    ls->Ng = -ls->D;
  }

  /* Attenuation. */
  const float3 local_ray = spot_light_to_local(klight, -ls->D);
  if (use_attenuation) {
    ls->eval_fac *= spot_light_attenuation(&klight->spot, local_ray);
  }

  /* Texture coordinates. */
  spot_light_uv(local_ray, klight->spot.half_cot_half_spot_angle, &ls->u, &ls->v);
}

ccl_device_inline bool spot_light_intersect(const ccl_global KernelLight *klight,
                                            const ccl_private Ray *ccl_restrict ray,
                                            ccl_private float *t)
{
  /* One sided. */
  if (dot(ray->D, ray->P - klight->co) >= 0.0f) {
    return false;
  }

  return point_light_intersect(klight, ray, t);
}

ccl_device_inline bool spot_light_sample_from_intersection(
    const ccl_global KernelLight *klight,
    ccl_private const Intersection *ccl_restrict isect,
    const float3 ray_P,
    const float3 ray_D,
    const float3 N,
    const uint32_t path_flag,
    ccl_private LightSample *ccl_restrict ls)
{
  const float r_sq = sqr(klight->spot.radius);
  const float d_sq = len_squared(ray_P - klight->co);

  ls->eval_fac = klight->spot.eval_fac;

  if (klight->spot.is_sphere) {
    ls->pdf = spot_light_pdf(&klight->spot, d_sq, r_sq, N, ray_D, path_flag);
    ls->Ng = normalize(ls->P - klight->co);
  }
  else {
    if (ls->t != FLT_MAX) {
      const float3 lightN = normalize(ray_P - klight->co);
      const float invarea = (r_sq > 0.0f) ? 1.0f / (r_sq * M_PI_F) : 1.0f;
      ls->pdf = invarea * light_pdf_area_to_solid_angle(lightN, -ray_D, ls->t);
    }
    else {
      ls->pdf = 0.0f;
    }
    ls->Ng = -ray_D;
  }

  /* Attenuation. */
  const float3 local_ray = spot_light_to_local(klight, -ray_D);
  if (!klight->spot.is_sphere || d_sq > r_sq) {
    ls->eval_fac *= spot_light_attenuation(&klight->spot, local_ray);
  }
  if (ls->eval_fac == 0) {
    return false;
  }

  /* Texture coordinates. */
  spot_light_uv(local_ray, klight->spot.half_cot_half_spot_angle, &ls->u, &ls->v);

  return true;
}

/* Find the ray segment lit by the spot light. */
ccl_device_inline bool spot_light_valid_ray_segment(const ccl_global KernelLight *klight,
                                                    const float3 P,
                                                    const float3 D,
                                                    ccl_private float2 *t_range)
{
  /* Convert to local space of the spot light. */
  const Transform itfm = klight->itfm;
  float3 local_P = P + klight->spot.dir * klight->spot.ray_segment_dp;
  local_P = transform_point(&itfm, local_P);
  const float3 local_D = transform_direction(&itfm, D);
  const float3 axis = make_float3(0.0f, 0.0f, -1.0f);

  /* Intersect the ray with the smallest enclosing cone of the light spread. */
  return ray_cone_intersect(
      axis, local_P, local_D, sqr(klight->spot.cos_half_spot_angle), t_range);
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
  point_to_centroid = safe_normalize_len(centroid - P, &min_distance);
  distance = min_distance * one_float2();

  const float radius = klight->spot.radius;

  if (klight->spot.is_sphere) {
    cos_theta_u = (min_distance > radius) ? cos_from_sin(radius / min_distance) : -1.0f;

    if (in_volume_segment) {
      return true;
    }

    distance = (min_distance > radius) ? min_distance * make_float2(1.0f / cos_theta_u, 1.0f) :
                                         one_float2() * radius / M_SQRT2_F;
  }
  else {
    const float hypotenus = sqrtf(sqr(radius) + sqr(min_distance));
    cos_theta_u = min_distance / hypotenus;

    if (in_volume_segment) {
      return true;
    }

    distance.x = hypotenus;
  }

  return true;
}

CCL_NAMESPACE_END
