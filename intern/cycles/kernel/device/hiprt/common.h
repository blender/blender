/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef __HIPRT__

struct RayPayload {
  KernelGlobals kg;
  RaySelfPrimitives self;
  uint visibility;
  int prim_type;
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
  KernelGlobals kg;
  RaySelfPrimitives self;
  float ray_time;
  int local_object;
  uint max_hits;
  uint *lcg_state;
  LocalIntersection *local_isect;
};

#  define SET_HIPRT_RAY(RAY_RT, RAY) \
    RAY_RT.direction = RAY->D; \
    RAY_RT.origin = RAY->P; \
    RAY_RT.maxT = RAY->tmax; \
    RAY_RT.minT = RAY->tmin;

#  define GET_TRAVERSAL_STACK() \
    Stack stack(kg->global_stack_buffer, kg->shared_stack); \
    Instance_Stack instance_stack;

#  define GET_TRAVERSAL_ANY_HIT(FUNCTION_TABLE, RAY_TYPE, RAY_TIME) \
    hiprtSceneTraversalAnyHitCustomStack<Stack, Instance_Stack> traversal( \
        (hiprtScene)kernel_data.device_bvh, \
        ray_hip, \
        stack, \
        instance_stack, \
        visibility, \
        hiprtTraversalHintDefault, \
        &payload, \
        kernel_params.FUNCTION_TABLE, \
        RAY_TYPE, \
        RAY_TIME);

#  define GET_TRAVERSAL_CLOSEST_HIT(FUNCTION_TABLE, RAY_TYPE, RAY_TIME) \
    hiprtSceneTraversalClosestCustomStack<Stack, Instance_Stack> traversal( \
        (hiprtScene)kernel_data.device_bvh, \
        ray_hip, \
        stack, \
        instance_stack, \
        visibility, \
        hiprtTraversalHintDefault, \
        &payload, \
        kernel_params.FUNCTION_TABLE, \
        RAY_TYPE, \
        RAY_TIME);

ccl_device_inline void set_intersect_point(KernelGlobals kg,
                                           const hiprtHit &hit,
                                           ccl_private Intersection *isect)
{
  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  isect->type = kernel_data_fetch(objects, object_id).primitive_type;

  isect->t = hit.t;
  isect->prim = hit.primID + prim_offset;
  isect->object = object_id;
  isect->u = hit.uv.x;
  isect->v = hit.uv.y;
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

  KernelGlobals kg = payload->kg;

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

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, payload->self, object_id)) {
    return false; /* Ignore hit - continue traversal. */
  }
#  endif

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
    hit.primID = isect.prim;
    payload->prim_type = isect.type; /* packed_curve_type */
  }

  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_intersect(const hiprtRay &ray,
                                                        RayPayload *payload,
                                                        hiprtHit &hit)
{
  KernelGlobals kg = payload->kg;
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
    hit.primID = isect.prim;
    payload->prim_type = isect.type;
  }

  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_local_intersect(const hiprtRay &ray,
                                                              LocalPayload *payload,
                                                              hiprtHit &hit)
{
#  ifdef __OBJECT_MOTION__
  KernelGlobals kg = payload->kg;

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

#  else
  return false;
#  endif
}

ccl_device_inline bool motion_triangle_custom_volume_intersect(const hiprtRay &ray,
                                                               RayPayload *payload,
                                                               hiprtHit &hit)
{
#  ifdef __OBJECT_MOTION__
  KernelGlobals kg = payload->kg;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int object_flag = kernel_data_fetch(object_flag, object_id);

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
    hit.primID = isect.prim;
    payload->prim_type = isect.type;
  }

  return b_hit;
#  else
  return false;
#  endif
}

ccl_device_inline bool point_custom_intersect(const hiprtRay &ray,
                                              RayPayload *payload,
                                              hiprtHit &hit)
{
#  if defined(__POINTCLOUD__)
  KernelGlobals kg = payload->kg;

  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  const int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
  const int prim_id_local = prim_info.x;
  const int prim_id_global = prim_id_local + prim_offset;

  const int primitive_type = prim_info.y;

#    ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, payload->self, object_id)) {
    return false; /* Ignore hit - continue traversal */
  }
#    endif

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
    hit.primID = isect.prim;
    payload->prim_type = isect.type;
  }

  return b_hit;
#  else
  return false;
#  endif
}

/* --------------------------------------------------------------------
 * Intersection filters.
 */

ccl_device_inline bool closest_intersection_filter(const hiprtRay &ray,
                                                   RayPayload *payload,
                                                   const hiprtHit &hit)
{
  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int prim = hit.primID + prim_offset;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(payload->kg, payload->self, object_id)) {
    return true; /* Ignore hit - continue traversal. */
  }
#  endif

  if (intersection_skip_self_shadow(payload->self, object_id, prim)) {
    return true; /* Ignore hit - continue traversal. */
  }

  return false;
}

ccl_device_inline bool shadow_intersection_filter(const hiprtRay &ray,
                                                  ShadowPayload *payload,
                                                  const hiprtHit &hit)

{
  KernelGlobals kg = payload->kg;

  uint num_transparent_hits = payload->num_transparent_hits;
  const uint max_transparent_hits = payload->max_transparent_hits;
  const int state = payload->in_state;
  const RaySelfPrimitives &self = payload->self;

  const int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);
  const int prim = hit.primID + prim_offset;

  const float ray_tmax = hit.t;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, self, object)) {
    return true; /* Ignore hit - continue traversal */
  }
#  endif

#  ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true; /* No hit - continue traversal. */
  }
#  endif

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

#  ifndef __TRANSPARENT_SHADOWS__
  return false;
#  else
  const int flags = intersection_get_shader_flags(kg, prim, primitive_type);
  if (!(flags & SD_HAS_TRANSPARENT_SHADOW)) {
    return false;
  }

  num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (num_transparent_hits > max_transparent_hits) {
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
#  endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool shadow_intersection_filter_curves(const hiprtRay &ray,
                                                         ShadowPayload *payload,
                                                         const hiprtHit &hit)

{
  KernelGlobals kg = payload->kg;

  uint num_transparent_hits = payload->num_transparent_hits;
  const uint num_recorded_hits = *(payload->r_num_recorded_hits);
  const uint max_transparent_hits = payload->max_transparent_hits;
  const RaySelfPrimitives &self = payload->self;

  const int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim = hit.primID;

  const float ray_tmax = hit.t;

#  ifdef __SHADOW_LINKING__
  /* It doesn't seem like this is necessary. */
  if (intersection_skip_shadow_link(kg, self, object)) {
    return true; /* Ignore hit - continue traversal. */
  }
#  endif

#  ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true; /* No hit - continue traversal. */
  }
#  endif

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

  const int primitive_type = payload->prim_type;

#  ifndef __TRANSPARENT_SHADOWS__
  return false;
#  else
  const int flags = intersection_get_shader_flags(kg, prim, primitive_type);
  if (!(flags & SD_HAS_TRANSPARENT_SHADOW)) {
    return false;
  }

  num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (num_transparent_hits > max_transparent_hits) {
    return false;
  }

  float throughput = *payload->r_throughput;
  throughput *= intersection_curve_shadow_transparency(kg, object, prim, primitive_type, u);
  *payload->r_throughput = throughput;
  payload->num_transparent_hits = num_transparent_hits;

  if (throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
    return false;
  }

  return true;
#  endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool local_intersection_filter(const hiprtRay &ray,
                                                 LocalPayload *payload,
                                                 const hiprtHit &hit)
{
#  ifdef __BVH_LOCAL__
  KernelGlobals kg = payload->kg;

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
#    ifndef __RAY_OFFSET__
  if (intersection_skip_self_local(payload->self, prim)) {
    return true; /* Continue search. */
  }
#    endif

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
#  else
  return false;
#  endif
}

ccl_device_inline bool volume_intersection_filter(const hiprtRay &ray,
                                                  RayPayload *payload,
                                                  const hiprtHit &hit)
{
  const int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  const int prim = hit.primID + prim_offset;
  const int object_flag = kernel_data_fetch(object_flag, object_id);

  if (intersection_skip_self(payload->self, object_id, prim)) {
    return true;
  }
  if ((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
    return true;
  }
  return false;
}

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

#endif
