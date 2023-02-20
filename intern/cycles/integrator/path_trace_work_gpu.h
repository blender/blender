/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/integrator/state.h"

#include "device/graphics_interop.h"
#include "device/memory.h"
#include "device/queue.h"

#include "integrator/path_trace_work.h"
#include "integrator/work_tile_scheduler.h"

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

struct KernelWorkTile;

/* Implementation of PathTraceWork which schedules work to the device in tiles which are sized
 * to match device queue's number of path states.
 * This implementation suits best devices which have a lot of integrator states, such as GPU. */
class PathTraceWorkGPU : public PathTraceWork {
 public:
  PathTraceWorkGPU(Device *device,
                   Film *film,
                   DeviceScene *device_scene,
                   bool *cancel_requested_flag);

  virtual void alloc_work_memory() override;
  virtual void init_execution() override;

  virtual void render_samples(RenderStatistics &statistics,
                              int start_sample,
                              int samples_num,
                              int sample_offset) override;

  virtual void copy_to_display(PathTraceDisplay *display,
                               PassMode pass_mode,
                               int num_samples) override;
  virtual void destroy_gpu_resources(PathTraceDisplay *display) override;

  virtual bool copy_render_buffers_from_device() override;
  virtual bool copy_render_buffers_to_device() override;
  virtual bool zero_render_buffers() override;

  virtual int adaptive_sampling_converge_filter_count_active(float threshold, bool reset) override;
  virtual void cryptomatte_postproces() override;

 protected:
  void alloc_integrator_soa();
  void alloc_integrator_queue();
  void alloc_integrator_sorting();
  void alloc_integrator_path_split();

  /* Returns DEVICE_KERNEL_NUM if there are no scheduled kernels. */
  DeviceKernel get_most_queued_kernel() const;

  void enqueue_reset();

  bool enqueue_work_tiles(bool &finished);
  void enqueue_work_tiles(DeviceKernel kernel,
                          const KernelWorkTile work_tiles[],
                          const int num_work_tiles,
                          const int num_active_paths,
                          const int num_predicted_splits);

  bool enqueue_path_iteration();
  void enqueue_path_iteration(DeviceKernel kernel, const int num_paths_limit = INT_MAX);

  void compute_queued_paths(DeviceKernel kernel, DeviceKernel queued_kernel);
  void compute_sorted_queued_paths(DeviceKernel queued_kernel, const int num_paths_limit);

  void compact_main_paths(const int num_active_paths);
  void compact_shadow_paths();
  void compact_paths(const int num_active_paths,
                     const int max_active_path_index,
                     DeviceKernel terminated_paths_kernel,
                     DeviceKernel compact_paths_kernel,
                     DeviceKernel compact_kernel);

  int num_active_main_paths_paths();

  /* Check whether graphics interop can be used for the PathTraceDisplay update. */
  bool should_use_graphics_interop();

  /* Naive implementation of the `copy_to_display()` which performs film conversion on the
   * device, then copies pixels to the host and pushes them to the `display`. */
  void copy_to_display_naive(PathTraceDisplay *display, PassMode pass_mode, int num_samples);

  /* Implementation of `copy_to_display()` which uses driver's OpenGL/GPU interoperability
   * functionality, avoiding copy of pixels to the host. */
  bool copy_to_display_interop(PathTraceDisplay *display, PassMode pass_mode, int num_samples);

  /* Synchronously run film conversion kernel and store display result in the given destination. */
  void get_render_tile_film_pixels(const PassAccessor::Destination &destination,
                                   PassMode pass_mode,
                                   int num_samples);

  int adaptive_sampling_convergence_check_count_active(float threshold, bool reset);
  void enqueue_adaptive_sampling_filter_x();
  void enqueue_adaptive_sampling_filter_y();

  bool has_shadow_catcher() const;

  /* Count how many currently scheduled paths can still split. */
  int shadow_catcher_count_possible_splits();

  /* Kernel properties. */
  bool kernel_uses_sorting(DeviceKernel kernel);
  bool kernel_creates_shadow_paths(DeviceKernel kernel);
  bool kernel_creates_ao_paths(DeviceKernel kernel);
  bool kernel_is_shadow_path(DeviceKernel kernel);
  int kernel_max_active_main_path_index(DeviceKernel kernel);

  /* Integrator queue. */
  unique_ptr<DeviceQueue> queue_;

  /* Scheduler which gives work to path tracing threads. */
  WorkTileScheduler work_tile_scheduler_;

  /* Integrate state for paths. */
  IntegratorStateGPU integrator_state_gpu_;
  /* SoA arrays for integrator state. */
  vector<unique_ptr<device_memory>> integrator_state_soa_;
  uint integrator_state_soa_kernel_features_;
  int integrator_state_soa_volume_stack_size_ = 0;
  /* Keep track of number of queued kernels. */
  device_vector<IntegratorQueueCounter> integrator_queue_counter_;
  /* Shader sorting. */
  device_vector<int> integrator_shader_sort_counter_;
  device_vector<int> integrator_shader_raytrace_sort_counter_;
  device_vector<int> integrator_shader_mnee_sort_counter_;
  device_vector<int> integrator_shader_sort_prefix_sum_;
  device_vector<int> integrator_shader_sort_partition_key_offsets_;
  /* Path split. */
  device_vector<int> integrator_next_main_path_index_;
  device_vector<int> integrator_next_shadow_path_index_;

  /* Temporary buffer to get an array of queued path for a particular kernel. */
  device_vector<int> queued_paths_;
  device_vector<int> num_queued_paths_;

  /* Temporary buffer for passing work tiles to kernel. */
  device_vector<KernelWorkTile> work_tiles_;

  /* Temporary buffer used by the copy_to_display() whenever graphics interoperability is not
   * available. Is allocated on-demand. */
  device_vector<half4> display_rgba_half_;

  unique_ptr<DeviceGraphicsInterop> device_graphics_interop_;

  /* Cached result of device->should_use_graphics_interop(). */
  bool interop_use_checked_ = false;
  bool interop_use_ = false;

  /* Number of partitions to sort state indices into prior to material sort. */
  int num_sort_partitions_;

  /* Maximum number of concurrent integrator states. */
  int max_num_paths_;

  /* Minimum number of paths which keeps the device bust. If the actual number of paths falls below
   * this value more work will be scheduled. */
  int min_num_active_main_paths_;

  /* Maximum path index, effective number of paths used may be smaller than
   * the size of the integrator_state_ buffer so can avoid iterating over the
   * full buffer. */
  int max_active_main_path_index_;
};

CCL_NAMESPACE_END
