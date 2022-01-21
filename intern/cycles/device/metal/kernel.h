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

#pragma once

#ifdef WITH_METAL

#  include "device/kernel.h"
#  include <Metal/Metal.h>

CCL_NAMESPACE_BEGIN

class MetalDevice;

enum {
  METALRT_FUNC_DEFAULT_TRI,
  METALRT_FUNC_DEFAULT_BOX,
  METALRT_FUNC_SHADOW_TRI,
  METALRT_FUNC_SHADOW_BOX,
  METALRT_FUNC_LOCAL_TRI,
  METALRT_FUNC_LOCAL_BOX,
  METALRT_FUNC_CURVE_RIBBON,
  METALRT_FUNC_CURVE_RIBBON_SHADOW,
  METALRT_FUNC_CURVE_ALL,
  METALRT_FUNC_CURVE_ALL_SHADOW,
  METALRT_FUNC_POINT,
  METALRT_FUNC_POINT_SHADOW,
  METALRT_FUNC_NUM
};

enum { METALRT_TABLE_DEFAULT, METALRT_TABLE_SHADOW, METALRT_TABLE_LOCAL, METALRT_TABLE_NUM };

/* Pipeline State Object types */
enum {
  /* A kernel that can be used with all scenes, supporting all features.
   * It is slow to compile, but only needs to be compiled once and is then
   * cached for future render sessions. This allows a render to get underway
   * on the GPU quickly.
   */
  PSO_GENERIC,

  /* A kernel that is relatively quick to compile, but is specialized for the
   * scene being rendered. It only contains the functionality and even baked in
   * constants for values that means it needs to be recompiled whenever a
   * dependent setting is changed. The render performance of this kernel is
   * significantly faster though, and justifies the extra compile time.
   */
  /* METAL_WIP: This isn't used and will require more changes to enable. */
  PSO_SPECIALISED,

  PSO_NUM
};

const char *kernel_type_as_string(int kernel_type);

struct MetalKernelPipeline {
  void release()
  {
    if (pipeline) {
      [pipeline release];
      pipeline = nil;
      if (@available(macOS 11.0, *)) {
        for (int i = 0; i < METALRT_TABLE_NUM; i++) {
          if (intersection_func_table[i]) {
            [intersection_func_table[i] release];
            intersection_func_table[i] = nil;
          }
        }
      }
    }
    if (function) {
      [function release];
      function = nil;
    }
    if (@available(macOS 11.0, *)) {
      for (int i = 0; i < METALRT_TABLE_NUM; i++) {
        if (intersection_func_table[i]) {
          [intersection_func_table[i] release];
        }
      }
    }
  }

  bool loaded = false;
  id<MTLFunction> function = nil;
  id<MTLComputePipelineState> pipeline = nil;

  API_AVAILABLE(macos(11.0))
  id<MTLIntersectionFunctionTable> intersection_func_table[METALRT_TABLE_NUM] = {nil};
};

struct MetalKernelLoadDesc {
  int pso_index = 0;
  const char *function_name = nullptr;
  int kernel_index = 0;
  int threads_per_threadgroup = 0;
  MTLFunctionConstantValues *constant_values = nullptr;
  NSArray *linked_functions = nullptr;

  struct IntersectorFunctions {
    NSArray *defaults;
    NSArray *shadow;
    NSArray *local;
    NSArray *operator[](int index) const
    {
      if (index == METALRT_TABLE_DEFAULT)
        return defaults;
      if (index == METALRT_TABLE_SHADOW)
        return shadow;
      return local;
    }
  } intersector_functions = {nullptr};
};

/* Metal kernel and associate occupancy information. */
class MetalDeviceKernel {
 public:
  ~MetalDeviceKernel();

  bool load(MetalDevice *device, MetalKernelLoadDesc const &desc, class MD5Hash const &md5);

  void mark_loaded(int pso_index)
  {
    pso[pso_index].loaded = true;
  }

  int get_num_threads_per_block() const
  {
    return num_threads_per_block;
  }
  const MetalKernelPipeline &get_pso() const;

  double load_duration = 0.0;

 private:
  MetalKernelPipeline pso[PSO_NUM];

  int num_threads_per_block = 0;
};

/* Cache of Metal kernels for each DeviceKernel. */
class MetalDeviceKernels {
 public:
  bool load(MetalDevice *device, int kernel_type);
  bool available(DeviceKernel kernel) const;
  const MetalDeviceKernel &get(DeviceKernel kernel) const;

  MetalDeviceKernel kernels_[DEVICE_KERNEL_NUM];

  id<MTLFunction> rt_intersection_funcs[PSO_NUM][METALRT_FUNC_NUM] = {{nil}};

  string loaded_md5[PSO_NUM];
};

CCL_NAMESPACE_END

#endif /* WITH_METAL */
