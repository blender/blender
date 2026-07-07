/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* OptiX implementation of ray-scene intersection.
 *
 * Note on the payload registers.
 * Intersection and filtering functions might be sharing the same registers, even if it is not
 * very obvious from the trace/traverse call. The registers that have special meaning and are to
 * be kept "locked" to their meaning:
 *   uint p4 = visibility;
 *   uint p6 = pointer_pack_to_uint_0(ray);
 *   uint p7 = pointer_pack_to_uint_1(ray); */

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

ccl_device_forceinline Intersection get_intersection()
{
  Intersection isect;

  isect.t = optixGetRayTmax();
  isect.prim = optixGetPrimitiveIndex();
  isect.object = get_object_id();

  if (optixIsTriangleHit()) {
    /* Triangle. */
    const float2 barycentrics = optixGetTriangleBarycentrics();
    isect.u = barycentrics.x;
    isect.v = barycentrics.y;
    isect.type = kernel_data_fetch(objects, isect.object).primitive_type;
  }
#ifdef __HAIR__
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    /* Curve. */
    isect.u = __uint_as_float(optixGetAttribute_0());
    isect.v = __uint_as_float(optixGetAttribute_1());

    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, isect.prim);
    isect.type = segment.type;
    isect.prim = segment.prim;
  }
#endif
  else {
    /* Point. */
    isect.u = 0.0f;
    isect.v = 0.0f;
    isect.type = kernel_data_fetch(objects, isect.object).primitive_type;
  }

  return isect;
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

  const float isect_t = optixGetRayTmax();
  uint *const lcg_state = get_payload_ptr_0<uint>();
  LocalIntersection *const local_isect = get_payload_ptr_2<LocalIntersection>();

  const int hit_index = local_intersect_get_record_index(
      local_isect, isect_t, lcg_state, max_hits);
  if (hit_index == -1) {
    return optixIgnoreIntersection();
  }

  Intersection *isect = &local_isect->hits[hit_index];
  isect->t = isect_t;
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

  local_isect->Ng[hit_index] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  /* Continue tracing (without this the trace call would return after the first hit). */
  optixIgnoreIntersection();
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_shadow_all_hit()
{
#ifdef __TRANSPARENT_SHADOWS__
  KernelGlobals kg = nullptr;

  ccl_private BVHShadowAllPayload *payload = get_payload_ptr_0<BVHShadowAllPayload>();
  const uint ray_visibility = optixGetPayload_4();
  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();

  Intersection isect = get_intersection();
  if (!bvh_shadow_all_anyhit_filter<true>(
          kg, payload->state, *payload, ray->self, ray_visibility, isect))
  {
    optixTerminateRay();
    return;
  }

  /* The idea here is to accept the hit, so that traversal won't consider any more hits beyond the
   * distance of the current hit anymore.
   *
   * We could accept the hit which is furthest away from the ones that are already recorded (for
   * this `>` needs to be replaced with `>=`). However, doing so has a performance impact in the
   * pabellon benchmark scene. The hypothesis here is that allowing to traverse one extra hit after
   * the array is filled allows to hit an opaque surface and do early exit from the shadow shading.
   *
   * Similar to this logic (allowing an extra hit) was in the original OptiX integration, so we
   * just keep following it to avoid performance regression. There is no the correct solution here,
   * as it depends on the scene. For example, if there are many transparent surfaces with no opaque
   * hit then it is faster to start accepting hits as soon as possible. However, if there are many
   * transparent surfaces, followed up with an opaque surface, it is faster to not accept any hit
   * and allow the opaque optimization to lead to an early output from the intersect-shade loop. */
  if (isect.t > payload->max_record_isect_t) {
    return;
  }

  optixIgnoreIntersection();
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_volume_test()
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  if (!optixIsTriangleHit()) {
    /* Ignore curves. */
    return optixIgnoreIntersection();
  }
#endif

  KernelGlobals kg = nullptr;

  const int object = get_object_id();
  const int prim = optixGetPrimitiveIndex();
  ccl_private Ray *const ray = get_payload_ptr_6<Ray>();
  const uint ray_visibility = optixGetPayload_4();

  if (bvh_volume_anyhit_triangle_filter(kg, object, prim, ray->self, ray_visibility)) {
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
  /* Note: some registers have hardcoded meaning.
   * Be careful when changing the values here. See the note at the top of this file for more
   * details. */
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
  /* Note: some registers have hardcoded meaning.
   * Be careful when changing the values here. See the note at the top of this file for more
   * details. */
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
  /* Note: some registers have hardcoded meaning.
   * Be careful when changing the values here. See the note at the top of this file for more
   * details. */
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

#ifdef __TRANSPARENT_SHADOWS__
ccl_device_intersect void scene_intersect_shadow_all_optix(
    const ccl_private Ray *ccl_restrict ray,
    const uint ray_visibility,
    ccl_private BVHShadowAllPayload &ccl_restrict payload)
{
  /* Note: some registers have hardcoded meaning.
   * Be careful when changing the values here. See the note at the top of this file for more
   * details. */
  uint p0 = pointer_pack_to_uint_0(&payload);
  uint p1 = pointer_pack_to_uint_1(&payload);
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = ray_visibility;
  uint p5 = 0;
  uint p6 = pointer_pack_to_uint_0(ray);
  uint p7 = pointer_pack_to_uint_1(ray);

  uint ray_mask = ray_visibility & 0xFF;
  if (0 == ray_mask && (ray_visibility & ~0xFF) != 0) {
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
}
#endif

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  /* Note: some registers have hardcoded meaning.
   * Be careful when changing the values here. See the note at the top of this file for more
   * details. */
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
