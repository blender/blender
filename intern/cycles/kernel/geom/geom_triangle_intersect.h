/*
 * Copyright 2014, Blender Foundation.
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

/* Triangle/Ray intersections.
 *
 * For BVH ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage.
 */

CCL_NAMESPACE_BEGIN

ccl_device_inline bool triangle_intersect(KernelGlobals *kg,
                                          Intersection *isect,
                                          float3 P,
                                          float3 dir,
                                          uint visibility,
                                          int object,
                                          int prim_addr)
{
  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, prim_addr);
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const ssef *ssef_verts = (ssef *)&kg->__prim_tri_verts.data[tri_vindex];
#else
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
#endif
  float t, u, v;
  if (ray_triangle_intersect(P,
                             dir,
                             isect->t,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
                             ssef_verts,
#else
                             float4_to_float3(tri_a),
                             float4_to_float3(tri_b),
                             float4_to_float3(tri_c),
#endif
                             &u,
                             &v,
                             &t)) {
#ifdef __VISIBILITY_FLAG__
    /* Visibility flag test. we do it here under the assumption
     * that most triangles are culled by node flags.
     */
    if (kernel_tex_fetch(__prim_visibility, prim_addr) & visibility)
#endif
    {
      isect->prim = prim_addr;
      isect->object = object;
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
ccl_device_inline bool triangle_intersect_local(KernelGlobals *kg,
                                                LocalIntersection *local_isect,
                                                float3 P,
                                                float3 dir,
                                                int object,
                                                int local_object,
                                                int prim_addr,
                                                float tmax,
                                                uint *lcg_state,
                                                int max_hits)
{
  /* Only intersect with matching object, for instanced objects we
   * already know we are only intersecting the right object. */
  if (object == OBJECT_NONE) {
    if (kernel_tex_fetch(__prim_object, prim_addr) != local_object) {
      return false;
    }
  }

  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, prim_addr);
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const ssef *ssef_verts = (ssef *)&kg->__prim_tri_verts.data[tri_vindex];
#  else
  const float3 tri_a = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0)),
               tri_b = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1)),
               tri_c = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2));
#  endif
  float t, u, v;
  if (!ray_triangle_intersect(P,
                              dir,
                              tmax,
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
                              ssef_verts,
#  else
                              tri_a,
                              tri_b,
                              tri_c,
#  endif
                              &u,
                              &v,
                              &t)) {
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
  Intersection *isect = &local_isect->hits[hit];
  isect->prim = prim_addr;
  isect->object = object;
  isect->type = PRIMITIVE_TRIANGLE;
  isect->u = u;
  isect->v = v;
  isect->t = t;

  /* Record geometric normal. */
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const float3 tri_a = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0)),
               tri_b = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1)),
               tri_c = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2));
#  endif
  local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  return false;
}
#endif /* __BVH_LOCAL__ */

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

/* Reintersections uses the paper:
 *
 * Tomas Moeller
 * Fast, minimum storage ray/triangle intersection
 * http://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
 */

ccl_device_inline float3 triangle_refine(KernelGlobals *kg,
                                         ShaderData *sd,
                                         const Intersection *isect,
                                         const Ray *ray)
{
  float3 P = ray->P;
  float3 D = ray->D;
  float t = isect->t;

#ifdef __INTERSECTION_REFINE__
  if (isect->object != OBJECT_NONE) {
    if (UNLIKELY(t == 0.0f)) {
      return P;
    }
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D * t);
    D = normalize_len(D, &t);
  }

  P = P + D * t;

  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, isect->prim);
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
  float3 edge1 = make_float3(tri_a.x - tri_c.x, tri_a.y - tri_c.y, tri_a.z - tri_c.z);
  float3 edge2 = make_float3(tri_b.x - tri_c.x, tri_b.y - tri_c.y, tri_b.z - tri_c.z);
  float3 tvec = make_float3(P.x - tri_c.x, P.y - tri_c.y, P.z - tri_c.z);
  float3 qvec = cross(tvec, edge1);
  float3 pvec = cross(D, edge2);
  float det = dot(edge1, pvec);
  if (det != 0.0f) {
    /* If determinant is zero it means ray lies in the plane of
     * the triangle. It is possible in theory due to watertight
     * nature of triangle intersection. For such cases we simply
     * don't refine intersection hoping it'll go all fine.
     */
    float rt = dot(edge2, qvec) / det;
    P = P + D * rt;
  }

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
  }

  return P;
#else
  return P + D * t;
#endif
}

/* Same as above, except that isect->t is assumed to be in object space for
 * instancing.
 */
ccl_device_inline float3 triangle_refine_local(KernelGlobals *kg,
                                               ShaderData *sd,
                                               const Intersection *isect,
                                               const Ray *ray)
{
#ifdef __KERNEL_OPTIX__
  /* isect->t is always in world space with OptiX. */
  return triangle_refine(kg, sd, isect, ray);
#else
  float3 P = ray->P;
  float3 D = ray->D;
  float t = isect->t;

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D);
    D = normalize(D);
  }

  P = P + D * t;

#  ifdef __INTERSECTION_REFINE__
  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, isect->prim);
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
  float3 edge1 = make_float3(tri_a.x - tri_c.x, tri_a.y - tri_c.y, tri_a.z - tri_c.z);
  float3 edge2 = make_float3(tri_b.x - tri_c.x, tri_b.y - tri_c.y, tri_b.z - tri_c.z);
  float3 tvec = make_float3(P.x - tri_c.x, P.y - tri_c.y, P.z - tri_c.z);
  float3 qvec = cross(tvec, edge1);
  float3 pvec = cross(D, edge2);
  float det = dot(edge1, pvec);
  if (det != 0.0f) {
    /* If determinant is zero it means ray lies in the plane of
     * the triangle. It is possible in theory due to watertight
     * nature of triangle intersection. For such cases we simply
     * don't refine intersection hoping it'll go all fine.
     */
    float rt = dot(edge2, qvec) / det;
    P = P + D * rt;
  }
#  endif /* __INTERSECTION_REFINE__ */

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
  }

  return P;
#endif
}

CCL_NAMESPACE_END
