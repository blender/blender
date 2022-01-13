/*
 * Copyright 2011-2016 Blender Foundation
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

CCL_NAMESPACE_BEGIN

/**
 * Use the barycentric coordinates to get the intersection location
 */
ccl_device_inline float3 motion_triangle_point_from_uv(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       const int isect_object,
                                                       const int isect_prim,
                                                       const float u,
                                                       const float v,
                                                       float3 verts[3])
{
  float w = 1.0f - u - v;
  float3 P = u * verts[0] + v * verts[1] + w * verts[2];

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
                                                 float3 P,
                                                 float3 dir,
                                                 float tmax,
                                                 float time,
                                                 uint visibility,
                                                 int object,
                                                 int prim,
                                                 int prim_addr)
{
  /* Get vertex locations for intersection. */
  float3 verts[3];
  motion_triangle_vertices(kg, object, prim, time, verts);
  /* Ray-triangle intersection, unoptimized. */
  float t, u, v;
  if (ray_triangle_intersect(P, dir, tmax, verts[0], verts[1], verts[2], &u, &v, &t)) {
#ifdef __VISIBILITY_FLAG__
    /* Visibility flag test. we do it here under the assumption
     * that most triangles are culled by node flags.
     */
    if (kernel_tex_fetch(__prim_visibility, prim_addr) & visibility)
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
                                                       float3 P,
                                                       float3 dir,
                                                       float time,
                                                       int object,
                                                       int prim,
                                                       int prim_addr,
                                                       float tmax,
                                                       ccl_private uint *lcg_state,
                                                       int max_hits)
{
  /* Get vertex locations for intersection. */
  float3 verts[3];
  motion_triangle_vertices(kg, object, prim, time, verts);
  /* Ray-triangle intersection, unoptimized. */
  float t, u, v;
  if (!ray_triangle_intersect(P, dir, tmax, verts[0], verts[1], verts[2], &u, &v, &t)) {
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

      if (hit >= max_hits)
        return false;
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
