/*
 * Copyright 2011-2013 Blender Foundation
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

#pragma once

#define __KERNEL_GPU__
#define __KERNEL_METAL__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifndef ATTR_FALLTHROUGH
#  define ATTR_FALLTHROUGH
#endif

#include <metal_atomic>
#include <metal_pack>
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#ifdef __METALRT__
using namespace metal::raytracing;
#endif

#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wuninitialized"

/* Qualifiers */

#define ccl_device
#define ccl_device_inline ccl_device
#define ccl_device_forceinline ccl_device
#define ccl_device_noinline ccl_device __attribute__((noinline))
#define ccl_device_noinline_cpu ccl_device
#define ccl_device_inline_method ccl_device
#define ccl_global device
#define ccl_inline_constant static constant constexpr
#define ccl_device_constant constant
#define ccl_constant constant
#define ccl_gpu_shared threadgroup
#define ccl_private thread
#define ccl_may_alias
#define ccl_restrict __restrict
#define ccl_loop_no_unroll
#define ccl_align(n) alignas(n)
#define ccl_optional_struct_init

/* No assert supported for Metal */

#define kernel_assert(cond)

#define ccl_gpu_global_id_x() metal_global_id
#define ccl_gpu_warp_size simdgroup_size
#define ccl_gpu_thread_idx_x simd_group_index
#define ccl_gpu_thread_mask(thread_warp) uint64_t((1ull << thread_warp) - 1)

#define ccl_gpu_ballot(predicate) ((uint64_t)((simd_vote::vote_t)simd_ballot(predicate)))
#define ccl_gpu_syncthreads() threadgroup_barrier(mem_flags::mem_threadgroup);

// clang-format off

/* kernel.h adapters */

#define ccl_gpu_kernel(block_num_threads, thread_num_registers)
#define ccl_gpu_kernel_threads(block_num_threads)

/* Convert a comma-separated list into a semicolon-separated list
 * (so that we can generate a struct based on kernel entry-point parameters). */
#define FN0()
#define FN1(p1) p1;
#define FN2(p1, p2) p1; p2;
#define FN3(p1, p2, p3) p1; p2; p3;
#define FN4(p1, p2, p3, p4) p1; p2; p3; p4;
#define FN5(p1, p2, p3, p4, p5) p1; p2; p3; p4; p5;
#define FN6(p1, p2, p3, p4, p5, p6) p1; p2; p3; p4; p5; p6;
#define FN7(p1, p2, p3, p4, p5, p6, p7) p1; p2; p3; p4; p5; p6; p7;
#define FN8(p1, p2, p3, p4, p5, p6, p7, p8) p1; p2; p3; p4; p5; p6; p7; p8;
#define FN9(p1, p2, p3, p4, p5, p6, p7, p8, p9) p1; p2; p3; p4; p5; p6; p7; p8; p9;
#define FN10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10;
#define FN11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11;
#define FN12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12;
#define FN13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13;
#define FN14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14;
#define FN15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15;
#define FN16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15; p16;
#define FN17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15; p16; p17;
#define FN18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15; p16; p17; p18;
#define FN19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15; p16; p17; p18; p19;
#define FN20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) p1; p2; p3; p4; p5; p6; p7; p8; p9; p10; p11; p12; p13; p14; p15; p16; p17; p18; p19; p20;
#define GET_LAST_ARG(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, ...) p20
#define PARAMS_MAKER(...) GET_LAST_ARG(__VA_ARGS__, FN20, FN19, FN18, FN17, FN16, FN15, FN14, FN13, FN12, FN11, FN10, FN9, FN8, FN7, FN6, FN5, FN4, FN3, FN2, FN1, FN0)

/* Generate a struct containing the entry-point parameters and a "run"
 * method which can access them implicitly via this-> */
#define ccl_gpu_kernel_signature(name, ...) \
struct kernel_gpu_##name \
{ \
  PARAMS_MAKER(__VA_ARGS__)(__VA_ARGS__) \
  void run(thread MetalKernelContext& context, \
           threadgroup int *simdgroup_offset, \
           const uint metal_global_id, \
           const ushort metal_local_id, \
           const ushort metal_local_size, \
           uint simdgroup_size, \
           uint simd_lane_index, \
           uint simd_group_index, \
           uint num_simd_groups) ccl_global const; \
}; \
kernel void cycles_metal_##name(device const kernel_gpu_##name *params_struct, \
                                constant KernelParamsMetal &ccl_restrict   _launch_params_metal, \
                                constant MetalAncillaries *_metal_ancillaries, \
                                threadgroup int *simdgroup_offset[[ threadgroup(0) ]], \
                                const uint metal_global_id [[thread_position_in_grid]], \
                                const ushort metal_local_id   [[thread_position_in_threadgroup]], \
                                const ushort metal_local_size [[threads_per_threadgroup]], \
                                uint simdgroup_size [[threads_per_simdgroup]], \
                                uint simd_lane_index [[thread_index_in_simdgroup]], \
                                uint simd_group_index [[simdgroup_index_in_threadgroup]], \
                                uint num_simd_groups [[simdgroups_per_threadgroup]]) { \
  MetalKernelContext context(_launch_params_metal, _metal_ancillaries); \
  params_struct->run(context, simdgroup_offset, metal_global_id, metal_local_id, metal_local_size, simdgroup_size, simd_lane_index, simd_group_index, num_simd_groups); \
} \
void kernel_gpu_##name::run(thread MetalKernelContext& context, \
                  threadgroup int *simdgroup_offset, \
                  const uint metal_global_id, \
                  const ushort metal_local_id, \
                  const ushort metal_local_size, \
                  uint simdgroup_size, \
                  uint simd_lane_index, \
                  uint simd_group_index, \
                  uint num_simd_groups) ccl_global const

#define ccl_gpu_kernel_call(x) context.x

/* define a function object where "func" is the lambda body, and additional parameters are used to specify captured state  */
#define ccl_gpu_kernel_lambda(func, ...) \
  struct KernelLambda \
  { \
    KernelLambda(ccl_private MetalKernelContext &_context) : context(_context) {} \
    ccl_private MetalKernelContext &context; \
    __VA_ARGS__; \
    int operator()(const int state) const { return (func); } \
  } ccl_gpu_kernel_lambda_pass(context)

// clang-format on

/* volumetric lambda functions - use function objects for lambda-like functionality */
#define VOLUME_READ_LAMBDA(function_call) \
  struct FnObjectRead { \
    KernelGlobals kg; \
    ccl_private MetalKernelContext *context; \
    int state; \
\
    VolumeStack operator()(const int i) const \
    { \
      return context->function_call; \
    } \
  } volume_read_lambda_pass{kg, this, state};

#define VOLUME_WRITE_LAMBDA(function_call) \
  struct FnObjectWrite { \
    KernelGlobals kg; \
    ccl_private MetalKernelContext *context; \
    int state; \
\
    void operator()(const int i, VolumeStack entry) const \
    { \
      context->function_call; \
    } \
  } volume_write_lambda_pass{kg, this, state};

/* make_type definitions with Metal style element initializers */
#ifdef make_float2
#  undef make_float2
#endif
#ifdef make_float3
#  undef make_float3
#endif
#ifdef make_float4
#  undef make_float4
#endif
#ifdef make_int2
#  undef make_int2
#endif
#ifdef make_int3
#  undef make_int3
#endif
#ifdef make_int4
#  undef make_int4
#endif
#ifdef make_uchar4
#  undef make_uchar4
#endif

#define make_float2(x, y) float2(x, y)
#define make_float3(x, y, z) float3(x, y, z)
#define make_float4(x, y, z, w) float4(x, y, z, w)
#define make_int2(x, y) int2(x, y)
#define make_int3(x, y, z) int3(x, y, z)
#define make_int4(x, y, z, w) int4(x, y, z, w)
#define make_uchar4(x, y, z, w) uchar4(x, y, z, w)

/* Math functions */

#define __uint_as_float(x) as_type<float>(x)
#define __float_as_uint(x) as_type<uint>(x)
#define __int_as_float(x) as_type<float>(x)
#define __float_as_int(x) as_type<int>(x)
#define __float2half(x) half(x)
#define powf(x, y) pow(float(x), float(y))
#define fabsf(x) fabs(float(x))
#define copysignf(x, y) copysign(float(x), float(y))
#define asinf(x) asin(float(x))
#define acosf(x) acos(float(x))
#define atanf(x) atan(float(x))
#define floorf(x) floor(float(x))
#define ceilf(x) ceil(float(x))
#define hypotf(x, y) hypot(float(x), float(y))
#define atan2f(x, y) atan2(float(x), float(y))
#define fmaxf(x, y) fmax(float(x), float(y))
#define fminf(x, y) fmin(float(x), float(y))
#define fmodf(x, y) fmod(float(x), float(y))
#define sinhf(x) sinh(float(x))
#define coshf(x) cosh(float(x))
#define tanhf(x) tanh(float(x))
#define saturatef(x) saturate(float(x))

/* Use native functions with possibly lower precision for performance,
 * no issues found so far. */
#define trigmode fast
#define sinf(x) trigmode::sin(float(x))
#define cosf(x) trigmode::cos(float(x))
#define tanf(x) trigmode::tan(float(x))
#define expf(x) trigmode::exp(float(x))
#define sqrtf(x) trigmode::sqrt(float(x))
#define logf(x) trigmode::log(float(x))

#define NULL 0

#define __device__

#ifdef __METALRT__

#  define __KERNEL_GPU_RAYTRACING__

#  if defined(__METALRT_MOTION__)
#    define METALRT_TAGS instancing, instance_motion, primitive_motion
#  else
#    define METALRT_TAGS instancing
#  endif /* __METALRT_MOTION__ */

typedef acceleration_structure<METALRT_TAGS> metalrt_as_type;
typedef intersection_function_table<triangle_data, METALRT_TAGS> metalrt_ift_type;
typedef metal::raytracing::intersector<triangle_data, METALRT_TAGS> metalrt_intersector_type;

#endif /* __METALRT__ */

/* texture bindings and sampler setup */

struct Texture2DParamsMetal {
  texture2d<float, access::sample> tex;
};
struct Texture3DParamsMetal {
  texture3d<float, access::sample> tex;
};

struct MetalAncillaries {
  device Texture2DParamsMetal *textures_2d;
  device Texture3DParamsMetal *textures_3d;

#ifdef __METALRT__
  metalrt_as_type accel_struct;
  metalrt_ift_type ift_default;
  metalrt_ift_type ift_shadow;
  metalrt_ift_type ift_local;
#endif
};

#include "util/half.h"
#include "util/types.h"

enum SamplerType {
  SamplerFilterNearest_AddressRepeat,
  SamplerFilterNearest_AddressClampEdge,
  SamplerFilterNearest_AddressClampZero,

  SamplerFilterLinear_AddressRepeat,
  SamplerFilterLinear_AddressClampEdge,
  SamplerFilterLinear_AddressClampZero,

  SamplerCount
};

constant constexpr array<sampler, SamplerCount> metal_samplers = {
    sampler(address::repeat, filter::nearest),
    sampler(address::clamp_to_edge, filter::nearest),
    sampler(address::clamp_to_zero, filter::nearest),
    sampler(address::repeat, filter::linear),
    sampler(address::clamp_to_edge, filter::linear),
    sampler(address::clamp_to_zero, filter::linear),
};
