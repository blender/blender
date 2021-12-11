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

#  include <Metal/Metal.h>
#  include <string>

#  include "device/metal/device.h"
#  include "device/metal/kernel.h"
#  include "device/queue.h"

#  include "util/thread.h"

CCL_NAMESPACE_BEGIN

enum MetalGPUVendor {
  METAL_GPU_UNKNOWN = 0,
  METAL_GPU_APPLE = 1,
  METAL_GPU_AMD = 2,
  METAL_GPU_INTEL = 3,
};

/* Retains a named MTLDevice for device enumeration. */
struct MetalPlatformDevice {
  MetalPlatformDevice(id<MTLDevice> device, const string &device_name)
      : device_id(device), device_name(device_name)
  {
    [device_id retain];
  }
  ~MetalPlatformDevice()
  {
    [device_id release];
  }
  id<MTLDevice> device_id;
  string device_name;
};

/* Contains static Metal helper functions. */
struct MetalInfo {
  static bool device_version_check(id<MTLDevice> device);
  static void get_usable_devices(vector<MetalPlatformDevice> *usable_devices);
  static MetalGPUVendor get_vendor_from_device_name(string const &device_name);

  /* Platform information. */
  static bool get_num_devices(uint32_t *num_platforms);
  static uint32_t get_num_devices();

  static bool get_device_name(id<MTLDevice> device_id, string *device_name);
  static string get_device_name(id<MTLDevice> device_id);
};

/* Pool of MTLBuffers whose lifetime is linked to a single MTLCommandBuffer */
class MetalBufferPool {
  struct MetalBufferListEntry {
    MetalBufferListEntry(id<MTLBuffer> buffer, id<MTLCommandBuffer> command_buffer)
        : buffer(buffer), command_buffer(command_buffer)
    {
    }

    MetalBufferListEntry() = delete;

    id<MTLBuffer> buffer;
    id<MTLCommandBuffer> command_buffer;
  };
  std::vector<MetalBufferListEntry> buffer_free_list;
  std::vector<MetalBufferListEntry> buffer_in_use_list;
  thread_mutex buffer_mutex;
  size_t total_temp_mem_size = 0;

 public:
  MetalBufferPool() = default;
  ~MetalBufferPool();

  id<MTLBuffer> get_buffer(id<MTLDevice> device,
                           id<MTLCommandBuffer> command_buffer,
                           NSUInteger length,
                           MTLResourceOptions options,
                           const void *pointer,
                           Stats &stats);
  void process_command_buffer_completion(id<MTLCommandBuffer> command_buffer);
};

CCL_NAMESPACE_END

#endif /* WITH_METAL */
