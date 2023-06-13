/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

/* GPU platform support */

typedef enum eGPUBackendType {
  GPU_BACKEND_NONE = 0,
  GPU_BACKEND_OPENGL = 1 << 0,
  GPU_BACKEND_METAL = 1 << 1,
  GPU_BACKEND_VULKAN = 1 << 3,
  GPU_BACKEND_ANY = 0xFFFFFFFFu
} eGPUBackendType;

/* GPU Types */
typedef enum eGPUDeviceType {
  GPU_DEVICE_NVIDIA = (1 << 0),
  GPU_DEVICE_ATI = (1 << 1),
  GPU_DEVICE_INTEL = (1 << 2),
  GPU_DEVICE_INTEL_UHD = (1 << 3),
  GPU_DEVICE_APPLE = (1 << 4),
  GPU_DEVICE_SOFTWARE = (1 << 5),
  GPU_DEVICE_UNKNOWN = (1 << 6),
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

/* GPU Types */
/* TODO: Verify all use-cases of GPU_type_matches to determine which graphics API it should apply
 * to, and replace with `GPU_type_matches_ex` where appropriate. */
bool GPU_type_matches(eGPUDeviceType device, eGPUOSType os, eGPUDriverType driver);
bool GPU_type_matches_ex(eGPUDeviceType device,
                         eGPUOSType os,
                         eGPUDriverType driver,
                         eGPUBackendType backend);

eGPUSupportLevel GPU_platform_support_level(void);
const char *GPU_platform_vendor(void);
const char *GPU_platform_renderer(void);
const char *GPU_platform_version(void);
const char *GPU_platform_support_level_key(void);
const char *GPU_platform_gpu_name(void);

#ifdef __cplusplus
}
#endif
