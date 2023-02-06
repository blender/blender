/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#pragma once

#ifdef WITH_METAL

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/metal/util.h"
#  include "kernel/device/metal/globals.h"

CCL_NAMESPACE_BEGIN

class MetalDevice;

/* Base class for Metal queues. */
class MetalDeviceQueue : public DeviceQueue {
 public:
  MetalDeviceQueue(MetalDevice *device);
  ~MetalDeviceQueue();

  virtual int num_concurrent_states(const size_t) const override;
  virtual int num_concurrent_busy_states(const size_t) const override;
  virtual int num_sort_partition_elements() const override;
  virtual bool supports_local_atomic_sort() const override;

  virtual void init_execution() override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int work_size,
                       DeviceKernelArguments const &args) override;

  virtual bool synchronize() override;

  virtual void zero_to_device(device_memory &mem) override;
  virtual void copy_to_device(device_memory &mem) override;
  virtual void copy_from_device(device_memory &mem) override;

 protected:
  void setup_capture();
  void update_capture(DeviceKernel kernel);
  void begin_capture();
  void end_capture();
  void prepare_resources(DeviceKernel kernel);

  id<MTLComputeCommandEncoder> get_compute_encoder(DeviceKernel kernel);
  id<MTLBlitCommandEncoder> get_blit_encoder();

  MetalDevice *metal_device_;
  MetalBufferPool temp_buffer_pool_;

  API_AVAILABLE(macos(11.0), ios(14.0))
  MTLCommandBufferDescriptor *command_buffer_desc_ = nullptr;
  id<MTLDevice> mtlDevice_ = nil;
  id<MTLCommandQueue> mtlCommandQueue_ = nil;
  id<MTLCommandBuffer> mtlCommandBuffer_ = nil;
  id<MTLComputeCommandEncoder> mtlComputeEncoder_ = nil;
  id<MTLBlitCommandEncoder> mtlBlitEncoder_ = nil;
  API_AVAILABLE(macos(10.14), ios(14.0))
  id<MTLSharedEvent> shared_event_ = nil;
  API_AVAILABLE(macos(10.14), ios(14.0))
  MTLSharedEventListener *shared_event_listener_ = nil;

  dispatch_queue_t event_queue_;
  dispatch_semaphore_t wait_semaphore_;

  struct CopyBack {
    void *host_pointer;
    void *gpu_mem;
    uint64_t size;
  };
  std::vector<CopyBack> copy_back_mem_;

  uint64_t shared_event_id_;
  uint64_t command_buffers_submitted_ = 0;
  uint64_t command_buffers_completed_ = 0;
  Stats &stats_;

  void close_compute_encoder();
  void close_blit_encoder();

  bool verbose_tracing_ = false;
  bool label_command_encoders_ = false;

  /* Per-kernel profiling (see CYCLES_METAL_PROFILING). */

  struct TimingData {
    DeviceKernel kernel;
    int work_size;
    uint64_t timing_id;
  };
  std::vector<TimingData> command_encoder_labels_;
  API_AVAILABLE(macos(10.14), ios(14.0))
  id<MTLSharedEvent> timing_shared_event_ = nil;
  uint64_t timing_shared_event_id_;
  uint64_t command_buffer_start_timing_id_;

  struct TimingStats {
    double total_time = 0.0;
    uint64_t total_work_size = 0;
    uint64_t num_dispatches = 0;
  };
  TimingStats timing_stats_[DEVICE_KERNEL_NUM];
  double last_completion_time_ = 0.0;

  /* .gputrace capture (see CYCLES_DEBUG_METAL_CAPTURE_...). */

  id<MTLCaptureScope> mtlCaptureScope_ = nil;
  DeviceKernel capture_kernel_;
  int capture_dispatch_counter_ = 0;
  bool capture_samples_ = false;
  int capture_reset_counter_ = 0;
  bool is_capturing_ = false;
  bool is_capturing_to_disk_ = false;
  bool has_captured_to_disk_ = false;
};

CCL_NAMESPACE_END

#endif /* WITH_METAL */
