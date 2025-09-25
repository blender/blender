/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <cstdint>

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "GPU_platform.hh"

namespace blender::gpu {

class GPUPlatformGlobal {
 public:
  bool initialized = false;
  GPUDeviceType device;
  GPUOSType os;
  GPUDriverType driver;
  GPUSupportLevel support_level;
  char *vendor = nullptr;
  char *renderer = nullptr;
  char *version = nullptr;
  char *support_key = nullptr;
  char *gpu_name = nullptr;
  GPUBackendType backend = GPU_BACKEND_NONE;
  GPUArchitectureType architecture_type = GPU_ARCHITECTURE_IMR;
  Vector<GPUDevice> devices;

  /* The UUID of the device. Can be an empty array, since it is not supported on all platforms. */
  Array<uint8_t, 16> device_uuid;
  /* The LUID of the device. Can be an empty array, since it is not supported on all platforms. */
  Array<uint8_t, 8> device_luid;
  /* A bit field with the nth bit active identifying the nth device with the same LUID. Only
   * matters if device_luid is defined. */
  uint32_t device_luid_node_mask;

  void init(GPUDeviceType gpu_device,
            GPUOSType os_type,
            GPUDriverType driver_type,
            GPUSupportLevel gpu_support_level,
            GPUBackendType backend,
            const char *vendor_str,
            const char *renderer_str,
            const char *version_str,
            GPUArchitectureType arch_type);

  void clear();
};

extern GPUPlatformGlobal GPG;

}  // namespace blender::gpu
