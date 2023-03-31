/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_platform.h"

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

 public:
  void init(eGPUDeviceType gpu_device,
            eGPUOSType os_type,
            eGPUDriverType driver_type,
            eGPUSupportLevel gpu_support_level,
            eGPUBackendType backend,
            const char *vendor_str,
            const char *renderer_str,
            const char *version_str);

  void clear();
};

extern GPUPlatformGlobal GPG;

}  // namespace blender::gpu
