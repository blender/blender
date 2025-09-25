/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* OptiX implementation of ray-scene intersection. */

#pragma once

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"

#define OPTIX_DEFINE_ABI_VERSION_ONLY
#include <optix_function_table.h>

CCL_NAMESPACE_BEGIN

/* Utilities. */

template<typename T> ccl_device_forceinline T *get_payload_ptr_0()
{
  return pointer_unpack_from_uint<T>(optixGetPayload_0(), optixGetPayload_1());
}
template<typename T> ccl_device_forceinline T *get_payload_ptr_2()
{
  return pointer_unpack_from_uint<T>(optixGetPayload_2(), optixGetPayload_3());
}

template<typename T> ccl_device_forceinline T *get_payload_ptr_6()
{
  return (T *)(((uint64_t)optixGetPayload_7() << 32) | optixGetPayload_6());
}

ccl_device_forceinline int get_object_id()
{
#ifdef __OBJECT_MOTION__
  /* Always get the instance ID from the TLAS
   * There might be a motion transform node between TLAS and BLAS which does not have one. */
  return optixGetInstanceIdFromHandle(optixGetTransformListHandle(0));
#else
  return optixGetInstanceId();
#endif
}

/* Hit/miss functions. */

extern "C" __global__ void __miss__kernel_optix_miss()
{
  /* 'kernel_path_lamp_emission' checks intersection distance, so need to set it even on a miss. */
  optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
  optixSetPayload_5(PRIMITIVE_NONE);
}

extern "C" __global__ void __anyhit__kernel_optix_ignore()
{
  return optixIgnoreIntersection();
}

extern "C" __global__ void __closesthit__kernel_optix_ignore() {}

extern "C" __global__ void __anyhit__kernel_optix_local_hit()
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  if (!optixIsTriangleHit()) {
    /* Ignore curves and points. */
    return optixIgnoreIntersection();
  }
#endif

#ifdef __BVH_LOCAL__
  const int object = get_object_id();
  if (object != optixGetPayload_4() /* local_object */) {
    /* Only intersect with matching object. */
    return optixIgnoreIntersection();
  }

  const int prim = optixGetPrimitiveIndex();
  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();
  if (intersection_skip_self_local(ray->self, prim)) {
    return optixIgnoreIntersection();
  }

  const uint max_hits = optixGetPayload_5();
  if (max_hits == 0) {
    /* Special case for when no hit information is requested, just report that something was hit */
    optixSetPayload_5(true);
    return optixTerminateRay();
  }

  int hit = 0;
  uint *const lcg_state = get_payload_ptr_0<uint>();
  LocalIntersection *const local_isect = get_payload_ptr_2<LocalIntersection>();

  if (lcg_state) {
    for (int i = min(max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
      if (optixGetRayTmax() == local_isect->hits[i].t) {
        return optixIgnoreIntersection();
      }
    }

    hit = local_isect->num_hits++;

    if (local_isect->num_hits > max_hits) {
      hit = lcg_step_uint(lcg_state) % local_isect->num_hits;
      if (hit >= max_hits) {
        return optixIgnoreIntersection();
      }
    }
  }
  else {
    if (local_isect->num_hits && optixGetRayTmax() > local_isect->hits[0].t) {
      /* Record closest intersection only.
       * Do not terminate ray here, since there is no guarantee about distance ordering in any-hit.
       */
      return optixIgnoreIntersection();
    }

    local_isect->num_hits = 1;
  }

  Intersection *isect = &local_isect->hits[hit];
  isect->t = optixGetRayTmax();
  isect->prim = prim;
  isect->object = get_object_id();
  isect->type = kernel_data_fetch(objects, isect->object).primitive_type;

  const float2 barycentrics = optixGetTriangleBarycentrics();
  isect->u = barycentrics.x;
  isect->v = barycentrics.y;

  /* Record geometric normal. */
  const packed_uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  const float3 tri_a = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 tri_b = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 tri_c = kernel_data_fetch(tri_verts, tri_vindex.z);

  local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  /* Continue tracing (without this the trace call would return after the first hit). */
  optixIgnoreIntersection();
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_shadow_all_hit()
{
#ifdef __SHADOW_RECORD_ALL__
  int prim = optixGetPrimitiveIndex();
  const uint object = get_object_id();
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#  endif

  float u = 0.0f, v = 0.0f;
  int type = 0;
  if (optixIsTriangleHit()) {
    /* Triangle. */
    const float2 barycentrics = optixGetTriangleBarycentrics();
    u = barycentrics.x;
    v = barycentrics.y;
    type = kernel_data_fetch(objects, object).primitive_type;
  }
#  ifdef __HAIR__
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    /* Curve. */
    u = __uint_as_float(optixGetAttribute_0());
    v = __uint_as_float(optixGetAttribute_1());

    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    type = segment.type;
    prim = segment.prim;
  }
#  endif
  else {
    /* Point. */
    type = kernel_data_fetch(objects, object).primitive_type;
    u = 0.0f;
    v = 0.0f;
  }

  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();
  if (intersection_skip_self_shadow(ray->self, object, prim)) {
    return optixIgnoreIntersection();
  }

#  ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(nullptr, ray->self, object)) {
    return optixIgnoreIntersection();
  }
#  endif

#  ifndef __TRANSPARENT_SHADOWS__
  /* No transparent shadows support compiled in, make opaque. */
  optixSetPayload_5(true);
  return optixTerminateRay();
#  else
  const uint max_transparent_hits = optixGetPayload_3();
  const uint num_hits_packed = optixGetPayload_2();
  const uint num_recorded_hits = uint16_unpack_from_uint_0(num_hits_packed);
  uint num_transparent_hits = uint16_unpack_from_uint_1(num_hits_packed);

  /* If no transparent shadows, all light is blocked and we can stop immediately. */
  const int flags = intersection_get_shader_flags(nullptr, prim, type);
  if (!(flags & SD_HAS_TRANSPARENT_SHADOW)) {
    optixSetPayload_5(true);
    return optixTerminateRay();
  }

  /* Only count transparent bounces, volume bounds bounces are counted during shading. */
  num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (num_transparent_hits > max_transparent_hits) {
    /* Max number of hits exceeded. */
    optixSetPayload_5(true);
    return optixTerminateRay();
  }

  /* Always use baked shadow transparency for curves. */
  if (type & PRIMITIVE_CURVE) {
    float throughput = __uint_as_float(optixGetPayload_1());
    throughput *= intersection_curve_shadow_transparency(nullptr, object, prim, type, u);
    optixSetPayload_1(__float_as_uint(throughput));
    optixSetPayload_2(uint16_pack_to_uint(num_recorded_hits, num_transparent_hits));

    if (throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
      optixSetPayload_5(true);
      return optixTerminateRay();
    }
    else {
      /* Continue tracing. */
      optixIgnoreIntersection();
      return;
    }
  }

  /* Record transparent intersection. */
  optixSetPayload_2(uint16_pack_to_uint(num_recorded_hits + 1, num_transparent_hits));

  uint record_index = num_recorded_hits;

  const IntegratorShadowState state = optixGetPayload_0();

  const uint max_record_hits = INTEGRATOR_SHADOW_ISECT_SIZE;
  if (record_index >= max_record_hits) {
    /* If maximum number of hits reached, find a hit to replace. */
    float max_recorded_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, t);
    uint max_recorded_hit = 0;

    for (int i = 1; i < max_record_hits; i++) {
      const float isect_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, i, t);
      if (isect_t > max_recorded_t) {
        max_recorded_t = isect_t;
        max_recorded_hit = i;
      }
    }

    if (optixGetRayTmax() >= max_recorded_t) {
      /* Accept hit, so that OptiX won't consider any more hits beyond the distance of the
       * current hit anymore. */
      return;
    }

    record_index = max_recorded_hit;
  }

  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, u) = u;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, v) = v;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, t) = optixGetRayTmax();
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, prim) = prim;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, object) = object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, type) = type;

  /* Continue tracing. */
  optixIgnoreIntersection();
#  endif /* __TRANSPARENT_SHADOWS__ */
#endif   /* __SHADOW_RECORD_ALL__ */
}

extern "C" __global__ void __anyhit__kernel_optix_volume_test()
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  if (!optixIsTriangleHit()) {
    /* Ignore curves. */
    return optixIgnoreIntersection();
  }
#endif

  const uint object = get_object_id();
#ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#endif

  if ((kernel_data_fetch(object_flag, object) & SD_OBJECT_HAS_VOLUME) == 0) {
    return optixIgnoreIntersection();
  }

  const int prim = optixGetPrimitiveIndex();
  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();
  if (intersection_skip_self(ray->self, object, prim)) {
    return optixIgnoreIntersection();
  }
}

extern "C" __global__ void __anyhit__kernel_optix_visibility_test()
{
  const uint object = get_object_id();
  const uint visibility = optixGetPayload_4();
#ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#endif

  int prim = optixGetPrimitiveIndex();
  if (optixIsTriangleHit()) {
    /* Triangle. */
  }
#ifdef __HAIR__
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    /* Curve. */
    prim = kernel_data_fetch(curve_segments, prim).prim;
  }
#endif

  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();

  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
#ifdef __SHADOW_LINKING__
    if (intersection_skip_shadow_link(nullptr, ray->self, object)) {
      return optixIgnoreIntersection();
    }
#endif

    if (intersection_skip_self_shadow(ray->self, object, prim)) {
      return optixIgnoreIntersection();
    }
    else {
      /* Shadow ray early termination. */
      return optixTerminateRay();
    }
  }
  else {
    if (intersection_skip_self(ray->self, object, prim)) {
      return optixIgnoreIntersection();
    }
  }
}

extern "C" __global__ void __closesthit__kernel_optix_hit()
{
  const int object = get_object_id();
  const int prim = optixGetPrimitiveIndex();

  optixSetPayload_0(__float_as_uint(optixGetRayTmax())); /* Intersection distance */
  optixSetPayload_4(object);

  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    optixSetPayload_1(__float_as_uint(barycentrics.x));
    optixSetPayload_2(__float_as_uint(barycentrics.y));
    optixSetPayload_3(prim);
    optixSetPayload_5(kernel_data_fetch(objects, object).primitive_type);
  }
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    optixSetPayload_1(optixGetAttribute_0()); /* Same as 'optixGetCurveParameter()' */
    optixSetPayload_2(optixGetAttribute_1());
    optixSetPayload_3(segment.prim);
    optixSetPayload_5(segment.type);
  }
  else {
    optixSetPayload_1(0);
    optixSetPayload_2(0);
    optixSetPayload_3(prim);
    optixSetPayload_5(kernel_data_fetch(objects, object).primitive_type);
  }
}

/* Custom primitive intersection functions. */

#ifdef __HAIR__
ccl_device_inline void optix_intersection_curve(const int prim, const int type)
{
  const int object = get_object_id();

#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  const float3 ray_P = optixGetObjectRayOrigin();
  const float3 ray_D = optixGetObjectRayDirection();
  const float ray_tmin = optixGetRayTmin();

#  ifdef __OBJECT_MOTION__
  const float time = optixGetRayTime();
#  else
  const float time = 0.0f;
#  endif

  Intersection isect;
  isect.t = optixGetRayTmax();

  if (curve_intersect(nullptr, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type))
  {
    static_assert(PRIMITIVE_ALL < 128, "Values >= 128 are reserved for OptiX internal use");
    optixReportIntersection(isect.t,
                            type & PRIMITIVE_ALL,
                            __float_as_int(isect.u),  /* Attribute_0 */
                            __float_as_int(isect.v)); /* Attribute_1 */
  }
}

extern "C" __global__ void __intersection__curve_ribbon()
{
  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, optixGetPrimitiveIndex());
  const int prim = segment.prim;
  const int type = segment.type;
  if ((type & PRIMITIVE_CURVE) == PRIMITIVE_CURVE_RIBBON) {
    optix_intersection_curve(prim, type);
  }
}

#endif

#ifdef __POINTCLOUD__
extern "C" __global__ void __intersection__point()
{
  const int prim = optixGetPrimitiveIndex();
  const int object = get_object_id();
  const int type = kernel_data_fetch(objects, object).primitive_type;

#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  const float3 ray_P = optixGetObjectRayOrigin();
  const float3 ray_D = optixGetObjectRayDirection();
  const float ray_tmin = optixGetRayTmin();

#  ifdef __OBJECT_MOTION__
  const float time = optixGetRayTime();
#  else
  const float time = 0.0f;
#  endif

  Intersection isect;
  isect.t = optixGetRayTmax();

  if (point_intersect(nullptr, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type))
  {
    static_assert(PRIMITIVE_ALL < 128, "Values >= 128 are reserved for OptiX internal use");
    optixReportIntersection(isect.t, type & PRIMITIVE_ALL);
  }
}
#endif

/* Scene intersection. */

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          const ccl_private Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  uint p0 = 0;
  uint p1 = 0;
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = visibility;
  uint p5 = PRIMITIVE_NONE;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  uint ray_mask = visibility & 0xFF;
  uint ray_flags = OPTIX_RAY_FLAG_ENFORCE_ANYHIT;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }
  else if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    ray_flags |= OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT;
  }

  optixTrace(intersection_ray_valid(ray) ? kernel_data.device_bvh : 0,
             ray->P,
             ray->D,
             ray->tmin,
             ray->tmax,
             ray->time,
             ray_mask,
             ray_flags,
             0, /* SBT offset for PG_HITD */
             0,
             0,
             p0,
             p1,
             p2,
             p3,
             p4,
             p5,
             p6,
             p7);

  isect->t = __uint_as_float(p0);
  isect->u = __uint_as_float(p1);
  isect->v = __uint_as_float(p2);
  isect->prim = p3;
  isect->object = p4;
  isect->type = p5;

  return p5 != PRIMITIVE_NONE;
}

ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 const uint visibility)
{
  uint p0 = 0;
  uint p1 = 0;
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = visibility;
  uint p5 = PRIMITIVE_NONE;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  uint ray_mask = visibility & 0xFF;
  uint ray_flags = OPTIX_RAY_FLAG_ENFORCE_ANYHIT;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }
  else if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    ray_flags |= OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT;
  }

  optixTraverse(intersection_ray_valid(ray) ? kernel_data.device_bvh : 0,
                ray->P,
                ray->D,
                ray->tmin,
                ray->tmax,
                ray->time,
                ray_mask,
                ray_flags,
                0, /* SBT offset for PG_HITD */
                0,
                0,
                p0,
                p1,
                p2,
                p3,
                p4,
                p5,
                p6,
                p7);

  return optixHitObjectIsHit();
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
  uint p0 = pointer_pack_to_uint_0(lcg_state);
  uint p1 = pointer_pack_to_uint_1(lcg_state);
  uint p2 = pointer_pack_to_uint_0(local_isect);
  uint p3 = pointer_pack_to_uint_1(local_isect);
  uint p4 = local_object;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  /* Is set to zero on miss or if ray is aborted, so can be used as return value. */
  uint p5 = max_hits;

  if (local_isect) {
    local_isect->num_hits = 0; /* Initialize hit count to zero. */
  }
  optixTraverse(intersection_ray_valid(ray) ? kernel_data.device_bvh : 0,
                ray->P,
                ray->D,
                ray->tmin,
                ray->tmax,
                ray->time,
                0xFF,
                /* Need to always call into __anyhit__kernel_optix_local_hit. */
                OPTIX_RAY_FLAG_ENFORCE_ANYHIT,
                2, /* SBT offset for PG_HITL */
                0,
                0,
                p0,
                p1,
                p2,
                p3,
                p4,
                p5,
                p6,
                p7);

  return p5;
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     const ccl_private Ray *ray,
                                                     const uint visibility,
                                                     const uint max_transparent_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  uint p0 = state;
  uint p1 = __float_as_uint(1.0f); /* Throughput. */
  uint p2 = 0;                     /* Number of hits. */
  uint p3 = max_transparent_hits;
  uint p4 = visibility;
  uint p5 = false;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

  optixTraverse(intersection_ray_valid(ray) ? kernel_data.device_bvh : 0,
                ray->P,
                ray->D,
                ray->tmin,
                ray->tmax,
                ray->time,
                ray_mask,
                /* Need to always call into __anyhit__kernel_optix_shadow_all_hit. */
                OPTIX_RAY_FLAG_ENFORCE_ANYHIT,
                1, /* SBT offset for PG_HITS */
                0,
                0,
                p0,
                p1,
                p2,
                p3,
                p4,
                p5,
                p6,
                p7);

  *num_recorded_hits = uint16_unpack_from_uint_0(p2);
  *throughput = __uint_as_float(p1);

  return p5;
}
#endif

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  uint p0 = 0;
  uint p1 = 0;
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = visibility;
  uint p5 = PRIMITIVE_NONE;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

  optixTrace(intersection_ray_valid(ray) ? kernel_data.device_bvh : 0,
             ray->P,
             ray->D,
             ray->tmin,
             ray->tmax,
             ray->time,
             ray_mask,
             /* Need to always call into __anyhit__kernel_optix_volume_test. */
             OPTIX_RAY_FLAG_ENFORCE_ANYHIT,
             3, /* SBT offset for PG_HITV */
             0,
             0,
             p0,
             p1,
             p2,
             p3,
             p4,
             p5,
             p6,
             p7);

  isect->t = __uint_as_float(p0);
  isect->u = __uint_as_float(p1);
  isect->v = __uint_as_float(p2);
  isect->prim = p3;
  isect->object = p4;
  isect->type = p5;

  return p5 != PRIMITIVE_NONE;
}
#endif

CCL_NAMESPACE_END
