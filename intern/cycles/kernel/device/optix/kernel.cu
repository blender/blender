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

#include "kernel/device/gpu/image.h"  // Texture lookup uses normal CUDA intrinsics

#include "kernel/integrator/integrator_state.h"
#include "kernel/integrator/integrator_state_flow.h"
#include "kernel/integrator/integrator_state_util.h"

#include "kernel/integrator/integrator_intersect_closest.h"
#include "kernel/integrator/integrator_intersect_shadow.h"
#include "kernel/integrator/integrator_intersect_subsurface.h"
#include "kernel/integrator/integrator_intersect_volume_stack.h"

// clang-format on

template<typename T> ccl_device_forceinline T *get_payload_ptr_0()
{
  return (T *)(((uint64_t)optixGetPayload_1() << 32) | optixGetPayload_0());
}
template<typename T> ccl_device_forceinline T *get_payload_ptr_2()
{
  return (T *)(((uint64_t)optixGetPayload_3() << 32) | optixGetPayload_2());
}

template<bool always = false> ccl_device_forceinline uint get_object_id()
{
#ifdef __OBJECT_MOTION__
  // Always get the the instance ID from the TLAS
  // There might be a motion transform node between TLAS and BLAS which does not have one
  uint object = optixGetInstanceIdFromHandle(optixGetTransformListHandle(0));
#else
  uint object = optixGetInstanceId();
#endif
  // Choose between always returning object ID or only for instances
  if (always || (object & 1) == 0)
    // Can just remove the low bit since instance always contains object ID
    return object >> 1;
  else
    // Set to OBJECT_NONE if this is not an instanced object
    return OBJECT_NONE;
}

extern "C" __global__ void __raygen__kernel_optix_integrator_intersect_closest()
{
  const int global_index = optixGetLaunchIndex().x;
  const int path_index = (__params.path_index_array) ? __params.path_index_array[global_index] :
                                                       global_index;
  integrator_intersect_closest(nullptr, path_index);
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
  // 'kernel_path_lamp_emission' checks intersection distance, so need to set it even on a miss
  optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
  optixSetPayload_5(PRIMITIVE_NONE);
}

extern "C" __global__ void __anyhit__kernel_optix_local_hit()
{
#ifdef __BVH_LOCAL__
  const uint object = get_object_id<true>();
  if (object != optixGetPayload_4() /* local_object */) {
    // Only intersect with matching object
    return optixIgnoreIntersection();
  }

  const uint max_hits = optixGetPayload_5();
  if (max_hits == 0) {
    // Special case for when no hit information is requested, just report that something was hit
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
      // Record closest intersection only
      // Do not terminate ray here, since there is no guarantee about distance ordering in any-hit
      return optixIgnoreIntersection();
    }

    local_isect->num_hits = 1;
  }

  Intersection *isect = &local_isect->hits[hit];
  isect->t = optixGetRayTmax();
  isect->prim = optixGetPrimitiveIndex();
  isect->object = get_object_id();
  isect->type = kernel_tex_fetch(__prim_type, isect->prim);

  const float2 barycentrics = optixGetTriangleBarycentrics();
  isect->u = 1.0f - barycentrics.y - barycentrics.x;
  isect->v = barycentrics.x;

  // Record geometric normal
  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, isect->prim);
  const float3 tri_a = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0));
  const float3 tri_b = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1));
  const float3 tri_c = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2));
  local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  // Continue tracing (without this the trace call would return after the first hit)
  optixIgnoreIntersection();
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_shadow_all_hit()
{
#ifdef __SHADOW_RECORD_ALL__
  bool ignore_intersection = false;

  const uint prim = optixGetPrimitiveIndex();
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_tex_fetch(__prim_visibility, prim) & visibility) == 0) {
    ignore_intersection = true;
  }
#  endif

  float u = 0.0f, v = 0.0f;
  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    u = 1.0f - barycentrics.y - barycentrics.x;
    v = barycentrics.x;
  }
#  ifdef __HAIR__
  else {
    u = __uint_as_float(optixGetAttribute_0());
    v = __uint_as_float(optixGetAttribute_1());

    // Filter out curve endcaps
    if (u == 0.0f || u == 1.0f) {
      ignore_intersection = true;
    }
  }
#  endif

  int num_hits = optixGetPayload_2();
  int record_index = num_hits;
  const int max_hits = optixGetPayload_3();

  if (!ignore_intersection) {
    optixSetPayload_2(num_hits + 1);
  }

  Intersection *const isect_array = get_payload_ptr_0<Intersection>();

#  ifdef __TRANSPARENT_SHADOWS__
  if (num_hits >= max_hits) {
    /* If maximum number of hits reached, find a hit to replace. */
    const int num_recorded_hits = min(max_hits, num_hits);
    float max_recorded_t = isect_array[0].t;
    int max_recorded_hit = 0;

    for (int i = 1; i < num_recorded_hits; i++) {
      if (isect_array[i].t > max_recorded_t) {
        max_recorded_t = isect_array[i].t;
        max_recorded_hit = i;
      }
    }

    if (optixGetRayTmax() >= max_recorded_t) {
      /* Accept hit, so that OptiX won't consider any more hits beyond the distance of the current
       * hit anymore. */
      return;
    }

    record_index = max_recorded_hit;
  }
#  endif

  if (!ignore_intersection) {
    Intersection *const isect = isect_array + record_index;
    isect->u = u;
    isect->v = v;
    isect->t = optixGetRayTmax();
    isect->prim = prim;
    isect->object = get_object_id();
    isect->type = kernel_tex_fetch(__prim_type, prim);

#  ifdef __TRANSPARENT_SHADOWS__
    // Detect if this surface has a shader with transparent shadows
    if (!shader_transparent_shadow(NULL, isect) || max_hits == 0) {
#  endif
      // If no transparent shadows, all light is blocked and we can stop immediately
      optixSetPayload_5(true);
      return optixTerminateRay();
#  ifdef __TRANSPARENT_SHADOWS__
    }
#  endif
  }

  // Continue tracing
  optixIgnoreIntersection();
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_visibility_test()
{
  uint visibility = optixGetPayload_4();
#ifdef __VISIBILITY_FLAG__
  const uint prim = optixGetPrimitiveIndex();
  if ((kernel_tex_fetch(__prim_visibility, prim) & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#endif

#ifdef __HAIR__
  if (!optixIsTriangleHit()) {
    // Filter out curve endcaps
    const float u = __uint_as_float(optixGetAttribute_0());
    if (u == 0.0f || u == 1.0f) {
      return optixIgnoreIntersection();
    }
  }
#endif

  // Shadow ray early termination
  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    return optixTerminateRay();
  }
}

extern "C" __global__ void __closesthit__kernel_optix_hit()
{
  optixSetPayload_0(__float_as_uint(optixGetRayTmax()));  // Intersection distance
  optixSetPayload_3(optixGetPrimitiveIndex());
  optixSetPayload_4(get_object_id());
  // Can be PRIMITIVE_TRIANGLE and PRIMITIVE_MOTION_TRIANGLE or curve type and segment index
  optixSetPayload_5(kernel_tex_fetch(__prim_type, optixGetPrimitiveIndex()));

  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    optixSetPayload_1(__float_as_uint(1.0f - barycentrics.y - barycentrics.x));
    optixSetPayload_2(__float_as_uint(barycentrics.x));
  }
  else {
    optixSetPayload_1(optixGetAttribute_0());  // Same as 'optixGetCurveParameter()'
    optixSetPayload_2(optixGetAttribute_1());
  }
}

#ifdef __HAIR__
ccl_device_inline void optix_intersection_curve(const uint prim, const uint type)
{
  const uint object = get_object_id<true>();
  const uint visibility = optixGetPayload_4();

  float3 P = optixGetObjectRayOrigin();
  float3 dir = optixGetObjectRayDirection();

  // The direction is not normalized by default, but the curve intersection routine expects that
  float len;
  dir = normalize_len(dir, &len);

#  ifdef __OBJECT_MOTION__
  const float time = optixGetRayTime();
#  else
  const float time = 0.0f;
#  endif

  Intersection isect;
  isect.t = optixGetRayTmax();
  // Transform maximum distance into object space
  if (isect.t != FLT_MAX)
    isect.t *= len;

  if (curve_intersect(NULL, &isect, P, dir, isect.t, visibility, object, prim, time, type)) {
    optixReportIntersection(isect.t / len,
                            type & PRIMITIVE_ALL,
                            __float_as_int(isect.u),   // Attribute_0
                            __float_as_int(isect.v));  // Attribute_1
  }
}

extern "C" __global__ void __intersection__curve_ribbon()
{
  const uint prim = optixGetPrimitiveIndex();
  const uint type = kernel_tex_fetch(__prim_type, prim);

  if (type & (PRIMITIVE_CURVE_RIBBON | PRIMITIVE_MOTION_CURVE_RIBBON)) {
    optix_intersection_curve(prim, type);
  }
}
#endif
