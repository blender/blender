/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

// clang-format off
#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/device/gpu/image.h"  /* Texture lookup uses normal CUDA intrinsics. */

#include "kernel/tables.h"

#include "kernel/integrator/state.h"
#include "kernel/integrator/state_flow.h"
#include "kernel/integrator/state_util.h"

#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/intersect_shadow.h"
#include "kernel/integrator/intersect_subsurface.h"
#include "kernel/integrator/intersect_volume_stack.h"
// clang-format on

#define OPTIX_DEFINE_ABI_VERSION_ONLY
#include <optix_function_table.h>

template<typename T> ccl_device_forceinline T *get_payload_ptr_0()
{
  return pointer_unpack_from_uint<T>(optixGetPayload_0(), optixGetPayload_1());
}
template<typename T> ccl_device_forceinline T *get_payload_ptr_2()
{
  return pointer_unpack_from_uint<T>(optixGetPayload_2(), optixGetPayload_3());
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

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_closest()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_intersect_closest(nullptr, path_index, __params.render_buffer);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_shadow()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_intersect_shadow(nullptr, path_index);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_subsurface()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_intersect_subsurface(nullptr, path_index);
}

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_volume_stack()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_intersect_volume_stack(nullptr, path_index);
}

extern "C" __global__ void __miss__kernel_optix_miss()
{
  /* 'kernel_path_lamp_emission' checks intersection distance, so need to set it even on a miss. */
  optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
  optixSetPayload_5(PRIMITIVE_NONE);
}

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

  const int prim = optixGetPrimitiveIndex();

  Intersection *isect = &local_isect->hits[hit];
  isect->t = optixGetRayTmax();
  isect->prim = prim;
  isect->object = get_object_id();
  isect->type = kernel_tex_fetch(__objects, isect->object).primitive_type;

  const float2 barycentrics = optixGetTriangleBarycentrics();
  isect->u = 1.0f - barycentrics.y - barycentrics.x;
  isect->v = barycentrics.x;

  /* Record geometric normal. */
  const uint tri_vindex = kernel_tex_fetch(__tri_vindex, prim).w;
  const float3 tri_a = kernel_tex_fetch(__tri_verts, tri_vindex + 0);
  const float3 tri_b = kernel_tex_fetch(__tri_verts, tri_vindex + 1);
  const float3 tri_c = kernel_tex_fetch(__tri_verts, tri_vindex + 2);
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
  if ((kernel_tex_fetch(__objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#  endif

  float u = 0.0f, v = 0.0f;
  int type = 0;
  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    u = 1.0f - barycentrics.y - barycentrics.x;
    v = barycentrics.x;
    type = kernel_tex_fetch(__objects, object).primitive_type;
  }
#  ifdef __HAIR__
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    u = __uint_as_float(optixGetAttribute_0());
    v = __uint_as_float(optixGetAttribute_1());

    const KernelCurveSegment segment = kernel_tex_fetch(__curve_segments, prim);
    type = segment.type;
    prim = segment.prim;

#    if OPTIX_ABI_VERSION < 55
    /* Filter out curve endcaps. */
    if (u == 0.0f || u == 1.0f) {
      return optixIgnoreIntersection();
    }
#    endif
  }
#  endif
  else {
    type = kernel_tex_fetch(__objects, object).primitive_type;
    u = 0.0f;
    v = 0.0f;
  }

#  ifndef __TRANSPARENT_SHADOWS__
  /* No transparent shadows support compiled in, make opaque. */
  optixSetPayload_5(true);
  return optixTerminateRay();
#  else
  const uint max_hits = optixGetPayload_3();
  const uint num_hits_packed = optixGetPayload_2();
  const uint num_recorded_hits = uint16_unpack_from_uint_0(num_hits_packed);
  const uint num_hits = uint16_unpack_from_uint_1(num_hits_packed);

  /* If no transparent shadows, all light is blocked and we can stop immediately. */
  if (num_hits >= max_hits ||
      !(intersection_get_shader_flags(NULL, prim, type) & SD_HAS_TRANSPARENT_SHADOW)) {
    optixSetPayload_5(true);
    return optixTerminateRay();
  }

  /* Always use baked shadow transparency for curves. */
  if (type & PRIMITIVE_CURVE) {
    float throughput = __uint_as_float(optixGetPayload_1());
    throughput *= intersection_curve_shadow_transparency(nullptr, object, prim, u);
    optixSetPayload_1(__float_as_uint(throughput));
    optixSetPayload_2(uint16_pack_to_uint(num_recorded_hits, num_hits + 1));

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
  optixSetPayload_2(uint16_pack_to_uint(num_recorded_hits + 1, num_hits + 1));

  uint record_index = num_recorded_hits;

  const IntegratorShadowState state = optixGetPayload_0();

  const uint max_record_hits = min(max_hits, INTEGRATOR_SHADOW_ISECT_SIZE);
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
  if ((kernel_tex_fetch(__objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#endif

  if ((kernel_tex_fetch(__object_flag, object) & SD_OBJECT_HAS_VOLUME) == 0) {
    return optixIgnoreIntersection();
  }
}

extern "C" __global__ void __anyhit__kernel_optix_visibility_test()
{
#ifdef __HAIR__
#  if OPTIX_ABI_VERSION < 55
  if (optixGetPrimitiveType() == OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE) {
    /* Filter out curve endcaps. */
    const float u = __uint_as_float(optixGetAttribute_0());
    if (u == 0.0f || u == 1.0f) {
      return optixIgnoreIntersection();
    }
  }
#  endif
#endif

#ifdef __VISIBILITY_FLAG__
  const uint object = get_object_id();
  const uint visibility = optixGetPayload_4();
  if ((kernel_tex_fetch(__objects, object).visibility & visibility) == 0) {
    return optixIgnoreIntersection();
  }

  /* Shadow ray early termination. */
  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    return optixTerminateRay();
  }
#endif
}

extern "C" __global__ void __closesthit__kernel_optix_hit()
{
  const int object = get_object_id();
  const int prim = optixGetPrimitiveIndex();

  optixSetPayload_0(__float_as_uint(optixGetRayTmax())); /* Intersection distance */
  optixSetPayload_4(object);

  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    optixSetPayload_1(__float_as_uint(1.0f - barycentrics.y - barycentrics.x));
    optixSetPayload_2(__float_as_uint(barycentrics.x));
    optixSetPayload_3(prim);
    optixSetPayload_5(kernel_tex_fetch(__objects, object).primitive_type);
  }
  else if ((optixGetHitKind() & (~PRIMITIVE_MOTION)) != PRIMITIVE_POINT) {
    const KernelCurveSegment segment = kernel_tex_fetch(__curve_segments, prim);
    optixSetPayload_1(optixGetAttribute_0()); /* Same as 'optixGetCurveParameter()' */
    optixSetPayload_2(optixGetAttribute_1());
    optixSetPayload_3(segment.prim);
    optixSetPayload_5(segment.type);
  }
  else {
    optixSetPayload_1(0);
    optixSetPayload_2(0);
    optixSetPayload_3(prim);
    optixSetPayload_5(kernel_tex_fetch(__objects, object).primitive_type);
  }
}

#ifdef __HAIR__
ccl_device_inline void optix_intersection_curve(const int prim, const int type)
{
  const int object = get_object_id();

#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_tex_fetch(__objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  float3 P = optixGetObjectRayOrigin();
  float3 dir = optixGetObjectRayDirection();

  /* The direction is not normalized by default, but the curve intersection routine expects that */
  float len;
  dir = normalize_len(dir, &len);

#  ifdef __OBJECT_MOTION__
  const float time = optixGetRayTime();
#  else
  const float time = 0.0f;
#  endif

  Intersection isect;
  isect.t = optixGetRayTmax();
  /* Transform maximum distance into object space. */
  if (isect.t != FLT_MAX)
    isect.t *= len;

  if (curve_intersect(NULL, &isect, P, dir, isect.t, object, prim, time, type)) {
    static_assert(PRIMITIVE_ALL < 128, "Values >= 128 are reserved for OptiX internal use");
    optixReportIntersection(isect.t / len,
                            type & PRIMITIVE_ALL,
                            __float_as_int(isect.u),  /* Attribute_0 */
                            __float_as_int(isect.v)); /* Attribute_1 */
  }
}

extern "C" __global__ void __intersection__curve_ribbon()
{
  const KernelCurveSegment segment = kernel_tex_fetch(__curve_segments, optixGetPrimitiveIndex());
  const int prim = segment.prim;
  const int type = segment.type;
  if (type & PRIMITIVE_CURVE_RIBBON) {
    optix_intersection_curve(prim, type);
  }
}

#endif

#ifdef __POINTCLOUD__
extern "C" __global__ void __intersection__point()
{
  const int prim = optixGetPrimitiveIndex();
  const int object = get_object_id();
  const int type = kernel_tex_fetch(__objects, object).primitive_type;

#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_tex_fetch(__objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  float3 P = optixGetObjectRayOrigin();
  float3 dir = optixGetObjectRayDirection();

  /* The direction is not normalized by default, the point intersection routine expects that. */
  float len;
  dir = normalize_len(dir, &len);

#  ifdef __OBJECT_MOTION__
  const float time = optixGetRayTime();
#  else
  const float time = 0.0f;
#  endif

  Intersection isect;
  isect.t = optixGetRayTmax();
  /* Transform maximum distance into object space. */
  if (isect.t != FLT_MAX) {
    isect.t *= len;
  }

  if (point_intersect(NULL, &isect, P, dir, isect.t, object, prim, time, type)) {
    static_assert(PRIMITIVE_ALL < 128, "Values >= 128 are reserved for OptiX internal use");
    optixReportIntersection(isect.t / len, type & PRIMITIVE_ALL);
  }
}
#endif
