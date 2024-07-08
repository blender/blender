/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/metal/util.h"
#  include "device/metal/device_impl.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/string.h"
#  include "util/time.h"

#  include <IOKit/IOKitLib.h>
#  include <pwd.h>
#  include <sys/shm.h>
#  include <time.h>

CCL_NAMESPACE_BEGIN

string MetalInfo::get_device_name(id<MTLDevice> device)
{
  string device_name = [device.name UTF8String];

  /* Append the GPU core count so we can distinguish between GPU variants in benchmarks. */
  int gpu_core_count = get_apple_gpu_core_count(device);
  device_name += string_printf(gpu_core_count ? " (GPU - %d cores)" : " (GPU)", gpu_core_count);

  return device_name;
}

int MetalInfo::get_apple_gpu_core_count(id<MTLDevice> device)
{
  int core_count = 0;
  if (@available(macos 12.0, *)) {
    io_service_t gpu_service = IOServiceGetMatchingService(
        kIOMainPortDefault, IORegistryEntryIDMatching(device.registryID));
    if (CFNumberRef numberRef = (CFNumberRef)IORegistryEntryCreateCFProperty(
            gpu_service, CFSTR("gpu-core-count"), 0, 0))
    {
      if (CFGetTypeID(numberRef) == CFNumberGetTypeID()) {
        CFNumberGetValue(numberRef, kCFNumberSInt32Type, &core_count);
      }
      CFRelease(numberRef);
    }
  }
  return core_count;
}

AppleGPUArchitecture MetalInfo::get_apple_gpu_architecture(id<MTLDevice> device)
{
  const char *device_name = [device.name UTF8String];
  if (strstr(device_name, "M1")) {
    return APPLE_M1;
  }
  else if (strstr(device_name, "M2")) {
    return get_apple_gpu_core_count(device) <= 10 ? APPLE_M2 : APPLE_M2_BIG;
  }
  else if (strstr(device_name, "M3")) {
    return APPLE_M3;
  }
  return APPLE_UNKNOWN;
}

int MetalInfo::optimal_sort_partition_elements()
{
  if (auto str = getenv("CYCLES_METAL_SORT_PARTITION_ELEMENTS")) {
    return atoi(str);
  }

  /* On M1 and M2 GPUs, we see better cache utilization if we partition the active indices before
   * sorting each partition by material. Partitioning into chunks of 65536 elements results in an
   * overall render time speedup of up to 15%. */

  return 65536;
}

vector<id<MTLDevice>> const &MetalInfo::get_usable_devices()
{
  static vector<id<MTLDevice>> usable_devices;
  static bool already_enumerated = false;

  if (already_enumerated) {
    return usable_devices;
  }

  metal_printf("Usable Metal devices:\n");
  for (id<MTLDevice> device in MTLCopyAllDevices()) {
    string device_name = get_device_name(device);
    bool usable = false;

    if (@available(macos 12.2, *)) {
      const char *device_name_char = [device.name UTF8String];
      if (!(strstr(device_name_char, "Intel") || strstr(device_name_char, "AMD")) &&
          strstr(device_name_char, "Apple"))
      {
        /* TODO: Implement a better way to identify device vendor instead of relying on name. */
        usable = true;
      }
    }

    if (usable) {
      metal_printf("- %s\n", device_name.c_str());
      [device retain];
      usable_devices.push_back(device);
    }
    else {
      metal_printf("  (skipping \"%s\")\n", device_name.c_str());
    }
  }
  if (usable_devices.empty()) {
    metal_printf("   No usable Metal devices found\n");
  }
  already_enumerated = true;

  return usable_devices;
}

id<MTLBuffer> MetalBufferPool::get_buffer(id<MTLDevice> device,
                                          id<MTLCommandBuffer> command_buffer,
                                          NSUInteger length,
                                          MTLResourceOptions options,
                                          const void *pointer,
                                          Stats &stats)
{
  id<MTLBuffer> buffer = nil;

  MTLStorageMode storageMode = MTLStorageMode((options & MTLResourceStorageModeMask) >>
                                              MTLResourceStorageModeShift);
  MTLCPUCacheMode cpuCacheMode = MTLCPUCacheMode((options & MTLResourceCPUCacheModeMask) >>
                                                 MTLResourceCPUCacheModeShift);

  {
    thread_scoped_lock lock(buffer_mutex);
    /* Find an unused buffer with matching size and storage mode. */
    for (MetalBufferListEntry &bufferEntry : temp_buffers) {
      if (bufferEntry.buffer.length == length && storageMode == bufferEntry.buffer.storageMode &&
          cpuCacheMode == bufferEntry.buffer.cpuCacheMode && bufferEntry.command_buffer == nil)
      {
        buffer = bufferEntry.buffer;
        bufferEntry.command_buffer = command_buffer;
        break;
      }
    }
    if (!buffer) {
      /* Create a new buffer and add it to the pool. Typically this pool will only grow to a
       * handful of entries. */
      buffer = [device newBufferWithLength:length options:options];
      stats.mem_alloc(buffer.allocatedSize);
      total_temp_mem_size += buffer.allocatedSize;
      temp_buffers.push_back(MetalBufferListEntry{buffer, command_buffer});
    }
  }

  /* Copy over data */
  if (pointer) {
    memcpy(buffer.contents, pointer, length);
    if (buffer.storageMode == MTLStorageModeManaged) {
      [buffer didModifyRange:NSMakeRange(0, length)];
    }
  }

  return buffer;
}

void MetalBufferPool::process_command_buffer_completion(id<MTLCommandBuffer> command_buffer)
{
  assert(command_buffer);
  thread_scoped_lock lock(buffer_mutex);
  /* Mark any temp buffers associated with command_buffer as unused. */
  for (MetalBufferListEntry &buffer_entry : temp_buffers) {
    if (buffer_entry.command_buffer == command_buffer) {
      buffer_entry.command_buffer = nil;
    }
  }
}

MetalBufferPool::~MetalBufferPool()
{
  thread_scoped_lock lock(buffer_mutex);
  /* Release all buffers that have not been recently reused */
  for (MetalBufferListEntry &buffer_entry : temp_buffers) {
    total_temp_mem_size -= buffer_entry.buffer.allocatedSize;
    [buffer_entry.buffer release];
    buffer_entry.buffer = nil;
  }
  temp_buffers.clear();
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
