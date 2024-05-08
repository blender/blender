/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/metal/device.h"
#  include "device/metal/device_impl.h"
#  include "integrator/denoiser_oidn_gpu.h"

#endif

#include "util/debug.h"
#include "util/set.h"
#include "util/system.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_METAL

Device *device_metal_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return new MetalDevice(info, stats, profiler);
}

bool device_metal_init()
{
  return true;
}

void device_metal_info(vector<DeviceInfo> &devices)
{
  auto usable_devices = MetalInfo::get_usable_devices();
  /* Devices are numbered consecutively across platforms. */
  set<string> unique_ids;
  int device_index = 0;
  for (id<MTLDevice> &device : usable_devices) {
    /* Compute unique ID for persistent user preferences. */
    string device_name = MetalInfo::get_device_name(device);

    string id = string("METAL_") + device_name;

    /* Hardware ID might not be unique, add device number in that case. */
    if (unique_ids.find(id) != unique_ids.end()) {
      id += string_printf("_ID_%d", device_index);
    }
    unique_ids.insert(id);

    /* Create DeviceInfo. */
    DeviceInfo info;
    info.type = DEVICE_METAL;
    info.description = string_remove_trademark(string(device_name));

    info.num = device_index;
    /* We don't know if it's used for display, but assume it is. */
    info.display_device = true;
    info.denoisers = DENOISER_NONE;
    info.id = id;
#  if defined(WITH_OPENIMAGEDENOISE)
#    if OIDN_VERSION >= 20300
    if (oidnIsMetalDeviceSupported(device)) {
#    else
    if (OIDNDenoiserGPU::is_device_supported(info)) {
#    endif
      info.denoisers |= DENOISER_OPENIMAGEDENOISE;
    }
#  endif

    MetalGPUVendor vendor = MetalInfo::get_device_vendor(device);

    info.has_nanovdb = vendor == METAL_GPU_APPLE;
    info.has_light_tree = vendor != METAL_GPU_AMD;
    info.has_mnee = vendor != METAL_GPU_AMD;

    info.use_hardware_raytracing = false;

    /* MetalRT now uses features exposed in Xcode versions corresponding to macOS 14+, so don't
     * expose it in builds from older Xcode versions. */
#  if defined(MAC_OS_VERSION_14_0)
    if (vendor != METAL_GPU_INTEL) {
      if (@available(macos 14.0, *)) {
        info.use_hardware_raytracing = device.supportsRaytracing;

        /* Use hardware raytracing for faster rendering on architectures that support it. */
        info.use_metalrt_by_default = (MetalInfo::get_apple_gpu_architecture(device) >= APPLE_M3);
      }
    }
#  endif

    devices.push_back(info);
    device_index++;

    VLOG_INFO << "Added device \"" << info.description << "\" with id \"" << info.id << "\".";

    if (info.denoisers & DENOISER_OPENIMAGEDENOISE)
      VLOG_INFO << "Device with id \"" << info.id << "\" supports "
                << denoiserTypeToHumanReadable(DENOISER_OPENIMAGEDENOISE) << ".";
  }
}

string device_metal_capabilities()
{
  string result = "";
  auto allDevices = MTLCopyAllDevices();
  uint32_t num_devices = (uint32_t)allDevices.count;
  if (num_devices == 0) {
    return "No Metal devices found\n";
  }
  result += string_printf("Number of devices: %u\n", num_devices);

  for (id<MTLDevice> device in allDevices) {
    string device_name = MetalInfo::get_device_name(device);
    result += string_printf("\t\tDevice: %s\n", device_name.c_str());
  }

  return result;
}

#else

Device *device_metal_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return nullptr;
}

bool device_metal_init()
{
  return false;
}

void device_metal_info(vector<DeviceInfo> &devices) {}

string device_metal_capabilities()
{
  return "";
}

#endif

CCL_NAMESPACE_END
