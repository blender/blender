/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "GPU_platform_backend_enum.h"

/* GPU platform support */

/* GPU Types */
enum eGPUDeviceType {
  GPU_DEVICE_NVIDIA = (1 << 0),
  GPU_DEVICE_ATI = (1 << 1),
  GPU_DEVICE_INTEL = (1 << 2),
  GPU_DEVICE_INTEL_UHD = (1 << 3),
  GPU_DEVICE_APPLE = (1 << 4),
  GPU_DEVICE_SOFTWARE = (1 << 5),
  GPU_DEVICE_QUALCOMM = (1 << 6),
  GPU_DEVICE_UNKNOWN = (1 << 7),
  GPU_DEVICE_ANY = (0xff),
};

ENUM_OPERATORS(eGPUDeviceType, GPU_DEVICE_ANY)

enum eGPUOSType {
  GPU_OS_WIN = (1 << 8),
  GPU_OS_MAC = (1 << 9),
  GPU_OS_UNIX = (1 << 10),
  GPU_OS_ANY = (0xff00),
};

enum eGPUDriverType {
  GPU_DRIVER_OFFICIAL = (1 << 16),
  GPU_DRIVER_OPENSOURCE = (1 << 17),
  GPU_DRIVER_SOFTWARE = (1 << 18),
  GPU_DRIVER_ANY = (0xff0000),
};

enum eGPUSupportLevel {
  GPU_SUPPORT_LEVEL_SUPPORTED,
  GPU_SUPPORT_LEVEL_LIMITED,
  GPU_SUPPORT_LEVEL_UNSUPPORTED,
};

enum GPUArchitectureType {
  /* Immediate Mode Renderer (IMR).
   * Typically, an IMR architecture will execute GPU work in sequence, rasterizing primitives in
   * order. */
  GPU_ARCHITECTURE_IMR = 0,

  /* Tile-Based-Deferred-Renderer (TBDR).
   * A TBDR architecture will typically execute the vertex stage up-front for all primitives,
   * binning geometry into distinct tiled regions. Fragments will then be rasterized within
   * the bounds of one tile at a time. */
  GPU_ARCHITECTURE_TBDR = 1,
};

/* GPU Types */
/* TODO: Verify all use-cases of GPU_type_matches to determine which graphics API it should apply
 * to, and replace with `GPU_type_matches_ex` where appropriate. */
bool GPU_type_matches(eGPUDeviceType device, eGPUOSType os, eGPUDriverType driver);
bool GPU_type_matches_ex(eGPUDeviceType device,
                         eGPUOSType os,
                         eGPUDriverType driver,
                         eGPUBackendType backend);

eGPUSupportLevel GPU_platform_support_level();
const char *GPU_platform_vendor();
const char *GPU_platform_renderer();
const char *GPU_platform_version();
const char *GPU_platform_support_level_key();
const char *GPU_platform_gpu_name();
GPUArchitectureType GPU_platform_architecture();
