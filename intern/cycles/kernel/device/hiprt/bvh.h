/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

struct RayPayload {
  RaySelfPrimitives self;
  uint visibility;
  float ray_time;
};

/* Some ray types might use the same intersection function for regular and shadow intersections,
 * but have different filter functions for them. To make this code simpler subclass from
 * RayPayload.
 *
 * NOTE: This assumes that reinterpret_cast from void pointer to RayPayload works correctly. */
struct ShadowPayload : RayPayload {
  int in_state;
  uint max_transparent_hits;
  uint num_transparent_hits;
  uint *r_num_recorded_hits;
  float *r_throughput;
};

struct LocalPayload {
  RaySelfPrimitives self;
  float ray_time;
  int local_object;
  uint max_hits;
  uint *lcg_state;
  LocalIntersection *local_isect;
};

/* --------------------------------------------------------------------
 * Utilities.
 */

ccl_device_forceinline void set_hiprt_ray(const ccl_private Ray &ray,
                                          ccl_private hiprtRay &ray_hip)
{
  ray_hip.direction = ray.D;
  ray_hip.origin = ray.P;
  ray_hip.maxT = ray.tmax;
  ray_hip.minT = ray.tmin;
}

ccl_device_inline void set_intersect_point(const hiprtHit &hit, ccl_private Intersection *isect)
{
  const int object = kernel_data_fetch(user_instance_id, hit.instanceID);

  isect->t = hit.t;
  isect->u = hit.uv.x;
  isect->v = hit.uv.y;

  isect->object = object;
  isect->type = kernel_data_fetch(objects, object).primitive_type;

  if (isect->type & PRIMITIVE_CURVE) {
    /* For curves the isect->type is a packed segment information, which is different from the
     * primitive type associated with the object. */

    /* TODO(sergey): Try to solve this with less fetches.
     *
     * Ideally avoid having HIP-RT specific custom_prim_info tables, allowing them to be removed
     * in order to minimize the memory usage. */

    const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object);
    const int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
    isect->prim = prim_info.x + data_offset.y;
    isect->type = prim_info.y;
  }
  else {
    const int prim_offset = kernel_data_fetch(object_prim_offset, object);
    isect->prim = hit.primID + prim_offset;
  }
}

/* --------------------------------------------------------------------
 * Custom intersection functions.
 */

ccl_device_inline bool curve_custom_intersect(const hiprtRay &ray,
                                              RayPayload *payload,
                                              hiprtHit &hit)

{
  /* Could also cast shadow payload to get the elements needed to do the intersection no need to
   * write a separate function for shadow intersection. */

  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);

  /* `data_offset.x`: where the data (prim id, type )for the geometry of the current object begins
   * the prim_id that is in hiprtHit hit is local to the particular geometry so we add the above
   * `ofstream` to map prim id in hiprtHit to the one compatible to what next stage expects
   * `data_offset.y`: the offset that has to be added to a local primitive to get the global
   * `primitive id = kernel_data_fetch(object_prim_offset, object_id);` */
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);

  const int prim_offset = data_offset.y;

  const int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
  const int curve_index = prim_info.x;
  const int key_value = prim_info.y;

#ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, payload->self, object_id)) {
    return false; /* Ignore hit - continue traversal. */
  }
#endif

  if (intersection_skip_self_shadow(payload->self, object_id, curve_index + prim_offset)) {
    return false;
  }

  const float ray_time = payload->ray_time;

  if ((key_value & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {
    const int time_offset = kernel_data_fetch(prim_time_offset, object_id);
    const float2 prims_time = kernel_data_fetch(prims_time, hit.primID + time_offset);
    if (ray_time < prims_time.x || ray_time > prims_time.y) {
      return false;
    }
  }

  Intersection isect;
  const bool b_hit = curve_intersect(kg,
                                     &isect,
                                     ray.origin,
                                     ray.direction,
                                     ray.minT,
                                     ray.maxT,
                                     object_id,
                                     curve_index + prim_offset,
                                     ray_time,
                                     key_value);
  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
  }

  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_intersect(const hiprtRay &ray,
                                                        RayPayload *payload,
                                                        hiprtHit &hit)
{
  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  const int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  const int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_shadow(payload->self, object_id, prim_id_global)) {
    return false;
  }

  Intersection isect;
  const bool b_hit = motion_triangle_intersect(kg,
                                               &isect,
                                               ray.origin,
                                               ray.direction,
                                               ray.minT,
                                               ray.maxT,
                                               payload->ray_time,
                                               payload->visibility,
                                               object_id,
                                               prim_id_global,
                                               hit.instanceID);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
  }

  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_local_intersect(const hiprtRay &ray,
                                                              LocalPayload *payload,
                                                              hiprtHit &hit)
{
#ifdef __OBJECT_MOTION__
  KernelGlobals kg = nullptr;

  const int object_id = payload->local_object;
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);

  const int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  const int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_local(payload->self, prim_id_global)) {
    return false;
  }

  return motion_triangle_intersect_local(kg,
                                         payload->local_isect,
                                         ray.origin,
                                         ray.direction,
                                         payload->ray_time,
                                         object_id,
                                         prim_id_global,
                                         ray.minT,
                                         ray.maxT,
                                         payload->lcg_state,
                                         payload->max_hits);

#else
  return false;
#endif
}

ccl_device_inline bool motion_triangle_custom_volume_intersect(const hiprtRay &ray,
                                                               RayPayload *payload,
                                                               hiprtHit &hit)
{
#ifdef __OBJECT_MOTION__
  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const uint object_flag = kernel_data_fetch(object_flag, object_id);

  if (!(object_flag & SD_OBJECT_HAS_VOLUME)) {
    return false;
  }

  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  const int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  const int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_shadow(payload->self, object_id, prim_id_global)) {
    return false;
  }

  const int shader_flag = intersection_get_shader_flags(kg, prim_id_global, PRIMITIVE_TRIANGLE);
  if (!(shader_flag & SD_HAS_VOLUME)) {
    return false;
  }

  Intersection isect;
  const bool b_hit = motion_triangle_intersect(kg,
                                               &isect,
                                               ray.origin,
                                               ray.direction,
                                               ray.minT,
                                               ray.maxT,
                                               payload->ray_time,
                                               payload->visibility,
                                               object_id,
                                               prim_id_global,
                                               prim_id_local);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
  }

  return b_hit;
#else
  return false;
#endif
}

ccl_device_inline bool point_custom_intersect(const hiprtRay &ray,
                                              RayPayload *payload,
                                              hiprtHit &hit)
{
#if defined(__POINTCLOUD__)
  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  const int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
  const int prim_id_local = prim_info.x;
  const int prim_id_global = prim_id_local + prim_offset;

  const int primitive_type = prim_info.y;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, payload->self, object_id)) {
    return false; /* Ignore hit - continue traversal */
  }
#  endif

  if (intersection_skip_self_shadow(payload->self, object_id, prim_id_global)) {
    return false;
  }

  const float ray_time = payload->ray_time;

  if ((primitive_type & PRIMITIVE_MOTION_POINT) && kernel_data.bvh.use_bvh_steps) {
    const int time_offset = kernel_data_fetch(prim_time_offset, object_id);
    const float2 prims_time = kernel_data_fetch(prims_time, hit.primID + time_offset);
    if (ray_time < prims_time.x || ray_time > prims_time.y) {
      return false;
    }
  }

  Intersection isect;
  const bool b_hit = point_intersect(kg,
                                     &isect,
                                     ray.origin,
                                     ray.direction,
                                     ray.minT,
                                     ray.maxT,
                                     object_id,
                                     prim_id_global,
                                     ray_time,
                                     primitive_type);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
  }

  return b_hit;
#else
  return false;
#endif
}

/* --------------------------------------------------------------------
 * Intersection filters.
 */

ccl_device_inline bool closest_intersection_filter(const hiprtRay &ray,
                                                   RayPayload *payload,
                                                   const hiprtHit &hit)
{
  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int prim = hit.primID + prim_offset;

#ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, payload->self, object_id)) {
    return true; /* Ignore hit - continue traversal. */
  }
#endif

  if (intersection_skip_self_shadow(payload->self, object_id, prim)) {
    return true; /* Ignore hit - continue traversal. */
  }

  return false;
}

ccl_device_inline bool shadow_intersection_filter(const hiprtRay &ray,
                                                  ShadowPayload *payload,
                                                  const hiprtHit &hit)

{
  KernelGlobals kg = nullptr;

  uint num_transparent_hits = payload->num_transparent_hits;
  const uint max_transparent_hits = payload->max_transparent_hits;
  const int state = payload->in_state;
  const RaySelfPrimitives &self = payload->self;

  const int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);
  const int prim = hit.primID + prim_offset;

  const float ray_tmax = hit.t;

#ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, self, object)) {
    return true; /* Ignore hit - continue traversal */
  }
#endif

#ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true; /* No hit - continue traversal. */
  }
#endif

  if (intersection_skip_self_shadow(self, object, prim)) {
    return true; /* No hit -continue traversal. */
  }

  if (intersection_skip_shadow_already_recoded(state, object, prim, *payload->r_num_recorded_hits))
  {
    return true;
  }

  const float u = hit.uv.x;
  const float v = hit.uv.y;
  const int primitive_type = kernel_data_fetch(objects, object).primitive_type;

#ifndef __TRANSPARENT_SHADOWS__
  *payload->r_throughput = 0.0f;
  return false;
#else
  const int flags = intersection_get_shader_flags(kg, prim, primitive_type);
  if (!(flags & SD_HAS_TRANSPARENT_SHADOW)) {
    *payload->r_throughput = 0.0f;
    return false;
  }

  num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (num_transparent_hits > max_transparent_hits) {
    *payload->r_throughput = 0.0f;
    return false;
  }

  uint record_index = *payload->r_num_recorded_hits;

  payload->num_transparent_hits = num_transparent_hits;
  *(payload->r_num_recorded_hits) += 1;

  const uint max_record_hits = INTEGRATOR_SHADOW_ISECT_SIZE;
  if (record_index >= max_record_hits) {
    float max_recorded_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, t);
    uint max_recorded_hit = 0;

    for (int i = 1; i < max_record_hits; i++) {
      const float isect_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, i, t);
      if (isect_t > max_recorded_t) {
        max_recorded_t = isect_t;
        max_recorded_hit = i;
      }
    }

    if (ray_tmax >= max_recorded_t) {
      return true;
    }

    record_index = max_recorded_hit;
  }

  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, u) = u;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, v) = v;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, t) = ray_tmax;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, prim) = prim;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, object) = object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, type) = primitive_type;

  return true;
#endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool shadow_intersection_filter_curves(const hiprtRay &ray,
                                                         ShadowPayload *payload,
                                                         const hiprtHit &hit)

{
  KernelGlobals kg = nullptr;

  uint num_transparent_hits = payload->num_transparent_hits;
  const uint num_recorded_hits = *(payload->r_num_recorded_hits);
  const uint max_transparent_hits = payload->max_transparent_hits;
  const RaySelfPrimitives &self = payload->self;

  const int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object);
  const int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
  const int prim = prim_info.x + data_offset.y;

  const float ray_tmax = hit.t;

#ifdef __SHADOW_LINKING__
  /* It doesn't seem like this is necessary. */
  if (intersection_skip_shadow_link(kg, self, object)) {
    return true; /* Ignore hit - continue traversal. */
  }
#endif

#ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true; /* No hit - continue traversal. */
  }
#endif

  if (intersection_skip_self_shadow(self, object, prim)) {
    return true; /* No hit -continue traversal. */
  }

  /* FIXME: transparent curves are not recorded, this check doesn't work. */
  if (intersection_skip_shadow_already_recoded(payload->in_state, object, prim, num_recorded_hits))
  {
    return true;
  }

  const float u = hit.uv.x;
  const float v = hit.uv.y;

  if (u == 0.0f || u == 1.0f) {
    return true; /* Continue traversal. */
  }

  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
  const int primitive_type = segment.type;

#ifndef __TRANSPARENT_SHADOWS__
  *payload->r_throughput = 0.0f;
  return false;
#else
  const int flags = intersection_get_shader_flags(kg, prim, primitive_type);
  if (!(flags & SD_HAS_TRANSPARENT_SHADOW)) {
    *payload->r_throughput = 0.0f;
    return false;
  }

  num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (num_transparent_hits > max_transparent_hits) {
    *payload->r_throughput = 0.0f;
    return false;
  }

  float throughput = *payload->r_throughput;
  throughput *= intersection_curve_shadow_transparency(kg, object, prim, primitive_type, u);
  *payload->r_throughput = throughput;
  payload->num_transparent_hits = num_transparent_hits;

  if (throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
    *payload->r_throughput = 0.0f;
    return false;
  }

  return true;
#endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool local_intersection_filter(const hiprtRay &ray,
                                                 LocalPayload *payload,
                                                 const hiprtHit &hit)
{
#ifdef __BVH_LOCAL__
  KernelGlobals kg = nullptr;

  const int object_id = payload->local_object;
  const uint max_hits = payload->max_hits;

  /* Triangle primitive uses hardware intersection, other primitives  do custom intersection
   * which does reservoir sampling for intersections. For the custom primitives only check
   * whether we can stop traversal early on. The rest of the checks here only do for the
   * regular triangles. */
  const int primitive_type = kernel_data_fetch(objects, object_id).primitive_type;
  if (primitive_type != PRIMITIVE_TRIANGLE) {
    if (max_hits == 0) {
      return false;
    }
    return true;
  }

  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int prim = hit.primID + prim_offset;
#  ifndef __RAY_OFFSET__
  if (intersection_skip_self_local(payload->self, prim)) {
    return true; /* Continue search. */
  }
#  endif

  if (max_hits == 0) {
    return false; /* Stop search. */
  }

  const int hit_index = local_intersect_get_record_index(
      payload->local_isect, hit.t, payload->lcg_state, max_hits);
  if (hit_index == -1) {
    return true; /* Continue search. */
  }

  Intersection *isect = &payload->local_isect->hits[hit_index];
  isect->t = hit.t;
  isect->u = hit.uv.x;
  isect->v = hit.uv.y;
  isect->prim = prim;
  isect->object = object_id;
  isect->type = primitive_type;

  payload->local_isect->Ng[hit_index] = hit.normal;

  return true;
#else
  return false;
#endif
}

ccl_device_inline bool volume_intersection_filter(const hiprtRay &ray,
                                                  RayPayload *payload,
                                                  const hiprtHit &hit)
{
  KernelGlobals kg = nullptr;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int prim = hit.primID + prim_offset;
  if (intersection_skip_self(payload->self, object_id, prim)) {
    return true;
  }

  const int shader_flag = intersection_get_shader_flags(kg, prim, PRIMITIVE_TRIANGLE);
  if (!(shader_flag & SD_HAS_VOLUME)) {
    return true;
  }
  return false;
}

/* --------------------------------------------------------------------
 * HIP-RT device functions.
 */

HIPRT_DEVICE bool intersectFunc(const uint geom_type,
                                const uint ray_type,
                                const hiprtFuncTableHeader &tableHeader,
                                const hiprtRay &ray,
                                void *payload,
                                hiprtHit &hit)
{
  const uint index = tableHeader.numGeomTypes * ray_type + geom_type;
  switch (index) {
    case Curve_Intersect_Function:
    case Curve_Intersect_Shadow:
      return curve_custom_intersect(ray, (RayPayload *)payload, hit);
    case Motion_Triangle_Intersect_Function:
    case Motion_Triangle_Intersect_Shadow:
      return motion_triangle_custom_intersect(ray, (RayPayload *)payload, hit);
    case Motion_Triangle_Intersect_Local:
      return motion_triangle_custom_local_intersect(ray, (LocalPayload *)payload, hit);
    case Motion_Triangle_Intersect_Volume:
      return motion_triangle_custom_volume_intersect(ray, (RayPayload *)payload, hit);
    case Point_Intersect_Function:
    case Point_Intersect_Shadow:
      return point_custom_intersect(ray, (RayPayload *)payload, hit);
    default:
      break;
  }
  return false;
}

HIPRT_DEVICE bool filterFunc(const uint geom_type,
                             const uint ray_type,
                             const hiprtFuncTableHeader &tableHeader,
                             const hiprtRay &ray,
                             void *payload,
                             const hiprtHit &hit)
{
  const uint index = tableHeader.numGeomTypes * ray_type + geom_type;
  switch (index) {
    case Triangle_Filter_Closest:
      return closest_intersection_filter(ray, (RayPayload *)payload, hit);
    case Curve_Filter_Shadow:
      return shadow_intersection_filter_curves(ray, (ShadowPayload *)payload, hit);
    case Triangle_Filter_Shadow:
    case Motion_Triangle_Filter_Shadow:
    case Point_Filter_Shadow:
      return shadow_intersection_filter(ray, (ShadowPayload *)payload, hit);
    case Triangle_Filter_Local:
    case Motion_Triangle_Filter_Local:
      return local_intersection_filter(ray, (LocalPayload *)payload, hit);
    case Triangle_Filter_Volume:
    case Motion_Triangle_Filter_Volume:
      return volume_intersection_filter(ray, (RayPayload *)payload, hit);
    default:
      break;
  }
  return false;
}

/* --------------------------------------------------------------------
 * BVH functions.
 */

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          const ccl_private Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  isect->t = ray->tmax;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;
  isect->type = PRIMITIVE_NONE;
  if (!intersection_ray_valid(ray)) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;
    return false;
  }

  if (kernel_data.device_bvh == 0) {
    return false;
  }

  hiprtRay ray_hip;
  set_hiprt_ray(*ray, ray_hip);

  RayPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;
  payload.ray_time = ray->time;

  Stack stack(kg->global_stack_buffer, kg->shared_stack);
  Instance_Stack instance_stack;

  hiprtHit hit;
  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    hiprtSceneTraversalAnyHitCustomStack traversal((hiprtScene)kernel_data.device_bvh,
                                                   ray_hip,
                                                   stack,
                                                   instance_stack,
                                                   visibility,
                                                   hiprtTraversalHintDefault,
                                                   &payload,
                                                   kernel_params.table_closest_intersect,
                                                   0 /* RAY_TYPE */,
                                                   ray->time);
    hit = traversal.getNextHit();
  }
  else {
    hiprtSceneTraversalClosestCustomStack traversal((hiprtScene)kernel_data.device_bvh,
                                                    ray_hip,
                                                    stack,
                                                    instance_stack,
                                                    visibility,
                                                    hiprtTraversalHintDefault,
                                                    &payload,
                                                    kernel_params.table_closest_intersect,
                                                    0 /* RAY_TYPE */,
                                                    ray->time);

    hit = traversal.getNextHit();
  }

  if (hit.hasHit()) {
    set_intersect_point(hit, isect);
    return true;
  }

  return false;
}

ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 const uint visibility)
{
  Intersection isect;
  return scene_intersect(kg, ray, visibility, &isect);
}

#ifdef __BVH_LOCAL__
template<bool single_hit = false>
ccl_device_intersect bool scene_intersect_local(KernelGlobals kg,
                                                const ccl_private Ray *ray,
                                                ccl_private LocalIntersection *local_isect,
                                                const int local_object,
                                                ccl_private uint *lcg_state,
                                                const int max_hits)
{
  if (local_isect != nullptr) {
    local_isect->num_hits = 0;
  }

  if (!intersection_ray_valid(ray)) {
    return false;
  }

  const int primitive_type = kernel_data_fetch(objects, local_object).primitive_type;
  if (!(primitive_type & PRIMITIVE_TRIANGLE)) {
    /* Local intersection functions are only considering triangle and motion triangle primitives.
     * If the local intersection is requested from other primitives (curve or point cloud) perform
     * an early return to avoid tree traversal with no primitive intersection. */
    return false;
  }

  float3 P = ray->P;
  float3 dir = bvh_clamp_direction(ray->D);
  float3 idir = bvh_inverse_direction(dir);

  const uint object_flag = kernel_data_fetch(object_flag, local_object);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
#  if BVH_FEATURE(BVH_MOTION)
    bvh_instance_motion_push(kg, local_object, ray, &P, &dir, &idir);
#  else
    bvh_instance_push(kg, local_object, ray, &P, &dir, &idir);
#  endif
  }

  hiprtRay ray_hip;
  ray_hip.origin = P;
  ray_hip.direction = dir;
  ray_hip.maxT = ray->tmax;
  ray_hip.minT = ray->tmin;

  LocalPayload payload = {0};
  payload.self = ray->self;
  payload.ray_time = ray->time;
  payload.local_object = local_object;
  payload.max_hits = max_hits;
  payload.lcg_state = lcg_state;
  payload.local_isect = local_isect;

  Stack stack(kg->global_stack_buffer, kg->shared_stack);
  Instance_Stack instance_stack;

  hiprtGeometry local_geom = (hiprtGeometry)(kernel_data_fetch(blas_ptr, local_object));

  hiprtHit hit;
  if (primitive_type == PRIMITIVE_MOTION_TRIANGLE) {
    /* Motion triangle BVH uses custom primitives which requires custom traversal. */
    hiprtGeomCustomTraversalAnyHitCustomStack traversal(local_geom,
                                                        ray_hip,
                                                        stack,
                                                        hiprtTraversalHintDefault,
                                                        &payload,
                                                        kernel_params.table_local_intersect,
                                                        2);
    hit = traversal.getNextHit();
  }
  else {
    hiprtGeomTraversalAnyHitCustomStack traversal(local_geom,
                                                  ray_hip,
                                                  stack,
                                                  hiprtTraversalHintDefault,
                                                  &payload,
                                                  kernel_params.table_local_intersect,
                                                  2);
    hit = traversal.getNextHit();
  }

  return hit.hasHit();
}
#endif /*__BVH_LOCAL__ */

#ifdef __TRANSPARENT_SHADOWS__
ccl_device_intersect void scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     const ccl_private Ray *ray,
                                                     const uint visibility,
                                                     const uint max_transparent_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  *throughput = 1.0f;
  *num_recorded_hits = 0;

  if (!intersection_ray_valid(ray)) {
    return;
  }

  hiprtRay ray_hip;
  set_hiprt_ray(*ray, ray_hip);

  ShadowPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;
  payload.ray_time = ray->time;
  payload.in_state = state;
  payload.max_transparent_hits = max_transparent_hits;
  payload.num_transparent_hits = 0;
  payload.r_num_recorded_hits = num_recorded_hits;
  payload.r_throughput = throughput;

  Stack stack(kg->global_stack_buffer, kg->shared_stack);
  Instance_Stack instance_stack;

  hiprtSceneTraversalAnyHitCustomStack traversal((hiprtScene)kernel_data.device_bvh,
                                                 ray_hip,
                                                 stack,
                                                 instance_stack,
                                                 visibility,
                                                 hiprtTraversalHintDefault,
                                                 &payload,
                                                 kernel_params.table_shadow_intersect,
                                                 1 /* RAY_TYPE */,
                                                 ray->time);

  const hiprtHit hit = traversal.getNextHit();
  (void)hit;
}
#endif /* __TRANSPARENT_SHADOWS__ */

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  isect->t = ray->tmax;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;
  isect->type = PRIMITIVE_NONE;

  if (!intersection_ray_valid(ray)) {
    return false;
  }

  hiprtRay ray_hip;
  set_hiprt_ray(*ray, ray_hip);

  RayPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;
  payload.ray_time = ray->time;

  Stack stack(kg->global_stack_buffer, kg->shared_stack);
  Instance_Stack instance_stack;

  hiprtSceneTraversalClosestCustomStack traversal((hiprtScene)kernel_data.device_bvh,
                                                  ray_hip,
                                                  stack,
                                                  instance_stack,
                                                  visibility,
                                                  hiprtTraversalHintDefault,
                                                  &payload,
                                                  kernel_params.table_volume_intersect,
                                                  3 /* RAY_TYPE */,
                                                  ray->time);

  const hiprtHit hit = traversal.getNextHit();
  if (hit.hasHit()) {
    set_intersect_point(hit, isect);
    return true;
  }

  return false;
}
#endif /* __VOLUME__ */

CCL_NAMESPACE_END
