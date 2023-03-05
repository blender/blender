/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

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
  METALRT_FUNC_LOCAL_TRI_PRIM,
  METALRT_FUNC_LOCAL_BOX_PRIM,
  METALRT_FUNC_CURVE_RIBBON,
  METALRT_FUNC_CURVE_RIBBON_SHADOW,
  METALRT_FUNC_CURVE_ALL,
  METALRT_FUNC_CURVE_ALL_SHADOW,
  METALRT_FUNC_POINT,
  METALRT_FUNC_POINT_SHADOW,
  METALRT_FUNC_NUM
};

enum {
  METALRT_TABLE_DEFAULT,
  METALRT_TABLE_SHADOW,
  METALRT_TABLE_LOCAL,
  METALRT_TABLE_LOCAL_PRIM,
  METALRT_TABLE_NUM
};

/* Pipeline State Object types */
enum MetalPipelineType {
  /* A kernel that can be used with all scenes, supporting all features.
   * It is slow to compile, but only needs to be compiled once and is then
   * cached for future render sessions. This allows a render to get underway
   * on the GPU quickly.
   */
  PSO_GENERIC,

  /* A intersection kernel that is very quick to specialize and results in faster intersection
   * kernel performance. It uses Metal function constants to replace several KernelData variables
   * with fixed constants.
   */
  PSO_SPECIALIZED_INTERSECT,

  /* A shading kernel that is slow to specialize, but results in faster shading kernel performance
   * rendered. It uses Metal function constants to replace several KernelData variables with fixed
   * constants and short-circuit all unused SVM node case handlers.
   */
  PSO_SPECIALIZED_SHADE,

  PSO_NUM
};

#  define METALRT_FEATURE_MASK \
    (KERNEL_FEATURE_HAIR | KERNEL_FEATURE_HAIR_THICK | KERNEL_FEATURE_POINTCLOUD | \
     KERNEL_FEATURE_OBJECT_MOTION)

const char *kernel_type_as_string(MetalPipelineType pso_type);

struct MetalKernelPipeline {

  void compile();

  int originating_device_id;

  id<MTLLibrary> mtlLibrary = nil;
  MetalPipelineType pso_type;
  string source_md5;
  size_t usage_count = 0;

  KernelData kernel_data_;
  bool use_metalrt;
  uint32_t metalrt_features = 0;

  int threads_per_threadgroup;

  DeviceKernel device_kernel;
  bool loaded = false;
  id<MTLDevice> mtlDevice = nil;
  id<MTLFunction> function = nil;
  id<MTLComputePipelineState> pipeline = nil;
  int num_threads_per_block = 0;

  bool should_use_binary_archive() const;

  string error_str;

  API_AVAILABLE(macos(11.0))
  id<MTLIntersectionFunctionTable> intersection_func_table[METALRT_TABLE_NUM] = {nil};
  id<MTLFunction> rt_intersection_function[METALRT_FUNC_NUM] = {nil};
};

/* Cache of Metal kernels for each DeviceKernel. */
namespace MetalDeviceKernels {

bool any_specialization_happening_now();
int get_loaded_kernel_count(MetalDevice const *device, MetalPipelineType pso_type);
bool should_load_kernels(MetalDevice const *device, MetalPipelineType pso_type);
bool load(MetalDevice *device, MetalPipelineType pso_type);
const MetalKernelPipeline *get_best_pipeline(const MetalDevice *device, DeviceKernel kernel);
void wait_for_all();
bool is_benchmark_warmup();

} /* namespace MetalDeviceKernels */

CCL_NAMESPACE_END

#endif /* WITH_METAL */
