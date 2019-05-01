/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __DEVICE_SPLIT_KERNEL_H__
#define __DEVICE_SPLIT_KERNEL_H__

#include "device/device.h"
#include "render/buffers.h"

CCL_NAMESPACE_BEGIN

/* When allocate global memory in chunks. We may not be able to
 * allocate exactly "CL_DEVICE_MAX_MEM_ALLOC_SIZE" bytes in chunks;
 * Since some bytes may be needed for aligning chunks of memory;
 * This is the amount of memory that we dedicate for that purpose.
 */
#define DATA_ALLOCATION_MEM_FACTOR 5000000  // 5MB

/* Types used for split kernel */

class KernelDimensions {
 public:
  size_t global_size[2];
  size_t local_size[2];

  KernelDimensions(size_t global_size_[2], size_t local_size_[2])
  {
    memcpy(global_size, global_size_, sizeof(global_size));
    memcpy(local_size, local_size_, sizeof(local_size));
  }
};

class SplitKernelFunction {
 public:
  virtual ~SplitKernelFunction()
  {
  }

  /* enqueue the kernel, returns false if there is an error */
  virtual bool enqueue(const KernelDimensions &dim, device_memory &kg, device_memory &data) = 0;
};

class DeviceSplitKernel {
 private:
  Device *device;

  SplitKernelFunction *kernel_path_init;
  SplitKernelFunction *kernel_scene_intersect;
  SplitKernelFunction *kernel_lamp_emission;
  SplitKernelFunction *kernel_do_volume;
  SplitKernelFunction *kernel_queue_enqueue;
  SplitKernelFunction *kernel_indirect_background;
  SplitKernelFunction *kernel_shader_setup;
  SplitKernelFunction *kernel_shader_sort;
  SplitKernelFunction *kernel_shader_eval;
  SplitKernelFunction *kernel_holdout_emission_blurring_pathtermination_ao;
  SplitKernelFunction *kernel_subsurface_scatter;
  SplitKernelFunction *kernel_direct_lighting;
  SplitKernelFunction *kernel_shadow_blocked_ao;
  SplitKernelFunction *kernel_shadow_blocked_dl;
  SplitKernelFunction *kernel_enqueue_inactive;
  SplitKernelFunction *kernel_next_iteration_setup;
  SplitKernelFunction *kernel_indirect_subsurface;
  SplitKernelFunction *kernel_buffer_update;

  /* Global memory variables [porting]; These memory is used for
   * co-operation between different kernels; Data written by one
   * kernel will be available to another kernel via this global
   * memory.
   */
  device_only_memory<uchar> split_data;
  device_vector<uchar> ray_state;
  device_only_memory<int>
      queue_index; /* Array of size num_queues that tracks the size of each queue. */

  /* Flag to make sceneintersect and lampemission kernel use queues. */
  device_only_memory<char> use_queues_flag;

  /* Approximate time it takes to complete one sample */
  double avg_time_per_sample;

  /* Work pool with respect to each work group. */
  device_only_memory<unsigned int> work_pool_wgs;

  /* Cached kernel-dependent data, initialized once. */
  bool kernel_data_initialized;
  size_t local_size[2];
  size_t global_size[2];

 public:
  explicit DeviceSplitKernel(Device *device);
  virtual ~DeviceSplitKernel();

  bool load_kernels(const DeviceRequestedFeatures &requested_features);
  bool path_trace(DeviceTask *task,
                  RenderTile &rtile,
                  device_memory &kgbuffer,
                  device_memory &kernel_data);

  virtual uint64_t state_buffer_size(device_memory &kg,
                                     device_memory &data,
                                     size_t num_threads) = 0;
  size_t max_elements_for_max_buffer_size(device_memory &kg,
                                          device_memory &data,
                                          uint64_t max_buffer_size);

  virtual bool enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                              RenderTile &rtile,
                                              int num_global_elements,
                                              device_memory &kernel_globals,
                                              device_memory &kernel_data_,
                                              device_memory &split_data,
                                              device_memory &ray_state,
                                              device_memory &queue_index,
                                              device_memory &use_queues_flag,
                                              device_memory &work_pool_wgs) = 0;

  virtual SplitKernelFunction *get_split_kernel_function(const string &kernel_name,
                                                         const DeviceRequestedFeatures &) = 0;
  virtual int2 split_kernel_local_size() = 0;
  virtual int2 split_kernel_global_size(device_memory &kg,
                                        device_memory &data,
                                        DeviceTask *task) = 0;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_SPLIT_KERNEL_H__ */
