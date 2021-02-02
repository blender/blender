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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

/* GPU platform support */

/* GPU Types */
typedef enum eGPUDeviceType {
  GPU_DEVICE_NVIDIA = (1 << 0),
  GPU_DEVICE_ATI = (1 << 1),
  GPU_DEVICE_INTEL = (1 << 2),
  GPU_DEVICE_INTEL_UHD = (1 << 3),
  GPU_DEVICE_APPLE = (1 << 3),
  GPU_DEVICE_SOFTWARE = (1 << 4),
  GPU_DEVICE_UNKNOWN = (1 << 5),
  GPU_DEVICE_ANY = (0xff),
} eGPUDeviceType;

ENUM_OPERATORS(eGPUDeviceType, GPU_DEVICE_ANY)

typedef enum eGPUOSType {
  GPU_OS_WIN = (1 << 8),
  GPU_OS_MAC = (1 << 9),
  GPU_OS_UNIX = (1 << 10),
  GPU_OS_ANY = (0xff00),
} eGPUOSType;

typedef enum eGPUDriverType {
  GPU_DRIVER_OFFICIAL = (1 << 16),
  GPU_DRIVER_OPENSOURCE = (1 << 17),
  GPU_DRIVER_SOFTWARE = (1 << 18),
  GPU_DRIVER_ANY = (0xff0000),
} eGPUDriverType;

typedef enum eGPUSupportLevel {
  GPU_SUPPORT_LEVEL_SUPPORTED,
  GPU_SUPPORT_LEVEL_LIMITED,
  GPU_SUPPORT_LEVEL_UNSUPPORTED,
} eGPUSupportLevel;

#ifdef __cplusplus
extern "C" {
#endif

bool GPU_type_matches(eGPUDeviceType device, eGPUOSType os, eGPUDriverType driver);
eGPUSupportLevel GPU_platform_support_level(void);
const char *GPU_platform_support_level_key(void);
const char *GPU_platform_gpu_name(void);

#ifdef __cplusplus
}
#endif
