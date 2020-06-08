/*
 * Copyright 2011-2013 Blender Foundation
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

#include "blender/blender_device.h"
#include "blender/blender_util.h"

#include "util/util_foreach.h"

CCL_NAMESPACE_BEGIN

enum DenoiserType {
  DENOISER_NONE = 0,
  DENOISER_OPTIX = 1,

  DENOISER_NUM
};

enum ComputeDevice {
  COMPUTE_DEVICE_CPU = 0,
  COMPUTE_DEVICE_CUDA = 1,
  COMPUTE_DEVICE_OPENCL = 2,
  COMPUTE_DEVICE_OPTIX = 3,

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

DeviceInfo blender_device_info(BL::Preferences &b_preferences, BL::Scene &b_scene, bool background)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  /* Default to CPU device. */
  DeviceInfo device = Device::available_devices(DEVICE_MASK_CPU).front();

  if (get_enum(cscene, "device") == 2) {
    /* Find network device. */
    vector<DeviceInfo> devices = Device::available_devices(DEVICE_MASK_NETWORK);
    if (!devices.empty()) {
      return devices.front();
    }
  }
  else if (get_enum(cscene, "device") == 1) {
    /* Find cycles preferences. */
    PointerRNA cpreferences;

    BL::Preferences::addons_iterator b_addon_iter;
    for (b_preferences.addons.begin(b_addon_iter); b_addon_iter != b_preferences.addons.end();
         ++b_addon_iter) {
      if (b_addon_iter->module() == "cycles") {
        cpreferences = b_addon_iter->preferences().ptr;
        break;
      }
    }

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
        /* Cannot use CPU and OptiX device at the same time right now, so replace mask. */
        mask = DEVICE_MASK_OPTIX;
      }
      else if (compute_device == COMPUTE_DEVICE_OPENCL) {
        mask |= DEVICE_MASK_OPENCL;
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

      if (!get_boolean(cpreferences, "peer_memory")) {
        device.has_peer_memory = false;
      }
    }
  }

  /* Ensure there is an OptiX device when using the OptiX denoiser. */
  bool use_optix_denoising = get_enum(cscene, "preview_denoising", DENOISER_NUM, DENOISER_NONE) ==
                                 DENOISER_OPTIX &&
                             !background;
  BL::Scene::view_layers_iterator b_view_layer;
  for (b_scene.view_layers.begin(b_view_layer); b_view_layer != b_scene.view_layers.end();
       ++b_view_layer) {
    PointerRNA crl = RNA_pointer_get(&b_view_layer->ptr, "cycles");
    if (get_boolean(crl, "use_optix_denoising")) {
      use_optix_denoising = true;
    }
  }

  if (use_optix_denoising && device.type != DEVICE_OPTIX) {
    vector<DeviceInfo> optix_devices = Device::available_devices(DEVICE_MASK_OPTIX);
    if (!optix_devices.empty()) {
      /* Convert to a special multi device with separate denoising devices. */
      if (device.multi_devices.empty()) {
        device.multi_devices.push_back(device);
      }

      /* Simply use the first available OptiX device. */
      const DeviceInfo optix_device = optix_devices.front();
      device.id += optix_device.id; /* Uniquely identify this special multi device. */
      device.denoising_devices.push_back(optix_device);
    }
  }

  return device;
}

CCL_NAMESPACE_END
