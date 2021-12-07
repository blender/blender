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

#ifdef WITH_METAL

#  include "device/metal/util.h"
#  include "device/metal/device_impl.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/string.h"
#  include "util/time.h"

#  include <pwd.h>
#  include <sys/shm.h>
#  include <time.h>

CCL_NAMESPACE_BEGIN

MetalGPUVendor MetalInfo::get_vendor_from_device_name(string const &device_name)
{
  if (device_name.find("Intel") != string::npos) {
    return METAL_GPU_INTEL;
  }
  else if (device_name.find("AMD") != string::npos) {
    return METAL_GPU_AMD;
  }
  else if (device_name.find("Apple") != string::npos) {
    return METAL_GPU_APPLE;
  }
  return METAL_GPU_UNKNOWN;
}

bool MetalInfo::device_version_check(id<MTLDevice> device)
{
  if (@available(macos 12.0, *)) {
    MetalGPUVendor vendor = get_vendor_from_device_name([[device name] UTF8String]);

    static const char *forceIntelStr = getenv("CYCLES_METAL_FORCE_INTEL");
    bool forceIntel = forceIntelStr ? (atoi(forceIntelStr) != 0) : false;

    if (forceIntel) {
      /* return false for non-Intel GPUs to force selection of Intel */
      if (vendor == METAL_GPU_INTEL) {
        return true;
      }
    }
    else {
      switch (vendor) {
        case METAL_GPU_INTEL:
          /* isLowPower only returns true on machines that have an AMD GPU also
           * For Intel only machines - isLowPower will return false
           */
          if (getenv("CYCLES_METAL_ALLOW_LOW_POWER_GPUS") || !device.isLowPower) {
            return true;
          }
          return false;
        case METAL_GPU_APPLE:
        case METAL_GPU_AMD:
          return true;
        default:
          return false;
      }
    }
  }

  return false;
}

void MetalInfo::get_usable_devices(vector<MetalPlatformDevice> *usable_devices)
{
  static bool first_time = true;
#  define FIRST_VLOG(severity) \
    if (first_time) \
    VLOG(severity)

  usable_devices->clear();

  NSArray<id<MTLDevice>> *allDevices = MTLCopyAllDevices();
  for (id<MTLDevice> device in allDevices) {
    string device_name;
    if (!get_device_name(device, &device_name)) {
      FIRST_VLOG(2) << "Failed to get device name, ignoring.";
      continue;
    }

    static const char *forceIntelStr = getenv("CYCLES_METAL_FORCE_INTEL");
    bool forceIntel = forceIntelStr ? (atoi(forceIntelStr) != 0) : false;
    if (forceIntel && device_name.find("Intel") == string::npos) {
      FIRST_VLOG(2) << "CYCLES_METAL_FORCE_INTEL causing non-Intel device " << device_name
                    << " to be ignored.";
      continue;
    }

    if (!device_version_check(device)) {
      FIRST_VLOG(2) << "Ignoring device " << device_name << " due to too old compiler version.";
      continue;
    }
    FIRST_VLOG(2) << "Adding new device " << device_name << ".";
    string hardware_id;
    usable_devices->push_back(MetalPlatformDevice(device, device_name));
  }
  first_time = false;
}

bool MetalInfo::get_num_devices(uint32_t *num_devices)
{
  *num_devices = MTLCopyAllDevices().count;
  return true;
}

uint32_t MetalInfo::get_num_devices()
{
  uint32_t num_devices;
  if (!get_num_devices(&num_devices)) {
    return 0;
  }
  return num_devices;
}

bool MetalInfo::get_device_name(id<MTLDevice> device, string *platform_name)
{
  *platform_name = [device.name UTF8String];
  return true;
}

string MetalInfo::get_device_name(id<MTLDevice> device)
{
  string platform_name;
  if (!get_device_name(device, &platform_name)) {
    return "";
  }
  return platform_name;
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
