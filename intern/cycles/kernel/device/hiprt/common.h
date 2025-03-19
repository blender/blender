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

struct ShadowPayload {
  KernelGlobals kg;
  RaySelfPrimitives self;
  uint visibility;
  int prim_type;
  float ray_time;
  int in_state;
  uint max_hits;
  uint num_hits;
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
                                           hiprtHit &hit,
                                           ccl_private Intersection *isect)
{
  int prim_offset = 0;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  isect->type = kernel_data_fetch(objects, object_id).primitive_type;

  isect->t = hit.t;
  isect->prim = hit.primID + prim_offset;
  isect->object = object_id;
  isect->u = hit.uv.x;
  isect->v = hit.uv.y;
}

// custom intersection functions

ccl_device_inline bool curve_custom_intersect(const hiprtRay &ray,
                                              const void *userPtr,
                                              void *payload,
                                              hiprtHit &hit)

{
  Intersection isect;
  RayPayload *local_payload = (RayPayload *)payload;
  // could also cast shadow payload to get the elements needed to do the intersection
  // no need to write a separate function for shadow intersection

  KernelGlobals kg = local_payload->kg;

  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  // data_offset.x: where the data (prim id, type )for the geometry of the current object begins
  // the prim_id that is in hiprtHit hit is local to the partciular geometry so we add the above
  // ofstream
  // to map prim id in hiprtHit to the one compatible to what next stage expects

  // data_offset.y: the offset that has to be added to a local primitive to get the global
  // primitive id = kernel_data_fetch(object_prim_offset, object_id);

  int prim_offset = data_offset.y;

  int curve_index = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  int key_value = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).y;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(nullptr, local_payload->self, object_id)) {
    /* Ignore hit - continue traversal */
    return false;
  }
#  endif

  if (intersection_skip_self_shadow(local_payload->self, object_id, curve_index + prim_offset))
    return false;

  float ray_time = local_payload->ray_time;

  if ((key_value & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {

    int time_offset = kernel_data_fetch(prim_time_offset, object_id);
    float2 prims_time = kernel_data_fetch(prims_time, hit.primID + time_offset);

    if (ray_time < prims_time.x || ray_time > prims_time.y) {
      return false;
    }
  }

  bool b_hit = curve_intersect(kg,
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
    local_payload->prim_type = isect.type;  // packed_curve_type;
  }
  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_intersect(const hiprtRay &ray,
                                                        const void *userPtr,
                                                        void *payload,
                                                        hiprtHit &hit)
{
  RayPayload *local_payload = (RayPayload *)payload;
  KernelGlobals kg = local_payload->kg;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_shadow(local_payload->self, object_id, prim_id_global))
    return false;

  Intersection isect;

  bool b_hit = motion_triangle_intersect(kg,
                                         &isect,
                                         ray.origin,
                                         ray.direction,
                                         ray.minT,
                                         ray.maxT,
                                         local_payload->ray_time,
                                         local_payload->visibility,
                                         object_id,
                                         prim_id_global,
                                         hit.instanceID);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
    hit.primID = isect.prim;
    local_payload->prim_type = isect.type;
  }
  return b_hit;
}

ccl_device_inline bool motion_triangle_custom_local_intersect(const hiprtRay &ray,
                                                              const void *userPtr,
                                                              void *payload,
                                                              hiprtHit &hit)
{
#  ifdef __OBJECT_MOTION__
  LocalPayload *local_payload = (LocalPayload *)payload;
  KernelGlobals kg = local_payload->kg;
  int object_id = local_payload->local_object;

  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);

  int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_local(local_payload->self, prim_id_global))
    return false;

  LocalIntersection *local_isect = local_payload->local_isect;

  return motion_triangle_intersect_local(kg,
                                         local_isect,
                                         ray.origin,
                                         ray.direction,
                                         local_payload->ray_time,
                                         object_id,
                                         prim_id_global,
                                         prim_id_local,
                                         ray.minT,
                                         ray.maxT,
                                         local_payload->lcg_state,
                                         local_payload->max_hits);

#  else
  return false;
#  endif
}

ccl_device_inline bool motion_triangle_custom_volume_intersect(const hiprtRay &ray,
                                                               const void *userPtr,
                                                               void *payload,
                                                               hiprtHit &hit)
{
#  ifdef __OBJECT_MOTION__
  RayPayload *local_payload = (RayPayload *)payload;
  KernelGlobals kg = local_payload->kg;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  int object_flag = kernel_data_fetch(object_flag, object_id);

  if (!(object_flag & SD_OBJECT_HAS_VOLUME))
    return false;

  int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  int prim_id_local = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x).x;
  int prim_id_global = prim_id_local + prim_offset;

  if (intersection_skip_self_shadow(local_payload->self, object_id, prim_id_global))
    return false;

  Intersection isect;

  bool b_hit = motion_triangle_intersect(kg,
                                         &isect,
                                         ray.origin,
                                         ray.direction,
                                         ray.minT,
                                         ray.maxT,
                                         local_payload->ray_time,
                                         local_payload->visibility,
                                         object_id,
                                         prim_id_global,
                                         prim_id_local);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
    hit.primID = isect.prim;
    local_payload->prim_type = isect.type;
  }
  return b_hit;
#  else
  return false;
#  endif
}

ccl_device_inline bool point_custom_intersect(const hiprtRay &ray,
                                              const void *userPtr,
                                              void *payload,
                                              hiprtHit &hit)
{
#  if defined(__POINTCLOUD__)
  RayPayload *local_payload = (RayPayload *)payload;
  KernelGlobals kg = local_payload->kg;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);

  int2 data_offset = kernel_data_fetch(custom_prim_info_offset, object_id);
  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);

  int2 prim_info = kernel_data_fetch(custom_prim_info, hit.primID + data_offset.x);
  int prim_id_local = prim_info.x;
  int prim_id_global = prim_id_local + prim_offset;

  int type = prim_info.y;

#    ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(nullptr, local_payload->self, object_id)) {
    /* Ignore hit - continue traversal */
    return false;
  }
#    endif

  if (intersection_skip_self_shadow(local_payload->self, object_id, prim_id_global))
    return false;

  float ray_time = local_payload->ray_time;

  if ((type & PRIMITIVE_MOTION_POINT) && kernel_data.bvh.use_bvh_steps) {

    int time_offset = kernel_data_fetch(prim_time_offset, object_id);
    float2 prims_time = kernel_data_fetch(prims_time, hit.primID + time_offset);

    if (ray_time < prims_time.x || ray_time > prims_time.y) {
      return false;
    }
  }

  Intersection isect;

  bool b_hit = point_intersect(kg,
                               &isect,
                               ray.origin,
                               ray.direction,
                               ray.minT,
                               ray.maxT,
                               object_id,
                               prim_id_global,
                               ray_time,
                               type);

  if (b_hit) {
    hit.uv.x = isect.u;
    hit.uv.y = isect.v;
    hit.t = isect.t;
    hit.primID = isect.prim;
    local_payload->prim_type = isect.type;
  }
  return b_hit;
#  else
  return false;
#  endif
}

// intersection filters

ccl_device_inline bool closest_intersection_filter(const hiprtRay &ray,
                                                   const void *data,
                                                   void *user_data,
                                                   const hiprtHit &hit)
{
  RayPayload *payload = (RayPayload *)user_data;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  int prim = hit.primID + prim_offset;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(nullptr, payload->self, object_id)) {
    /* Ignore hit - continue traversal */
    return true;
  }
#  endif

  if (intersection_skip_self_shadow(payload->self, object_id, prim)) {
    /* Ignore hit - continue traversal */
    return true;
  }

  return false;
}

ccl_device_inline bool shadow_intersection_filter(const hiprtRay &ray,
                                                  const void *data,
                                                  void *user_data,
                                                  const hiprtHit &hit)

{
  ShadowPayload *payload = (ShadowPayload *)user_data;

  uint num_hits = payload->num_hits;
  uint num_recorded_hits = *(payload->r_num_recorded_hits);
  uint max_hits = payload->max_hits;
  int state = payload->in_state;
  KernelGlobals kg = payload->kg;
  RaySelfPrimitives self = payload->self;

  int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  int prim_offset = kernel_data_fetch(object_prim_offset, object);
  int prim = hit.primID + prim_offset;

  float ray_tmax = hit.t;

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(nullptr, self, object)) {
    /* Ignore hit - continue traversal */
    return true;
  }
#  endif

#  ifdef __VISIBILITY_FLAG__

  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true;  // no hit - continue traversal
  }
#  endif

  if (intersection_skip_self_shadow(self, object, prim)) {
    return true;  // no hit -continue traversal
  }

  float u = hit.uv.x;
  float v = hit.uv.y;
  int type = kernel_data_fetch(objects, object).primitive_type;

#  ifndef __TRANSPARENT_SHADOWS__

  return false;

#  else

  if (num_hits >= max_hits ||
      !(intersection_get_shader_flags(nullptr, prim, type) & SD_HAS_TRANSPARENT_SHADOW))
  {
    return false;
  }

  uint record_index = num_recorded_hits;

  num_hits += 1;
  num_recorded_hits += 1;
  payload->num_hits = num_hits;
  *(payload->r_num_recorded_hits) = num_recorded_hits;

  const uint max_record_hits = min(max_hits, INTEGRATOR_SHADOW_ISECT_SIZE);
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
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, type) = type;
  return true;

#  endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool shadow_intersection_filter_curves(const hiprtRay &ray,
                                                         const void *data,
                                                         void *user_data,
                                                         const hiprtHit &hit)

{
  ShadowPayload *payload = (ShadowPayload *)user_data;

  uint num_hits = payload->num_hits;
  uint num_recorded_hits = *(payload->r_num_recorded_hits);
  uint max_hits = payload->max_hits;
  KernelGlobals kg = payload->kg;
  RaySelfPrimitives self = payload->self;

  int object = kernel_data_fetch(user_instance_id, hit.instanceID);
  int prim = hit.primID;

  float ray_tmax = hit.t;

#  ifdef __SHADOW_LINKING__
  /* It doesn't seem like this is necessary. */
  if (intersection_skip_shadow_link(nullptr, self, object)) {
    /* Ignore hit - continue traversal */
    return true;
  }
#  endif

#  ifdef __VISIBILITY_FLAG__

  if ((kernel_data_fetch(objects, object).visibility & payload->visibility) == 0) {
    return true;  // no hit - continue traversal
  }
#  endif

  if (intersection_skip_self_shadow(self, object, prim)) {
    return true;  // no hit -continue traversal
  }

  float u = hit.uv.x;
  float v = hit.uv.y;

  if (u == 0.0f || u == 1.0f) {
    // continue traversal
    return true;
  }

  int type = payload->prim_type;

#  ifndef __TRANSPARENT_SHADOWS__

  return false;

#  else

  if (num_hits >= max_hits ||
      !(intersection_get_shader_flags(nullptr, prim, type) & SD_HAS_TRANSPARENT_SHADOW))
  {
    return false;
  }

  float throughput = *payload->r_throughput;
  throughput *= intersection_curve_shadow_transparency(kg, object, prim, type, u);
  *payload->r_throughput = throughput;
  payload->num_hits += 1;

  if (throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
    return false;
  }

  return true;

#  endif /* __TRANSPARENT_SHADOWS__ */
}

ccl_device_inline bool local_intersection_filter(const hiprtRay &ray,
                                                 const void *data,
                                                 void *user_data,
                                                 const hiprtHit &hit)
{
#  ifdef __BVH_LOCAL__
  LocalPayload *payload = (LocalPayload *)user_data;
  KernelGlobals kg = payload->kg;
  const int object_id = payload->local_object;
  const uint max_hits = payload->max_hits;

  /* Triangle primitive uses hardware intersection, other primitives  do custom intersection
   * which does reservoir samlping for intersections. For the custom primitives only check
   * whether we can stop travsersal early on. The rest of the checks here only do for the
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
    return true;  // continue search
  }
#    endif

  if (max_hits == 0) {
    return false;  // stop search
  }

  int hit_index = 0;
  if (payload->lcg_state) {
    for (int i = min(max_hits, payload->local_isect->num_hits) - 1; i >= 0; --i) {
      if (hit.t == payload->local_isect->hits[i].t) {
        return true;  // continue search
      }
    }
    hit_index = payload->local_isect->num_hits++;
    if (payload->local_isect->num_hits > max_hits) {
      hit_index = lcg_step_uint(payload->lcg_state) % payload->local_isect->num_hits;
      if (hit_index >= max_hits) {
        return true;  // continue search
      }
    }
  }
  else {
    if (payload->local_isect->num_hits && hit.t > payload->local_isect->hits[0].t) {
      return true;
    }
    payload->local_isect->num_hits = 1;
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
                                                  const void *data,
                                                  void *user_data,
                                                  const hiprtHit &hit)
{
  RayPayload *payload = (RayPayload *)user_data;
  int object_id = kernel_data_fetch(user_instance_id, hit.instanceID);
  int prim_offset = kernel_data_fetch(object_prim_offset, object_id);
  int prim = hit.primID + prim_offset;
  int object_flag = kernel_data_fetch(object_flag, object_id);

  if (intersection_skip_self(payload->self, object_id, prim))
    return true;
  else if ((object_flag & SD_OBJECT_HAS_VOLUME) == 0)
    return true;
  else
    return false;
}

HIPRT_DEVICE bool intersectFunc(const uint geomType,
                                const uint rayType,
                                const hiprtFuncTableHeader &tableHeader,
                                const hiprtRay &ray,
                                void *payload,
                                hiprtHit &hit)
{
  const uint index = tableHeader.numGeomTypes * rayType + geomType;
  const void *data = tableHeader.funcDataSets[index].filterFuncData;
  switch (index) {
    case Curve_Intersect_Function:
    case Curve_Intersect_Shadow:
      return curve_custom_intersect(ray, data, payload, hit);
    case Motion_Triangle_Intersect_Function:
    case Motion_Triangle_Intersect_Shadow:
      return motion_triangle_custom_intersect(ray, data, payload, hit);
    case Motion_Triangle_Intersect_Local:
      return motion_triangle_custom_local_intersect(ray, data, payload, hit);
    case Motion_Triangle_Intersect_Volume:
      return motion_triangle_custom_volume_intersect(ray, data, payload, hit);
    case Point_Intersect_Function:
    case Point_Intersect_Shadow:
      return point_custom_intersect(ray, data, payload, hit);
    default:
      break;
  }
  return false;
}

HIPRT_DEVICE bool filterFunc(const uint geomType,
                             const uint rayType,
                             const hiprtFuncTableHeader &tableHeader,
                             const hiprtRay &ray,
                             void *payload,
                             const hiprtHit &hit)
{
  const uint index = tableHeader.numGeomTypes * rayType + geomType;
  const void *data = tableHeader.funcDataSets[index].intersectFuncData;
  switch (index) {
    case Triangle_Filter_Closest:
      return closest_intersection_filter(ray, data, payload, hit);
    case Curve_Filter_Shadow:
      return shadow_intersection_filter_curves(ray, data, payload, hit);
    case Triangle_Filter_Shadow:
    case Motion_Triangle_Filter_Shadow:
    case Point_Filter_Shadow:
      return shadow_intersection_filter(ray, data, payload, hit);
    case Triangle_Filter_Local:
    case Motion_Triangle_Filter_Local:
      return local_intersection_filter(ray, data, payload, hit);
    case Triangle_Filter_Volume:
    case Motion_Triangle_Filter_Volume:
      return volume_intersection_filter(ray, data, payload, hit);
    default:
      break;
  }

  return false;
}

#endif
