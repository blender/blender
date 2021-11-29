/*
 * Copyright 2021 Blender Foundation
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
    typedef uint64_t ccl_gpu_tex_object;

    template<typename T>
    inline __attribute__((__always_inline__))
    T ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object tex, float x, float y) const {
      kernel_assert(0);
      return 0;
    }
    template<typename T>
    inline __attribute__((__always_inline__))
    T ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object tex, float x, float y, float z) const {
      kernel_assert(0);
      return 0;
    }

    // texture2d
    template<>
    inline __attribute__((__always_inline__))
    float4 ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object tex, float x, float y) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
      return metal_ancillaries->textures_2d[tid].tex.sample(metal_samplers[sid], float2(x, y));
    }
    template<>
    inline __attribute__((__always_inline__))
    float ccl_gpu_tex_object_read_2D(ccl_gpu_tex_object tex, float x, float y) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
      return metal_ancillaries->textures_2d[tid].tex.sample(metal_samplers[sid], float2(x, y)).x;
    }

    // texture3d
    template<>
    inline __attribute__((__always_inline__))
    float4 ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object tex, float x, float y, float z) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
      return metal_ancillaries->textures_3d[tid].tex.sample(metal_samplers[sid], float3(x, y, z));
    }
    template<>
    inline __attribute__((__always_inline__))
    float ccl_gpu_tex_object_read_3D(ccl_gpu_tex_object tex, float x, float y, float z) const {
      const uint tid(tex);
      const uint sid(tex >> 32);
      return metal_ancillaries->textures_3d[tid].tex.sample(metal_samplers[sid], float3(x, y, z)).x;
    }
#    include "kernel/device/gpu/image.h"

  // clang-format on
