/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/device.h"
#include "blender/session.h"
#include "blender/util.h"

#include "util/foreach.h"

CCL_NAMESPACE_BEGIN

enum ComputeDevice {
  COMPUTE_DEVICE_CPU = 0,
  COMPUTE_DEVICE_CUDA = 1,
  COMPUTE_DEVICE_OPTIX = 3,
  COMPUTE_DEVICE_HIP = 4,
  COMPUTE_DEVICE_METAL = 5,
  COMPUTE_DEVICE_ONEAPI = 6,

  COMPUTE_DEVICE_NUM
};

int blender_device_threads(BL::Scene &b_scene)
{
  BL::RenderSettings b_r = b_scene.render();

  if (b_r.threads_mode() == BL::RenderSettings::threads_mode_FIXED) {
    return b_r.threads();
  }
  else {
    return 0;
  }
}

void static adjust_device_info_from_preferences(DeviceInfo &info, PointerRNA cpreferences)
{
  if (!get_boolean(cpreferences, "peer_memory")) {
    info.has_peer_memory = false;
  }

  if (info.type == DEVICE_METAL) {
    MetalRTSetting use_metalrt = (MetalRTSetting)get_enum(
        cpreferences, "metalrt", METALRT_NUM_SETTINGS, METALRT_AUTO);

    info.use_hardware_raytracing = info.use_metalrt_by_default;
    if (use_metalrt == METALRT_OFF) {
      info.use_hardware_raytracing = false;
    }
    else if (use_metalrt == METALRT_ON) {
      info.use_hardware_raytracing = true;
    }
  }

  if (info.type == DEVICE_ONEAPI && !get_boolean(cpreferences, "use_oneapirt")) {
    info.use_hardware_raytracing = false;
  }

  if (info.type == DEVICE_HIP && !get_boolean(cpreferences, "use_hiprt")) {
    info.use_hardware_raytracing = false;
  }
}

void static adjust_device_info(DeviceInfo &device, PointerRNA cpreferences, bool preview)
{
  adjust_device_info_from_preferences(device, cpreferences);
  foreach (DeviceInfo &info, device.multi_devices) {
    adjust_device_info_from_preferences(info, cpreferences);

    /* There is an accumulative logic here, because Multi-devices are supported only for
     * the same backend + CPU in Blender right now, and both oneAPI and Metal have a
     * global boolean backend setting for enabling/disabling Hardware Ray Tracing,
     * so all sub-devices in the multi-device should enable (or disable) Hardware Ray Tracing
     * simultaneously (and CPU device is expected to ignore `use_hardware_raytracing` setting). */
    device.use_hardware_raytracing |= info.use_hardware_raytracing;
  }

  if (preview) {
    /* Disable specialization for preview renders. */
    device.kernel_optimization_level = KERNEL_OPTIMIZATION_LEVEL_OFF;
  }
  else {
    device.kernel_optimization_level = (KernelOptimizationLevel)get_enum(
        cpreferences,
        "kernel_optimization_level",
        KERNEL_OPTIMIZATION_NUM_LEVELS,
        KERNEL_OPTIMIZATION_LEVEL_FULL);
  }
}

DeviceInfo blender_device_info(BL::Preferences &b_preferences,
                               BL::Scene &b_scene,
                               bool background,
                               bool preview,
                               DeviceInfo &preferences_device)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  /* Find cycles preferences. */
  PointerRNA cpreferences;
  for (BL::Addon &b_addon : b_preferences.addons) {
    if (b_addon.module() == "cycles") {
      cpreferences = b_addon.preferences().ptr;
      break;
    }
  }

  /* Default to CPU device. */
  DeviceInfo cpu_device = Device::available_devices(DEVICE_MASK_CPU).front();

  /* Device, which is choosen in the Blender Preferences. */
  preferences_device = DeviceInfo();

  /* Test if we are using GPU devices. */
  ComputeDevice compute_device = (ComputeDevice)get_enum(
      cpreferences, "compute_device_type", COMPUTE_DEVICE_NUM, COMPUTE_DEVICE_CPU);

  if (compute_device != COMPUTE_DEVICE_CPU) {
    /* Query GPU devices with matching types. */
    uint mask = DEVICE_MASK_CPU;
    if (compute_device == COMPUTE_DEVICE_CUDA) {
      mask |= DEVICE_MASK_CUDA;
    }
    else if (compute_device == COMPUTE_DEVICE_OPTIX) {
      mask |= DEVICE_MASK_OPTIX;
    }
    else if (compute_device == COMPUTE_DEVICE_HIP) {
      mask |= DEVICE_MASK_HIP;
    }
    else if (compute_device == COMPUTE_DEVICE_METAL) {
      mask |= DEVICE_MASK_METAL;
    }
    else if (compute_device == COMPUTE_DEVICE_ONEAPI) {
      mask |= DEVICE_MASK_ONEAPI;
    }
    vector<DeviceInfo> devices = Device::available_devices(mask);

    /* Match device preferences and available devices. */
    vector<DeviceInfo> used_devices;
    RNA_BEGIN (&cpreferences, device, "devices") {
      if (get_boolean(device, "use")) {
        string id = get_string(device, "id");
        foreach (DeviceInfo &info, devices) {
          if (info.id == id) {
            used_devices.push_back(info);
            break;
          }
        }
      }
    }
    RNA_END;

    if (!used_devices.empty()) {
      int threads = blender_device_threads(b_scene);
      preferences_device = Device::get_multi_device(used_devices, threads, background);
    }
  }
  else {
    preferences_device = cpu_device;
  }
  adjust_device_info(preferences_device, cpreferences, preview);
  adjust_device_info(cpu_device, cpreferences, preview);

  /* Device, which will be used, according to Settings, Scene preferences and command line
   * parameters. */
  DeviceInfo device;

  if (BlenderSession::device_override != DEVICE_MASK_ALL) {
    vector<DeviceInfo> devices = Device::available_devices(BlenderSession::device_override);

    if (devices.empty()) {
      device = Device::dummy_device("Found no Cycles device of the specified type");
    }
    else {
      int threads = blender_device_threads(b_scene);
      device = Device::get_multi_device(devices, threads, background);
    }
    adjust_device_info(device, cpreferences, preview);
  }
  else {
    /* 1 is a "GPU compute" in properties.py for Scene settings. */
    if (get_enum(cscene, "device") == 1) {
      device = preferences_device;
    }
    else {
      device = cpu_device;
    }
  }

  return device;
}

CCL_NAMESPACE_END
