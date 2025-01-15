/* SPDX-FileCopyrightText: 2014-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Triangle/Ray intersections.
 *
 * For BVH ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage.
 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/object.h"
#include "kernel/geom/triangle.h"
#include "kernel/sample/lcg.h"

#include "util/math_float3.h"
#include "util/math_intersect.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool triangle_intersect(KernelGlobals kg,
                                          ccl_private Intersection *isect,
                                          const float3 P,
                                          const float3 dir,
                                          const float tmin,
                                          const float tmax,
                                          const uint visibility,
                                          const int object,
                                          const int prim,
                                          const int prim_addr)
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  const float3 tri_a = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 tri_b = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 tri_c = kernel_data_fetch(tri_verts, tri_vindex.z);

  float t;
  float u;
  float v;
  if (ray_triangle_intersect(P, dir, tmin, tmax, tri_a, tri_b, tri_c, &u, &v, &t)) {
#ifdef __VISIBILITY_FLAG__
    /* Visibility flag test. we do it here under the assumption
     * that most triangles are culled by node flags.
     */
    if (kernel_data_fetch(prim_visibility, prim_addr) & visibility)
#endif
    {
      isect->object = object;
      isect->prim = prim;
      isect->type = PRIMITIVE_TRIANGLE;
      isect->u = u;
      isect->v = v;
      isect->t = t;
      return true;
    }
  }
  return false;
}

/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point.
 * Returns whether traversal should be stopped.
 */

#ifdef __BVH_LOCAL__
ccl_device_inline bool triangle_intersect_local(KernelGlobals kg,
                                                ccl_private LocalIntersection *local_isect,
                                                const float3 P,
                                                const float3 dir,
                                                const int object,
                                                const int prim,
                                                const int prim_addr,
                                                const float tmin,
                                                const float tmax,
                                                ccl_private uint *lcg_state,
                                                const int max_hits)
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  const float3 tri_a = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 tri_b = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 tri_c = kernel_data_fetch(tri_verts, tri_vindex.z);

  float t;
  float u;
  float v;
  if (!ray_triangle_intersect(P, dir, tmin, tmax, tri_a, tri_b, tri_c, &u, &v, &t)) {
    return false;
  }

  /* If no actual hit information is requested, just return here. */
  if (max_hits == 0) {
    return true;
  }

  int hit;
  if (lcg_state) {
    /* Record up to max_hits intersections. */
    for (int i = min(max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
      if (local_isect->hits[i].t == t) {
        return false;
      }
    }

    local_isect->num_hits++;

    if (local_isect->num_hits <= max_hits) {
      hit = local_isect->num_hits - 1;
    }
    else {
      /* reservoir sampling: if we are at the maximum number of
       * hits, randomly replace element or skip it */
      hit = lcg_step_uint(lcg_state) % local_isect->num_hits;

      if (hit >= max_hits) {
        return false;
      }
    }
  }
  else {
    /* Record closest intersection only. */
    if (local_isect->num_hits && t > local_isect->hits[0].t) {
      return false;
    }

    hit = 0;
    local_isect->num_hits = 1;
  }

  /* Record intersection. */
  ccl_private Intersection *isect = &local_isect->hits[hit];
  isect->prim = prim;
  isect->object = object;
  isect->type = PRIMITIVE_TRIANGLE;
  isect->u = u;
  isect->v = v;
  isect->t = t;

  /* Record geometric normal. */
  local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  return false;
}
#endif /* __BVH_LOCAL__ */

/**
 * Use the barycentric coordinates to get the intersection location
 */
ccl_device_inline float3 triangle_point_from_uv(KernelGlobals kg,
                                                ccl_private ShaderData *sd,
                                                const int isect_prim,
                                                const float u,
                                                const float v)
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, isect_prim);
  const float3 tri_a = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 tri_b = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 tri_c = kernel_data_fetch(tri_verts, tri_vindex.z);

  /* This appears to give slightly better precision than interpolating with w = (1 - u - v). */
  float3 P = tri_a + u * (tri_b - tri_a) + v * (tri_c - tri_a);

  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    const Transform tfm = object_get_transform(kg, sd);
    P = transform_point(&tfm, P);
  }

  return P;
}

ccl_device_inline void triangle_shader_setup(KernelGlobals kg, ccl_private ShaderData *sd)
{
  sd->shader = kernel_data_fetch(tri_shader, sd->prim);

  sd->P = triangle_point_from_uv(kg, sd, sd->prim, sd->u, sd->v);

  /* Normals. */
  const float3 Ng = triangle_normal(kg, sd);
  sd->Ng = Ng;
  sd->N = Ng;

  /* Smooth normal. */
  if (sd->shader & SHADER_SMOOTH_NORMAL) {
    sd->N = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v);
  }

#ifdef __DPDU__
  /* dPdu/dPdv */
  triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);
#endif
}

CCL_NAMESPACE_END
