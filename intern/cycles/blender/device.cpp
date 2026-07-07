/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/device.h"
#include "blender/session.h"
#include "blender/util.h"

#include "BKE_scene.hh"
#include "RNA_prototypes.hh"

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

int blender_device_threads(blender::Scene &b_scene)
{
  blender::RenderData &b_r = b_scene.r;
  int threads_override = blender::BLI_system_num_threads_override_get();
  if (threads_override > 0 || (b_r.mode & blender::R_FIXED_THREADS) != 0) {
    return BKE_render_num_threads(&b_r);
  }

  return 0;
}

static void adjust_device_info_from_preferences(DeviceInfo &info, blender::PointerRNA cpreferences)
{
  if (!get_boolean(cpreferences, "peer_memory")) {
    info.has_peer_memory = false;
  }

  if (info.type == DEVICE_METAL) {
    const MetalRTSetting use_metalrt = (MetalRTSetting)get_enum(
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

static void adjust_device_info(DeviceInfo &device, blender::PointerRNA cpreferences, bool preview)
{
  adjust_device_info_from_preferences(device, cpreferences);
  for (DeviceInfo &info : device.multi_devices) {
    adjust_device_info_from_preferences(info, cpreferences);
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

DeviceInfo blender_device_info(blender::UserDef &b_preferences,
                               blender::Scene &b_scene,
                               bool background,
                               bool preview,
                               DeviceInfo &preferences_device)
{
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");

  /* Find cycles preferences. */
  blender::PointerRNA cpreferences;
  for (blender::bAddon &b_addon : b_preferences.addons) {
    if (STREQ(b_addon.module, "cycles")) {
      blender::PointerRNA addon_rna_ptr = RNA_pointer_create_discrete(
          nullptr, blender::RNA_Addon, &b_addon);
      cpreferences = RNA_pointer_get(&addon_rna_ptr, "preferences");
      break;
    }
  }

  /* Default to CPU device. */
  DeviceInfo cpu_device = Device::available_devices(DEVICE_MASK_CPU).front();

  /* Device, which is chosen in the Blender Preferences. Default to CPU device. */
  preferences_device = cpu_device;

  /* Test if we are using GPU devices. */
  const ComputeDevice compute_device = (ComputeDevice)get_enum(
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
    const vector<DeviceInfo> devices = Device::available_devices(mask);

    /* Match device preferences and available devices. */
    vector<DeviceInfo> used_devices;
    blender::CollectionPropertyIterator rna_iter;
    for (RNA_collection_begin(&cpreferences, "devices", &rna_iter); rna_iter.valid;
         RNA_property_collection_next(&rna_iter))
    {
      blender::PointerRNA device = rna_iter.ptr;
      if (get_boolean(device, "use")) {
        const string id = get_string(device, "id");
        for (const DeviceInfo &info : devices) {
          if (info.id == id) {
            used_devices.push_back(info);
            break;
          }
        }
      }
    }
    blender::RNA_property_collection_end(&rna_iter);

    if (!used_devices.empty()) {
      const int threads = blender_device_threads(b_scene);
      preferences_device = Device::get_multi_device(used_devices, threads, background);
    }
  }

  adjust_device_info(preferences_device, cpreferences, preview);
  adjust_device_info(cpu_device, cpreferences, preview);

  /* Device, which will be used, according to Settings, Scene preferences and command line
   * parameters. */
  DeviceInfo device;

  if (BlenderSession::device_override != DEVICE_MASK_ALL) {
    const vector<DeviceInfo> devices = Device::available_devices(BlenderSession::device_override);

    if (devices.empty()) {
      device = Device::dummy_device("Found no Cycles device of the specified type");
    }
    else {
      const int threads = blender_device_threads(b_scene);
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
