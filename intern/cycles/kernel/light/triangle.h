/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/geom/geom.h"

CCL_NAMESPACE_BEGIN

/* returns true if the triangle is has motion blur or an instancing transform applied */
ccl_device_inline bool triangle_world_space_vertices(
    KernelGlobals kg, int object, int prim, float time, float3 V[3])
{
  bool has_motion = false;
  const int object_flag = kernel_data_fetch(object_flag, object);

  if (object_flag & SD_OBJECT_HAS_VERTEX_MOTION && time >= 0.0f) {
    motion_triangle_vertices(kg, object, prim, time, V);
    has_motion = true;
  }
  else {
    triangle_vertices(kg, prim, V);
  }

  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
#ifdef __OBJECT_MOTION__
    float object_time = (time >= 0.0f) ? time : 0.5f;
    Transform tfm = object_fetch_transform_motion_test(kg, object, object_time, NULL);
#else
    Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
    V[0] = transform_point(&tfm, V[0]);
    V[1] = transform_point(&tfm, V[1]);
    V[2] = transform_point(&tfm, V[2]);
    has_motion = true;
  }
  return has_motion;
}

ccl_device_inline float triangle_light_pdf_area_sampling(const float3 Ng, const float3 I, float t)
{
  float cos_pi = fabsf(dot(Ng, I));

  if (cos_pi == 0.0f)
    return 0.0f;

  return t * t / cos_pi;
}

ccl_device_forceinline float triangle_light_pdf(KernelGlobals kg,
                                                ccl_private const ShaderData *sd,
                                                float t)
{
  /* A naive heuristic to decide between costly solid angle sampling
   * and simple area sampling, comparing the distance to the triangle plane
   * to the length of the edges of the triangle. */

  float3 V[3];
  bool has_motion = triangle_world_space_vertices(kg, sd->object, sd->prim, sd->time, V);

  const float3 e0 = V[1] - V[0];
  const float3 e1 = V[2] - V[0];
  const float3 e2 = V[2] - V[1];
  const float longest_edge_squared = max(len_squared(e0), max(len_squared(e1), len_squared(e2)));
  const float3 N = cross(e0, e1);
  const float distance_to_plane = fabsf(dot(N, sd->I * t)) / dot(N, N);
  const float area = 0.5f * len(N);

  float pdf;

  if (longest_edge_squared > distance_to_plane * distance_to_plane) {
    /* sd contains the point on the light source
     * calculate Px, the point that we're shading */
    const float3 Px = sd->P + sd->I * t;
    const float3 v0_p = V[0] - Px;
    const float3 v1_p = V[1] - Px;
    const float3 v2_p = V[2] - Px;

    const float3 u01 = safe_normalize(cross(v0_p, v1_p));
    const float3 u02 = safe_normalize(cross(v0_p, v2_p));
    const float3 u12 = safe_normalize(cross(v1_p, v2_p));

    const float alpha = fast_acosf(dot(u02, u01));
    const float beta = fast_acosf(-dot(u01, u12));
    const float gamma = fast_acosf(dot(u02, u12));
    const float solid_angle = alpha + beta + gamma - M_PI_F;

    /* distribution_pdf_triangles is calculated over triangle area, but we're not sampling over
     * its area */
    if (UNLIKELY(solid_angle == 0.0f)) {
      return 0.0f;
    }
    else {
      pdf = 1.0f / solid_angle;
    }
  }
  else {
    if (UNLIKELY(area == 0.0f)) {
      return 0.0f;
    }

    pdf = triangle_light_pdf_area_sampling(sd->Ng, sd->I, t) / area;
  }

  /* Belongs in distribution.h but can reuse computations here. */
  if (!kernel_data.integrator.use_light_tree) {
    float distribution_area = area;

    if (has_motion && area != 0.0f) {
      /* For motion blur need area of triangle at fixed time as used in the CDF. */
      triangle_world_space_vertices(kg, sd->object, sd->prim, -1.0f, V);
      distribution_area = triangle_area(V[0], V[1], V[2]);
    }

    pdf *= distribution_area * kernel_data.integrator.distribution_pdf_triangles;
  }

  return pdf;
}

template<bool in_volume_segment>
ccl_device_forceinline bool triangle_light_sample(KernelGlobals kg,
                                                  int prim,
                                                  int object,
                                                  float randu,
                                                  float randv,
                                                  float time,
                                                  ccl_private LightSample *ls,
                                                  const float3 P)
{
  /* A naive heuristic to decide between costly solid angle sampling
   * and simple area sampling, comparing the distance to the triangle plane
   * to the length of the edges of the triangle. */

  float3 V[3];
  bool has_motion = triangle_world_space_vertices(kg, object, prim, time, V);

  const float3 e0 = V[1] - V[0];
  const float3 e1 = V[2] - V[0];
  const float3 e2 = V[2] - V[1];
  const float longest_edge_squared = max(len_squared(e0), max(len_squared(e1), len_squared(e2)));
  const float3 N0 = cross(e0, e1);
  float Nl = 0.0f;
  ls->Ng = safe_normalize_len(N0, &Nl);
  const float area = 0.5f * Nl;

  /* flip normal if necessary */
  const int object_flag = kernel_data_fetch(object_flag, object);
  if (object_flag & SD_OBJECT_NEGATIVE_SCALE) {
    ls->Ng = -ls->Ng;
  }
  ls->eval_fac = 1.0f;
  ls->shader = kernel_data_fetch(tri_shader, prim);
  ls->object = object;
  ls->prim = prim;
  ls->lamp = LAMP_NONE;
  ls->shader |= SHADER_USE_MIS;
  ls->type = LIGHT_TRIANGLE;
  ls->group = object_lightgroup(kg, object);

  float distance_to_plane = fabsf(dot(N0, V[0] - P) / dot(N0, N0));

  if (!in_volume_segment && (longest_edge_squared > distance_to_plane * distance_to_plane)) {
    /* see James Arvo, "Stratified Sampling of Spherical Triangles"
     * http://www.graphics.cornell.edu/pubs/1995/Arv95c.pdf */

    /* project the triangle to the unit sphere
     * and calculate its edges and angles */
    const float3 v0_p = V[0] - P;
    const float3 v1_p = V[1] - P;
    const float3 v2_p = V[2] - P;

    const float3 u01 = safe_normalize(cross(v0_p, v1_p));
    const float3 u02 = safe_normalize(cross(v0_p, v2_p));
    const float3 u12 = safe_normalize(cross(v1_p, v2_p));

    const float3 A = safe_normalize(v0_p);
    const float3 B = safe_normalize(v1_p);
    const float3 C = safe_normalize(v2_p);

    const float cos_alpha = dot(u02, u01);
    const float cos_beta = -dot(u01, u12);
    const float cos_gamma = dot(u02, u12);

    /* calculate dihedral angles */
    const float alpha = fast_acosf(cos_alpha);
    const float beta = fast_acosf(cos_beta);
    const float gamma = fast_acosf(cos_gamma);
    /* the area of the unit spherical triangle = solid angle */
    const float solid_angle = alpha + beta + gamma - M_PI_F;

    /* precompute a few things
     * these could be re-used to take several samples
     * as they are independent of randu/randv */
    const float cos_c = dot(A, B);
    const float sin_alpha = fast_sinf(alpha);
    const float product = sin_alpha * cos_c;

    /* Select a random sub-area of the spherical triangle
     * and calculate the third vertex C_ of that new triangle */
    const float phi = randu * solid_angle - alpha;
    float s, t;
    fast_sincosf(phi, &s, &t);
    const float u = t - cos_alpha;
    const float v = s + product;

    const float3 U = safe_normalize(C - dot(C, A) * A);

    float q = 1.0f;
    const float det = ((v * s + u * t) * sin_alpha);
    if (det != 0.0f) {
      q = ((v * t - u * s) * cos_alpha - v) / det;
    }
    const float temp = max(1.0f - q * q, 0.0f);

    const float3 C_ = safe_normalize(q * A + sqrtf(temp) * U);

    /* Finally, select a random point along the edge of the new triangle
     * That point on the spherical triangle is the sampled ray direction */
    const float z = 1.0f - randv * (1.0f - dot(C_, B));
    ls->D = z * B + safe_sqrtf(1.0f - z * z) * safe_normalize(C_ - dot(C_, B) * B);

    /* calculate intersection with the planar triangle */
    if (!ray_triangle_intersect(
            P, ls->D, 0.0f, FLT_MAX, V[0], V[1], V[2], &ls->u, &ls->v, &ls->t)) {
      ls->pdf = 0.0f;
      return false;
    }

    ls->P = P + ls->D * ls->t;

    /* distribution_pdf_triangles is calculated over triangle area, but we're sampling over solid
     * angle */
    if (UNLIKELY(solid_angle == 0.0f)) {
      ls->pdf = 0.0f;
      return false;
    }
    else {
      ls->pdf = 1.0f / solid_angle;
    }
  }
  else {
    if (UNLIKELY(area == 0.0f)) {
      return 0.0f;
    }

    /* compute random point in triangle. From Eric Heitz's "A Low-Distortion Map Between Triangle
     * and Square" */
    float u = randu;
    float v = randv;
    if (v > u) {
      u *= 0.5f;
      v -= u;
    }
    else {
      v *= 0.5f;
      u -= v;
    }

    const float t = 1.0f - u - v;
    ls->P = u * V[0] + v * V[1] + t * V[2];
    /* compute incoming direction, distance and pdf */
    ls->D = normalize_len(ls->P - P, &ls->t);
    ls->pdf = triangle_light_pdf_area_sampling(ls->Ng, -ls->D, ls->t) / area;
    ls->u = u;
    ls->v = v;
  }

  /* Belongs in distribution.h but can reuse computations here. */
  if (!kernel_data.integrator.use_light_tree) {
    float distribution_area = area;

    if (has_motion && area != 0.0f) {
      /* For motion blur need area of triangle at fixed time as used in the CDF. */
      triangle_world_space_vertices(kg, object, prim, -1.0f, V);
      distribution_area = triangle_area(V[0], V[1], V[2]);
    }

    ls->pdf_selection = distribution_area * kernel_data.integrator.distribution_pdf_triangles;
  }

  return (ls->pdf > 0.0f);
}

template<bool in_volume_segment>
ccl_device_forceinline bool triangle_light_tree_parameters(
    KernelGlobals kg,
    const ccl_global KernelLightTreeEmitter *kemitter,
    const float3 centroid,
    const float3 P,
    const float3 N,
    const BoundingCone bcone,
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

  cos_theta_u = FLT_MAX;

  const int object = kemitter->mesh_light.object_id;
  float3 vertices[3];
  triangle_world_space_vertices(kg, object, kemitter->prim, -1.0f, vertices);

  bool shape_above_surface = false;
  for (int i = 0; i < 3; i++) {
    const float3 corner = vertices[i];
    float distance_point_to_corner;
    const float3 point_to_corner = safe_normalize_len(corner - P, &distance_point_to_corner);
    cos_theta_u = fminf(cos_theta_u, dot(point_to_centroid, point_to_corner));
    shape_above_surface |= dot(point_to_corner, N) > 0;
    if (!in_volume_segment) {
      distance.x = fmaxf(distance.x, distance_point_to_corner);
    }
  }

  const bool front_facing = bcone.theta_o != 0.0f || dot(bcone.axis, point_to_centroid) < 0;
  const bool in_volume = is_zero(N);

  return (front_facing && shape_above_surface) || in_volume;
}

CCL_NAMESPACE_END
