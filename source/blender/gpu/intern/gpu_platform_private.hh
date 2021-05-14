/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

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

 public:
  void init(eGPUDeviceType gpu_device,
            eGPUOSType os_type,
            eGPUDriverType driver_type,
            eGPUSupportLevel gpu_support_level,
            const char *vendor_str,
            const char *renderer_str,
            const char *version_str);

  void clear(void);
};

extern GPUPlatformGlobal GPG;

}  // namespace blender::gpu
