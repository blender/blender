/*
 * Copyright 2011-2021 Blender Foundation
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

#include "device/kernel.h"

#include "device/graphics_interop.h"
#include "util/log.h"
#include "util/map.h"
#include "util/string.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class Device;
class device_memory;

struct KernelWorkTile;

/* Abstraction of a command queue for a device.
 * Provides API to schedule kernel execution in a specific queue with minimal possible overhead
 * from driver side.
 *
 * This class encapsulates all properties needed for commands execution. */
class DeviceQueue {
 public:
  virtual ~DeviceQueue();

  /* Number of concurrent states to process for integrator,
   * based on number of cores and/or available memory. */
  virtual int num_concurrent_states(const size_t state_size) const = 0;

  /* Number of states which keeps the device occupied with work without loosing performance.
   * The renderer will add more work (when available) when number of active paths falls below this
   * value. */
  virtual int num_concurrent_busy_states() const = 0;

  /* Initialize execution of kernels on this queue.
   *
   * Will, for example, load all data required by the kernels from Device to global or path state.
   *
   * Use this method after device synchronization has finished before enqueueing any kernels. */
  virtual void init_execution() = 0;

  /* Test if an optional device kernel is available. */
  virtual bool kernel_available(DeviceKernel kernel) const = 0;

  /* Enqueue kernel execution.
   *
   * Execute the kernel work_size times on the device.
   * Supported arguments types:
   * - int: pass pointer to the int
   * - device memory: pass pointer to device_memory.device_pointer
   * Return false if there was an error executing this or a previous kernel. */
  virtual bool enqueue(DeviceKernel kernel, const int work_size, void *args[]) = 0;

  /* Wait unit all enqueued kernels have finished execution.
   * Return false if there was an error executing any of the enqueued kernels. */
  virtual bool synchronize() = 0;

  /* Copy memory to/from device as part of the command queue, to ensure
   * operations are done in order without having to synchronize. */
  virtual void zero_to_device(device_memory &mem) = 0;
  virtual void copy_to_device(device_memory &mem) = 0;
  virtual void copy_from_device(device_memory &mem) = 0;

  /* Graphics resources interoperability.
   *
   * The interoperability comes here by the meaning that the device is capable of computing result
   * directly into an OpenGL (or other graphics library) buffer. */

  /* Create graphics interoperability context which will be taking care of mapping graphics
   * resource as a buffer writable by kernels of this device. */
  virtual unique_ptr<DeviceGraphicsInterop> graphics_interop_create()
  {
    LOG(FATAL) << "Request of GPU interop of a device which does not support it.";
    return nullptr;
  }

  /* Device this queue has been created for. */
  Device *device;

 protected:
  /* Hide construction so that allocation via `Device` API is enforced. */
  explicit DeviceQueue(Device *device);

  /* Implementations call these from the corresponding methods to generate debugging logs. */
  void debug_init_execution();
  void debug_enqueue(DeviceKernel kernel, const int work_size);
  void debug_synchronize();
  string debug_active_kernels();

  /* Combination of kernels enqueued together sync last synchronize. */
  DeviceKernelMask last_kernels_enqueued_;
  /* Time of synchronize call. */
  double last_sync_time_;
  /* Accumulated execution time for combinations of kernels launched together. */
  map<DeviceKernelMask, double> stats_kernel_time_;
};

CCL_NAMESPACE_END
