/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/area.h"
#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

/* Background Light */

ccl_device float3 background_map_sample(KernelGlobals kg, float2 rand, ccl_private float *pdf)
{
  /* for the following, the CDF values are actually a pair of floats, with the
   * function value as X and the actual CDF as Y.  The last entry's function
   * value is the CDF total. */
  int res_x = kernel_data.background.map_res_x;
  int res_y = kernel_data.background.map_res_y;
  int cdf_width = res_x + 1;

  /* This is basically std::lower_bound as used by PBRT. */
  int first = 0;
  int count = res_y;

  while (count > 0) {
    int step = count >> 1;
    int middle = first + step;

    if (kernel_data_fetch(light_background_marginal_cdf, middle).y < rand.y) {
      first = middle + 1;
      count -= step + 1;
    }
    else
      count = step;
  }

  int index_v = max(0, first - 1);
  kernel_assert(index_v >= 0 && index_v < res_y);

  float2 cdf_v = kernel_data_fetch(light_background_marginal_cdf, index_v);
  float2 cdf_next_v = kernel_data_fetch(light_background_marginal_cdf, index_v + 1);
  float2 cdf_last_v = kernel_data_fetch(light_background_marginal_cdf, res_y);

  /* importance-sampled V direction */
  float dv = inverse_lerp(cdf_v.y, cdf_next_v.y, rand.y);
  float v = (index_v + dv) / res_y;

  /* This is basically std::lower_bound as used by PBRT. */
  first = 0;
  count = res_x;
  while (count > 0) {
    int step = count >> 1;
    int middle = first + step;

    if (kernel_data_fetch(light_background_conditional_cdf, index_v * cdf_width + middle).y <
        rand.x)
    {
      first = middle + 1;
      count -= step + 1;
    }
    else
      count = step;
  }

  int index_u = max(0, first - 1);
  kernel_assert(index_u >= 0 && index_u < res_x);

  float2 cdf_u = kernel_data_fetch(light_background_conditional_cdf,
                                   index_v * cdf_width + index_u);
  float2 cdf_next_u = kernel_data_fetch(light_background_conditional_cdf,
                                        index_v * cdf_width + index_u + 1);
  float2 cdf_last_u = kernel_data_fetch(light_background_conditional_cdf,
                                        index_v * cdf_width + res_x);

  /* importance-sampled U direction */
  float du = inverse_lerp(cdf_u.y, cdf_next_u.y, rand.x);
  float u = (index_u + du) / res_x;

  /* compute pdf */
  float sin_theta = sinf(M_PI_F * v);
  float denom = (M_2PI_F * M_PI_F * sin_theta) * cdf_last_u.x * cdf_last_v.x;

  if (sin_theta == 0.0f || denom == 0.0f)
    *pdf = 0.0f;
  else
    *pdf = (cdf_u.x * cdf_v.x) / denom;

  /* compute direction */
  return equirectangular_to_direction(u, v);
}

/* TODO(sergey): Same as above, after the release we should consider using
 * 'noinline' for all devices.
 */
ccl_device float background_map_pdf(KernelGlobals kg, float3 direction)
{
  float2 uv = direction_to_equirectangular(direction);
  int res_x = kernel_data.background.map_res_x;
  int res_y = kernel_data.background.map_res_y;
  int cdf_width = res_x + 1;

  float sin_theta = sinf(uv.y * M_PI_F);

  if (sin_theta == 0.0f)
    return 0.0f;

  int index_u = clamp(float_to_int(uv.x * res_x), 0, res_x - 1);
  int index_v = clamp(float_to_int(uv.y * res_y), 0, res_y - 1);

  /* pdfs in V direction */
  float2 cdf_last_u = kernel_data_fetch(light_background_conditional_cdf,
                                        index_v * cdf_width + res_x);
  float2 cdf_last_v = kernel_data_fetch(light_background_marginal_cdf, res_y);

  float denom = (M_2PI_F * M_PI_F * sin_theta) * cdf_last_u.x * cdf_last_v.x;

  if (denom == 0.0f)
    return 0.0f;

  /* pdfs in U direction */
  float2 cdf_u = kernel_data_fetch(light_background_conditional_cdf,
                                   index_v * cdf_width + index_u);
  float2 cdf_v = kernel_data_fetch(light_background_marginal_cdf, index_v);

  return (cdf_u.x * cdf_v.x) / denom;
}

ccl_device_inline bool background_portal_data_fetch_and_check_side(
    KernelGlobals kg, float3 P, int index, ccl_private float3 *lightpos, ccl_private float3 *dir)
{
  int portal = kernel_data.integrator.portal_offset + index;
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, portal);

  *lightpos = klight->co;
  *dir = klight->area.dir;

  /* Check whether portal is on the right side. */
  if (dot(*dir, P - *lightpos) > 1e-4f)
    return true;

  return false;
}

ccl_device_inline float background_portal_pdf(
    KernelGlobals kg, float3 P, float3 direction, int ignore_portal, ccl_private bool *is_possible)
{
  float portal_pdf = 0.0f;

  int num_possible = 0;
  for (int p = 0; p < kernel_data.integrator.num_portals; p++) {
    if (p == ignore_portal)
      continue;

    float3 lightpos, dir;
    if (!background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
      continue;

    /* There's a portal that could be sampled from this position. */
    if (is_possible) {
      *is_possible = true;
    }
    num_possible++;

    int portal = kernel_data.integrator.portal_offset + p;
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, portal);

    const float3 axis_u = klight->area.axis_u;
    const float len_u = klight->area.len_u;
    const float3 axis_v = klight->area.axis_v;
    const float len_v = klight->area.len_v;
    const float3 inv_extent_u = axis_u / len_u;
    const float3 inv_extent_v = axis_v / len_v;

    bool is_round = (klight->area.invarea < 0.0f);

    if (!ray_quad_intersect(P,
                            direction,
                            1e-4f,
                            FLT_MAX,
                            lightpos,
                            inv_extent_u,
                            inv_extent_v,
                            dir,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            is_round))
      continue;

    if (is_round) {
      float t;
      float3 D = normalize_len(lightpos - P, &t);
      portal_pdf += fabsf(klight->area.invarea) * lamp_light_pdf(dir, -D, t);
    }
    else {
      portal_pdf += area_light_rect_sample(
          P, &lightpos, axis_u, len_u, axis_v, len_v, zero_float2(), false);
    }
  }

  if (ignore_portal >= 0) {
    /* We have skipped a portal that could be sampled as well. */
    num_possible++;
  }

  return (num_possible > 0) ? portal_pdf / num_possible : 0.0f;
}

ccl_device int background_num_possible_portals(KernelGlobals kg, float3 P)
{
  int num_possible_portals = 0;
  for (int p = 0; p < kernel_data.integrator.num_portals; p++) {
    float3 lightpos, dir;
    if (background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
      num_possible_portals++;
  }
  return num_possible_portals;
}

ccl_device float3 background_portal_sample(KernelGlobals kg,
                                           float3 P,
                                           float2 rand,
                                           int num_possible,
                                           ccl_private int *sampled_portal,
                                           ccl_private float *pdf)
{
  /* Pick a portal, then re-normalize rand.y. */
  rand.y *= num_possible;
  int portal = (int)rand.y;
  rand.y -= portal;

  /* TODO(sergey): Some smarter way of finding portal to sample
   * is welcome.
   */
  for (int p = 0; p < kernel_data.integrator.num_portals; p++) {
    /* Search for the sampled portal. */
    float3 lightpos, dir;
    if (!background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
      continue;

    if (portal == 0) {
      /* p is the portal to be sampled. */
      int portal = kernel_data.integrator.portal_offset + p;
      const ccl_global KernelLight *klight = &kernel_data_fetch(lights, portal);
      const float3 axis_u = klight->area.axis_u;
      const float3 axis_v = klight->area.axis_v;
      const float len_u = klight->area.len_u;
      const float len_v = klight->area.len_v;
      bool is_round = (klight->area.invarea < 0.0f);

      float3 D;
      if (is_round) {
        lightpos += ellipse_sample(axis_u * len_u * 0.5f, axis_v * len_v * 0.5f, rand);
        float t;
        D = normalize_len(lightpos - P, &t);
        *pdf = fabsf(klight->area.invarea) * lamp_light_pdf(dir, -D, t);
      }
      else {
        *pdf = area_light_rect_sample(P, &lightpos, axis_u, len_u, axis_v, len_v, rand, true);
        D = normalize(lightpos - P);
      }

      *pdf /= num_possible;
      *sampled_portal = p;
      return D;
    }

    portal--;
  }

  return zero_float3();
}

ccl_device_inline float3 background_sun_sample(KernelGlobals kg,
                                               float2 rand,
                                               ccl_private float *pdf)
{
  float3 D;
  const float3 N = float4_to_float3(kernel_data.background.sun);
  const float angle = kernel_data.background.sun.w;
  sample_uniform_cone(N, angle, rand, &D, pdf);
  return D;
}

ccl_device_inline float background_sun_pdf(KernelGlobals kg, float3 D)
{
  const float3 N = float4_to_float3(kernel_data.background.sun);
  const float angle = kernel_data.background.sun.w;
  return pdf_uniform_cone(N, D, angle);
}

ccl_device_inline float3 background_light_sample(KernelGlobals kg,
                                                 float3 P,
                                                 float2 rand,
                                                 ccl_private float *pdf)
{
  float portal_method_pdf = kernel_data.background.portal_weight;
  float sun_method_pdf = kernel_data.background.sun_weight;
  float map_method_pdf = kernel_data.background.map_weight;

  int num_portals = 0;
  if (portal_method_pdf > 0.0f) {
    /* Check if there are portals in the scene which we can sample. */
    num_portals = background_num_possible_portals(kg, P);
    if (num_portals == 0) {
      portal_method_pdf = 0.0f;
    }
  }

  float pdf_fac = (portal_method_pdf + sun_method_pdf + map_method_pdf);
  if (pdf_fac == 0.0f) {
    /* Use uniform as a fallback if we can't use any strategy. */
    *pdf = 1.0f / M_4PI_F;
    return sample_uniform_sphere(rand);
  }

  pdf_fac = 1.0f / pdf_fac;
  portal_method_pdf *= pdf_fac;
  sun_method_pdf *= pdf_fac;
  map_method_pdf *= pdf_fac;

  /* We have 100% in total and split it between the three categories.
   * Therefore, we pick portals if rand.x is between 0 and portal_method_pdf,
   * sun if rand.x is between portal_method_pdf and (portal_method_pdf + sun_method_pdf)
   * and map if rand.x is between (portal_method_pdf + sun_method_pdf) and 1. */
  float sun_method_cdf = portal_method_pdf + sun_method_pdf;

  int method = 0;
  float3 D;
  if (rand.x < portal_method_pdf) {
    method = 0;
    /* Rescale rand.x. */
    if (portal_method_pdf != 1.0f) {
      rand.x /= portal_method_pdf;
    }

    /* Sample a portal. */
    int portal;
    D = background_portal_sample(kg, P, rand, num_portals, &portal, pdf);
    if (num_portals > 1) {
      /* Ignore the chosen portal, its pdf is already included. */
      *pdf += background_portal_pdf(kg, P, D, portal, NULL);
    }

    /* Skip MIS if this is the only method. */
    if (portal_method_pdf == 1.0f) {
      return D;
    }
    *pdf *= portal_method_pdf;
  }
  else if (rand.x < sun_method_cdf) {
    method = 1;
    /* Rescale rand.x. */
    if (sun_method_pdf != 1.0f) {
      rand.x = (rand.x - portal_method_pdf) / sun_method_pdf;
    }

    D = background_sun_sample(kg, rand, pdf);

    /* Skip MIS if this is the only method. */
    if (sun_method_pdf == 1.0f) {
      return D;
    }
    *pdf *= sun_method_pdf;
  }
  else {
    method = 2;
    /* Rescale rand.x. */
    if (map_method_pdf != 1.0f) {
      rand.x = (rand.x - sun_method_cdf) / map_method_pdf;
    }

    D = background_map_sample(kg, rand, pdf);

    /* Skip MIS if this is the only method. */
    if (map_method_pdf == 1.0f) {
      return D;
    }
    *pdf *= map_method_pdf;
  }

  /* MIS weighting. */
  if (method != 0 && portal_method_pdf != 0.0f) {
    *pdf += portal_method_pdf * background_portal_pdf(kg, P, D, -1, NULL);
  }
  if (method != 1 && sun_method_pdf != 0.0f) {
    *pdf += sun_method_pdf * background_sun_pdf(kg, D);
  }
  if (method != 2 && map_method_pdf != 0.0f) {
    *pdf += map_method_pdf * background_map_pdf(kg, D);
  }
  return D;
}

ccl_device float background_light_pdf(KernelGlobals kg, float3 P, float3 direction)
{
  float portal_method_pdf = kernel_data.background.portal_weight;
  float sun_method_pdf = kernel_data.background.sun_weight;
  float map_method_pdf = kernel_data.background.map_weight;

  float portal_pdf = 0.0f;
  /* Portals are a special case here since we need to compute their pdf in order
   * to find out if we can sample them. */
  if (portal_method_pdf > 0.0f) {
    /* Evaluate PDF of sampling this direction by portal sampling. */
    bool is_possible = false;
    portal_pdf = background_portal_pdf(kg, P, direction, -1, &is_possible);
    if (!is_possible) {
      /* Portal sampling is not possible here because all portals point to the wrong side.
       * If other methods can be used instead, do so, otherwise uniform sampling is used as a
       * fallback. */
      portal_method_pdf = 0.0f;
    }
  }

  float pdf_fac = (portal_method_pdf + sun_method_pdf + map_method_pdf);
  if (pdf_fac == 0.0f) {
    /* Use uniform as a fallback if we can't use any strategy. */
    return 1.0f / M_4PI_F;
  }

  pdf_fac = 1.0f / pdf_fac;
  portal_method_pdf *= pdf_fac;
  sun_method_pdf *= pdf_fac;
  map_method_pdf *= pdf_fac;

  float pdf = portal_pdf * portal_method_pdf;
  if (sun_method_pdf != 0.0f) {
    pdf += background_sun_pdf(kg, direction) * sun_method_pdf;
  }
  if (map_method_pdf != 0.0f) {
    pdf += background_map_pdf(kg, direction) * map_method_pdf;
  }

  return pdf;
}

ccl_device_forceinline bool background_light_tree_parameters(const float3 centroid,
                                                             ccl_private float &cos_theta_u,
                                                             ccl_private float2 &distance,
                                                             ccl_private float3 &point_to_centroid)
{
  /* Cover the whole sphere */
  cos_theta_u = -1.0f;

  distance = make_float2(1.0f, 1.0f);
  point_to_centroid = -centroid;

  return true;
}

CCL_NAMESPACE_END
