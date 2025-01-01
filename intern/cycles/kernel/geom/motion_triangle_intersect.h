/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Motion Triangle Primitive
 *
 * These are stored as regular triangles, plus extra positions and normals at
 * times other than the frame center. Computing the triangle vertex positions
 * or normals at a given ray time is a matter of interpolation of the two steps
 * between which the ray time lies.
 *
 * The extra positions and normals are stored as ATTR_STD_MOTION_VERTEX_POSITION
 * and ATTR_STD_MOTION_VERTEX_NORMAL mesh attributes.
 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/object.h"

#include "kernel/sample/lcg.h"

#include "util/math_intersect.h"

CCL_NAMESPACE_BEGIN

/**
 * Use the barycentric coordinates to get the intersection location
 */
ccl_device_inline float3 motion_triangle_point_from_uv(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       const float u,
                                                       const float v,
                                                       const float3 verts[3])
{
  /* This appears to give slightly better precision than interpolating with w = (1 - u - v). */
  float3 P = verts[0] + u * (verts[1] - verts[0]) + v * (verts[2] - verts[0]);

  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    const Transform tfm = object_get_transform(kg, sd);
    P = transform_point(&tfm, P);
  }

  return P;
}

/* Ray intersection. We simply compute the vertex positions at the given ray
 * time and do a ray intersection with the resulting triangle.
 */

ccl_device_inline bool motion_triangle_intersect(KernelGlobals kg,
                                                 ccl_private Intersection *isect,
                                                 const float3 P,
                                                 const float3 dir,
                                                 const float tmin,
                                                 const float tmax,
                                                 const float time,
                                                 const uint visibility,
                                                 const int object,
                                                 const int prim,
                                                 const int prim_addr)
{
  /* Get vertex locations for intersection. */
  float3 verts[3];
  motion_triangle_vertices(kg, object, prim, time, verts);
  /* Ray-triangle intersection, unoptimized. */
  float t;
  float u;
  float v;
  if (ray_triangle_intersect(P, dir, tmin, tmax, verts[0], verts[1], verts[2], &u, &v, &t)) {
#ifdef __VISIBILITY_FLAG__
    /* Visibility flag test. we do it here under the assumption
     * that most triangles are culled by node flags.
     */
    if (kernel_data_fetch(prim_visibility, prim_addr) & visibility)
#endif
    {
      isect->t = t;
      isect->u = u;
      isect->v = v;
      isect->prim = prim;
      isect->object = object;
      isect->type = PRIMITIVE_MOTION_TRIANGLE;
      return true;
    }
  }
  return false;
}

/* Special ray intersection routines for local intersections. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point.
 * Returns whether traversal should be stopped.
 */
#ifdef __BVH_LOCAL__
ccl_device_inline bool motion_triangle_intersect_local(KernelGlobals kg,
                                                       ccl_private LocalIntersection *local_isect,
                                                       const float3 P,
                                                       const float3 dir,
                                                       const float time,
                                                       const int object,
                                                       const int prim,
                                                       const int prim_addr,
                                                       const float tmin,
                                                       const float tmax,
                                                       ccl_private uint *lcg_state,
                                                       const int max_hits)
{
  /* Get vertex locations for intersection. */
  float3 verts[3];
  motion_triangle_vertices(kg, object, prim, time, verts);
  /* Ray-triangle intersection, unoptimized. */
  float t;
  float u;
  float v;
  if (!ray_triangle_intersect(P, dir, tmin, tmax, verts[0], verts[1], verts[2], &u, &v, &t)) {
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
      /* Reservoir sampling: if we are at the maximum number of
       * hits, randomly replace element or skip it.
       */
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
  isect->t = t;
  isect->u = u;
  isect->v = v;
  isect->prim = prim;
  isect->object = object;
  isect->type = PRIMITIVE_MOTION_TRIANGLE;

  /* Record geometric normal. */
  local_isect->Ng[hit] = normalize(cross(verts[1] - verts[0], verts[2] - verts[0]));

  return false;
}
#endif /* __BVH_LOCAL__ */

CCL_NAMESPACE_END
