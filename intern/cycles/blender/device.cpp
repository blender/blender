/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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

  if (b_r.threads_mode() == BL::RenderSettings::threads_mode_FIXED)
    return b_r.threads();
  else
    return 0;
}

DeviceInfo blender_device_info(BL::Preferences &b_preferences,
                               BL::Scene &b_scene,
                               bool background,
                               bool preview)
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
  DeviceInfo device = Device::available_devices(DEVICE_MASK_CPU).front();

  if (BlenderSession::device_override != DEVICE_MASK_ALL) {
    vector<DeviceInfo> devices = Device::available_devices(BlenderSession::device_override);

    if (devices.empty()) {
      device = Device::dummy_device("Found no Cycles device of the specified type");
    }
    else {
      int threads = blender_device_threads(b_scene);
      device = Device::get_multi_device(devices, threads, background);
    }
  }
  else if (get_enum(cscene, "device") == 1) {
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
        device = Device::get_multi_device(used_devices, threads, background);
      }
      /* Else keep using the CPU device that was set before. */
    }
  }

  if (!get_boolean(cpreferences, "peer_memory")) {
    device.has_peer_memory = false;
  }

  bool accumulated_use_hardware_raytracing = false;
  foreach (
      DeviceInfo &info,
      (device.multi_devices.size() != 0 ? device.multi_devices : vector<DeviceInfo>({device})))
  {
    if (info.type == DEVICE_METAL && !get_boolean(cpreferences, "use_metalrt")) {
      info.use_hardware_raytracing = false;
    }

    if (info.type == DEVICE_ONEAPI && !get_boolean(cpreferences, "use_oneapirt")) {
      info.use_hardware_raytracing = false;
    }

    if (info.type == DEVICE_HIP && !get_boolean(cpreferences, "use_hiprt")) {
      info.use_hardware_raytracing = false;
    }

    /* There is an accumulative logic here, because Multi-devices are support only for
     * the same backend + CPU in Blender right now, and both oneAPI and Metal have a
     * global boolean backend setting (see above) for enabling/disabling HW RT,
     * so all sub-devices in the multi-device should enable (or disable) HW RT
     * simultaneously (and CPU device are expected to ignore `use_hardware_raytracing` setting). */
    accumulated_use_hardware_raytracing |= info.use_hardware_raytracing;
  }
  device.use_hardware_raytracing = accumulated_use_hardware_raytracing;

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

  return device;
}

CCL_NAMESPACE_END
