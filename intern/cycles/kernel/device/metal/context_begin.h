/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

// clang-format off

#ifdef WITH_NANOVDB
#  define NDEBUG /* Disable "assert" in device code */
#  define NANOVDB_USE_INTRINSICS
#  include "nanovdb/NanoVDB.h"
#  include "nanovdb/util/SampleFromVoxels.h"
#endif

/* Open the Metal kernel context class
 * Necessary to access resource bindings */
class MetalKernelContext {
  public:
    constant KernelParamsMetal &launch_params_metal;
    constant MetalAncillaries *metal_ancillaries;

    MetalKernelContext(constant KernelParamsMetal &_launch_params_metal, constant MetalAncillaries * _metal_ancillaries)
    : launch_params_metal(_launch_params_metal), metal_ancillaries(_metal_ancillaries)
    {}

    MetalKernelContext(constant KernelParamsMetal &_launch_params_metal)
    : launch_params_metal(_launch_params_metal)
    {}

    /* texture fetch adapter functions */
    typedef uint64_t ccl_gpu_tex_object_2D;
    typedef uint64_t ccl_gpu_tex_object_3D;

    template<typename T>
    inline __attribute__((__always_inline__))
    T ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object_2D tex, float x, float y) const {
      kernel_assert(0);
      return 0;
    }
    template<typename T>
    inline __attribute__((__always_inline__))
    T ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object_3D tex, float x, float y, float z) const {
      kernel_assert(0);
      return 0;
    }

#ifdef __KERNEL_METAL_INTEL__
    template<typename TextureType, typename CoordsType>
    inline __attribute__((__always_inline__))
    auto ccl_gpu_tex_object_read_intel_workaround(TextureType texture_array,
                                                  const uint tid, const uint sid,
                                                  CoordsType coords) const
    {
      switch(sid) {
        default:
        case 0: return texture_array[tid].tex.sample(sampler(address::repeat, filter::nearest), coords);
        case 1: return texture_array[tid].tex.sample(sampler(address::clamp_to_edge, filter::nearest), coords);
        case 2: return texture_array[tid].tex.sample(sampler(address::clamp_to_zero, filter::nearest), coords);
        case 3: return texture_array[tid].tex.sample(sampler(address::mirrored_repeat, filter::nearest), coords);
        case 4: return texture_array[tid].tex.sample(sampler(address::repeat, filter::linear), coords);
        case 5: return texture_array[tid].tex.sample(sampler(address::clamp_to_edge, filter::linear), coords);
        case 6: return texture_array[tid].tex.sample(sampler(address::clamp_to_zero, filter::linear), coords);
        case 7: return texture_array[tid].tex.sample(sampler(address::mirrored_repeat, filter::linear), coords);
      }
    }
#endif

    // texture2d
    template<>
    inline __attribute__((__always_inline__))
    float4 ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object_2D tex, float x, float y) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
#ifndef __KERNEL_METAL_INTEL__
      return metal_ancillaries->textures_2d[tid].tex.sample(metal_samplers[sid], float2(x, y));
#else
      return ccl_gpu_tex_object_read_intel_workaround(metal_ancillaries->textures_2d, tid, sid, float2(x, y));
#endif
    }
    template<>
    inline __attribute__((__always_inline__))
    float ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object_2D tex, float x, float y) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
#ifndef __KERNEL_METAL_INTEL__
      return metal_ancillaries->textures_2d[tid].tex.sample(metal_samplers[sid], float2(x, y)).x;
#else
      return ccl_gpu_tex_object_read_intel_workaround(metal_ancillaries->textures_2d, tid, sid, float2(x, y)).x;
#endif
    }

    // texture3d
    template<>
    inline __attribute__((__always_inline__))
    float4 ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object_3D tex, float x, float y, float z) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
#ifndef __KERNEL_METAL_INTEL__
      return metal_ancillaries->textures_3d[tid].tex.sample(metal_samplers[sid], float3(x, y, z));
#else
      return ccl_gpu_tex_object_read_intel_workaround(metal_ancillaries->textures_3d, tid, sid, float3(x, y, z));
#endif
    }
    template<>
    inline __attribute__((__always_inline__))
    float ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object_3D tex, float x, float y, float z) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
#ifndef __KERNEL_METAL_INTEL__
      return metal_ancillaries->textures_3d[tid].tex.sample(metal_samplers[sid], float3(x, y, z)).x;
#else
      return ccl_gpu_tex_object_read_intel_workaround(metal_ancillaries->textures_3d, tid, sid, float3(x, y, z)).x;
#endif
    }
#    include "kernel/device/gpu/image.h"

  // clang-format on
