/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_platform.hh"

namespace blender::gpu {

class GPUPlatformGlobal {
 public:
  bool initialized = false;
  eGPUDeviceType device;
  eGPUOSType os;
  eGPUDriverType driver;
  eGPUSupportLevel support_level;
  char *vendor = nullptr;
  char *renderer = nullptr;
  char *version = nullptr;
  char *support_key = nullptr;
  char *gpu_name = nullptr;
  eGPUBackendType backend = GPU_BACKEND_NONE;
  GPUArchitectureType architecture_type = GPU_ARCHITECTURE_IMR;

 public:
  void init(eGPUDeviceType gpu_device,
            eGPUOSType os_type,
            eGPUDriverType driver_type,
            eGPUSupportLevel gpu_support_level,
            eGPUBackendType backend,
            const char *vendor_str,
            const char *renderer_str,
            const char *version_str,
            GPUArchitectureType arch_type);

  void clear();
};

extern GPUPlatformGlobal GPG;

}  // namespace blender::gpu
