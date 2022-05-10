/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#pragma once

#ifdef WITH_METAL

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/metal/util.h"
#  include "kernel/device/metal/globals.h"

#  define metal_printf VLOG(4) << string_printf

CCL_NAMESPACE_BEGIN

class MetalDevice;

/* Base class for Metal queues. */
class MetalDeviceQueue : public DeviceQueue {
 public:
  MetalDeviceQueue(MetalDevice *device);
  ~MetalDeviceQueue();

  virtual int num_concurrent_states(const size_t) const override;
  virtual int num_concurrent_busy_states() const override;

  virtual void init_execution() override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int work_size,
                       DeviceKernelArguments const &args) override;

  virtual bool synchronize() override;

  virtual void zero_to_device(device_memory &mem) override;
  virtual void copy_to_device(device_memory &mem) override;
  virtual void copy_from_device(device_memory &mem) override;

 protected:
  void prepare_resources(DeviceKernel kernel);

  id<MTLComputeCommandEncoder> get_compute_encoder(DeviceKernel kernel);
  id<MTLBlitCommandEncoder> get_blit_encoder();

  MetalDevice *metal_device;
  MetalBufferPool temp_buffer_pool;

  API_AVAILABLE(macos(11.0), ios(14.0))
  MTLCommandBufferDescriptor *command_buffer_desc = nullptr;
  id<MTLDevice> mtlDevice = nil;
  id<MTLCommandQueue> mtlCommandQueue = nil;
  id<MTLCommandBuffer> mtlCommandBuffer = nil;
  id<MTLComputeCommandEncoder> mtlComputeEncoder = nil;
  id<MTLBlitCommandEncoder> mtlBlitEncoder = nil;
  API_AVAILABLE(macos(10.14), ios(14.0))
  id<MTLSharedEvent> shared_event = nil;
  API_AVAILABLE(macos(10.14), ios(14.0))
  MTLSharedEventListener *shared_event_listener = nil;

  dispatch_queue_t event_queue;
  dispatch_semaphore_t wait_semaphore;

  struct CopyBack {
    void *host_pointer;
    void *gpu_mem;
    uint64_t size;
  };
  std::vector<CopyBack> copy_back_mem;

  uint64_t shared_event_id;
  uint64_t command_buffers_submitted = 0;
  uint64_t command_buffers_completed = 0;
  Stats &stats;

  void close_compute_encoder();
  void close_blit_encoder();
};

CCL_NAMESPACE_END

#endif /* WITH_METAL */
