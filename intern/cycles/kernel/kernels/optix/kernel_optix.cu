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

#include "kernel/kernel_compat_optix.h"
#include "util/util_atomic.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_globals.h"
#include "../cuda/kernel_cuda_image.h"  // Texture lookup uses normal CUDA intrinsics

#include "kernel/kernel_path.h"
#include "kernel/kernel_bake.h"

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
  if (always)
    // Can just remove the high bit since instance always contains object ID
    return object & 0x7FFFFF;
  // Set to OBJECT_NONE if this is not an instanced object
  else if (object & 0x800000)
    object = OBJECT_NONE;
  return object;
}

extern "C" __global__ void __raygen__kernel_optix_path_trace()
{
  KernelGlobals kg;  // Allocate stack storage for common data

  const uint3 launch_index = optixGetLaunchIndex();
  // Keep threads for same pixel together to improve occupancy of warps
  uint pixel_offset = launch_index.x / __params.tile.num_samples;
  uint sample_offset = launch_index.x % __params.tile.num_samples;

  kernel_path_trace(&kg,
                    __params.tile.buffer,
                    __params.tile.start_sample + sample_offset,
                    __params.tile.x + pixel_offset,
                    __params.tile.y + launch_index.y,
                    __params.tile.offset,
                    __params.tile.stride);
}

#ifdef __BAKING__
extern "C" __global__ void __raygen__kernel_optix_bake()
{
  KernelGlobals kg;
  const ShaderParams &p = __params.shader;
  kernel_bake_evaluate(&kg,
                       p.input,
                       p.output,
                       (ShaderEvalType)p.type,
                       p.filter,
                       p.sx + optixGetLaunchIndex().x,
                       p.offset,
                       p.sample);
}
#endif

extern "C" __global__ void __raygen__kernel_optix_displace()
{
  KernelGlobals kg;
  const ShaderParams &p = __params.shader;
  kernel_displace_evaluate(&kg, p.input, p.output, p.sx + optixGetLaunchIndex().x);
}

extern "C" __global__ void __raygen__kernel_optix_background()
{
  KernelGlobals kg;
  const ShaderParams &p = __params.shader;
  kernel_background_evaluate(&kg, p.input, p.output, p.sx + optixGetLaunchIndex().x);
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

  int hit = 0;
  uint *const lcg_state = get_payload_ptr_0<uint>();
  LocalIntersection *const local_isect = get_payload_ptr_2<LocalIntersection>();

  if (lcg_state) {
    const uint max_hits = optixGetPayload_5();
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
      // Record closest intersection only (do not terminate ray here, since there is no guarantee
      // about distance ordering in anyhit)
      return optixIgnoreIntersection();
    }

    local_isect->num_hits = 1;
  }

  Intersection *isect = &local_isect->hits[hit];
  isect->t = optixGetRayTmax();
  isect->prim = optixGetPrimitiveIndex();
  isect->object = get_object_id();
  isect->type = kernel_tex_fetch(__prim_type, isect->prim);

  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    isect->u = 1.0f - barycentrics.y - barycentrics.x;
    isect->v = barycentrics.x;
  }
  else {
    isect->u = __uint_as_float(optixGetAttribute_0());
    isect->v = __uint_as_float(optixGetAttribute_1());
  }

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
  const uint prim = optixGetPrimitiveIndex();
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = optixGetPayload_4();
  if ((kernel_tex_fetch(__prim_visibility, prim) & visibility) == 0) {
    return optixIgnoreIntersection();
  }
#  endif

  // Offset into array with num_hits
  Intersection *const isect = get_payload_ptr_0<Intersection>() + optixGetPayload_2();
  isect->t = optixGetRayTmax();
  isect->prim = prim;
  isect->object = get_object_id();
  isect->type = kernel_tex_fetch(__prim_type, prim);

  if (optixIsTriangleHit()) {
    const float2 barycentrics = optixGetTriangleBarycentrics();
    isect->u = 1.0f - barycentrics.y - barycentrics.x;
    isect->v = barycentrics.x;
  }
  else {
    isect->u = __uint_as_float(optixGetAttribute_0());
    isect->v = __uint_as_float(optixGetAttribute_1());
  }

#  ifdef __TRANSPARENT_SHADOWS__
  // Detect if this surface has a shader with transparent shadows
  if (!shader_transparent_shadow(NULL, isect) || optixGetPayload_2() >= optixGetPayload_3()) {
#  endif
    // This is an opaque hit or the hit limit has been reached, abort traversal
    optixSetPayload_5(true);
    return optixTerminateRay();
#  ifdef __TRANSPARENT_SHADOWS__
  }

  // TODO(pmours): Do we need REQUIRE_UNIQUE_ANYHIT for this to work?
  optixSetPayload_2(optixGetPayload_2() + 1);  // num_hits++

  // Continue tracing
  optixIgnoreIntersection();
#  endif
#endif
}

extern "C" __global__ void __anyhit__kernel_optix_visibility_test()
{
  uint visibility = optixGetPayload_4();
#ifdef __VISIBILITY_FLAG__
  const uint prim = optixGetPrimitiveIndex();
  if ((kernel_tex_fetch(__prim_visibility, prim) & visibility) == 0)
    return optixIgnoreIntersection();
#endif

  // Shadow ray early termination
  if (visibility & PATH_RAY_SHADOW_OPAQUE)
    return optixTerminateRay();
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
    optixSetPayload_1(optixGetAttribute_0());
    optixSetPayload_2(optixGetAttribute_1());
  }
}

#ifdef __HAIR__
extern "C" __global__ void __intersection__curve()
{
  const uint prim = optixGetPrimitiveIndex();
  const uint object = get_object_id<true>();
  const uint type = kernel_tex_fetch(__prim_type, prim);
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

  if (curve_intersect(NULL, &isect, P, dir, visibility, object, prim, time, type)) {
    optixReportIntersection(isect.t / len,
                            type & PRIMITIVE_ALL,
                            __float_as_int(isect.u),   // Attribute_0
                            __float_as_int(isect.v));  // Attribute_1
  }
}
#endif

#ifdef __KERNEL_DEBUG__
extern "C" __global__ void __exception__kernel_optix_exception()
{
  printf("Unhandled exception occured: code %d!\n", optixGetExceptionCode());
}
#endif
