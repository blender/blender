/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

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
  if (get_device_vendor(device) == METAL_GPU_APPLE) {
    /* Append the GPU core count so we can distinguish between GPU variants in benchmarks. */
    int gpu_core_count = get_apple_gpu_core_count(device);
    device_name += string_printf(gpu_core_count ? " (GPU - %d cores)" : " (GPU)", gpu_core_count);
  }
  return device_name;
}

int MetalInfo::get_apple_gpu_core_count(id<MTLDevice> device)
{
  int core_count = 0;
  if (@available(macos 12.0, *)) {
    io_service_t gpu_service = IOServiceGetMatchingService(
        kIOMainPortDefault, IORegistryEntryIDMatching(device.registryID));
    if (CFNumberRef numberRef = (CFNumberRef)IORegistryEntryCreateCFProperty(
            gpu_service, CFSTR("gpu-core-count"), 0, 0)) {
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
  return APPLE_UNKNOWN;
}

MetalGPUVendor MetalInfo::get_device_vendor(id<MTLDevice> device)
{
  const char *device_name = [device.name UTF8String];
  if (strstr(device_name, "Intel")) {
    return METAL_GPU_INTEL;
  }
  else if (strstr(device_name, "AMD")) {
    /* Setting this env var hides AMD devices thus exposing any integrated Intel devices. */
    if (auto str = getenv("CYCLES_METAL_FORCE_INTEL")) {
      if (atoi(str)) {
        return METAL_GPU_UNKNOWN;
      }
    }
    return METAL_GPU_AMD;
  }
  else if (strstr(device_name, "Apple")) {
    return METAL_GPU_APPLE;
  }
  return METAL_GPU_UNKNOWN;
}

int MetalInfo::optimal_sort_partition_elements(id<MTLDevice> device)
{
  if (auto str = getenv("CYCLES_METAL_SORT_PARTITION_ELEMENTS")) {
    return atoi(str);
  }

  /* On M1 and M2 GPUs, we see better cache utilization if we partition the active indices before
   * sorting each partition by material. Partitioning into chunks of 65536 elements results in an
   * overall render time speedup of up to 15%. */
  if (get_device_vendor(device) == METAL_GPU_APPLE) {
    return 65536;
  }
  return 0;
}

vector<id<MTLDevice>> const &MetalInfo::get_usable_devices()
{
  static vector<id<MTLDevice>> usable_devices;
  static bool already_enumerated = false;

  if (already_enumerated) {
    return usable_devices;
  }

  /* If the system has both an AMD GPU (discrete) and an Intel one (integrated), prefer the AMD
   * one. This can be overridden with CYCLES_METAL_FORCE_INTEL. */
  bool has_usable_amd_gpu = false;
  if (@available(macos 12.3, *)) {
    for (id<MTLDevice> device in MTLCopyAllDevices()) {
      has_usable_amd_gpu |= (get_device_vendor(device) == METAL_GPU_AMD);
    }
  }

  metal_printf("Usable Metal devices:\n");
  for (id<MTLDevice> device in MTLCopyAllDevices()) {
    string device_name = get_device_name(device);
    MetalGPUVendor vendor = get_device_vendor(device);
    bool usable = false;

    if (@available(macos 12.2, *)) {
      usable |= (vendor == METAL_GPU_APPLE);
    }

    if (@available(macos 12.3, *)) {
      usable |= (vendor == METAL_GPU_AMD);
    }

#  if defined(MAC_OS_VERSION_13_0)
    if (!has_usable_amd_gpu) {
      if (@available(macos 13.0, *)) {
        usable |= (vendor == METAL_GPU_INTEL);
      }
    }
#  endif

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
  id<MTLBuffer> buffer;

  MTLStorageMode storageMode = MTLStorageMode((options & MTLResourceStorageModeMask) >>
                                              MTLResourceStorageModeShift);
  MTLCPUCacheMode cpuCacheMode = MTLCPUCacheMode((options & MTLResourceCPUCacheModeMask) >>
                                                 MTLResourceCPUCacheModeShift);

  buffer_mutex.lock();
  for (auto entry = buffer_free_list.begin(); entry != buffer_free_list.end(); entry++) {
    MetalBufferListEntry bufferEntry = *entry;

    /* Check if buffer matches size and storage mode and is old enough to reuse */
    if (bufferEntry.buffer.length == length && storageMode == bufferEntry.buffer.storageMode &&
        cpuCacheMode == bufferEntry.buffer.cpuCacheMode) {
      buffer = bufferEntry.buffer;
      buffer_free_list.erase(entry);
      bufferEntry.command_buffer = command_buffer;
      buffer_in_use_list.push_back(bufferEntry);
      buffer_mutex.unlock();

      /* Copy over data */
      if (pointer) {
        memcpy(buffer.contents, pointer, length);
        if (bufferEntry.buffer.storageMode == MTLStorageModeManaged) {
          [buffer didModifyRange:NSMakeRange(0, length)];
        }
      }

      return buffer;
    }
  }
  // NSLog(@"Creating buffer of length %lu (%lu)", length, frameCount);
  if (pointer) {
    buffer = [device newBufferWithBytes:pointer length:length options:options];
  }
  else {
    buffer = [device newBufferWithLength:length options:options];
  }

  MetalBufferListEntry buffer_entry(buffer, command_buffer);

  stats.mem_alloc(buffer.allocatedSize);

  total_temp_mem_size += buffer.allocatedSize;
  buffer_in_use_list.push_back(buffer_entry);
  buffer_mutex.unlock();

  return buffer;
}

void MetalBufferPool::process_command_buffer_completion(id<MTLCommandBuffer> command_buffer)
{
  assert(command_buffer);
  thread_scoped_lock lock(buffer_mutex);
  /* Release all buffers that have not been recently reused back into the free pool */
  for (auto entry = buffer_in_use_list.begin(); entry != buffer_in_use_list.end();) {
    MetalBufferListEntry buffer_entry = *entry;
    if (buffer_entry.command_buffer == command_buffer) {
      entry = buffer_in_use_list.erase(entry);
      buffer_entry.command_buffer = nil;
      buffer_free_list.push_back(buffer_entry);
    }
    else {
      entry++;
    }
  }
}

MetalBufferPool::~MetalBufferPool()
{
  thread_scoped_lock lock(buffer_mutex);
  /* Release all buffers that have not been recently reused */
  for (auto entry = buffer_free_list.begin(); entry != buffer_free_list.end();) {
    MetalBufferListEntry buffer_entry = *entry;

    id<MTLBuffer> buffer = buffer_entry.buffer;
    // NSLog(@"Releasing buffer of length %lu (%lu) (%lu outstanding)", buffer.length, frameCount,
    // bufferFreeList.size());
    total_temp_mem_size -= buffer.allocatedSize;
    [buffer release];
    entry = buffer_free_list.erase(entry);
  }
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
