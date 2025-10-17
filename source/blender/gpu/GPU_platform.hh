/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "BLI_enum_flags.hh"
#include "BLI_span.hh"

#include "GPU_platform_backend_enum.h"  // IWYU pragma: export

/* GPU platform support */

/* GPU Types */
enum GPUDeviceType {
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

ENUM_OPERATORS(GPUDeviceType)

enum GPUOSType {
  GPU_OS_WIN = (1 << 8),
  GPU_OS_MAC = (1 << 9),
  GPU_OS_UNIX = (1 << 10),
  GPU_OS_ANY = (0xff00),
};

enum GPUDriverType {
  GPU_DRIVER_OFFICIAL = (1 << 16),
  GPU_DRIVER_OPENSOURCE = (1 << 17),
  GPU_DRIVER_SOFTWARE = (1 << 18),
  GPU_DRIVER_ANY = (0xff0000),
};

enum GPUSupportLevel {
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

struct GPUDevice {
  std::string identifier;
  int index;
  uint32_t vendor_id;
  uint32_t device_id;
  std::string name;
};

/* GPU Types */
/* TODO: Verify all use-cases of GPU_type_matches to determine which graphics API it should apply
 * to, and replace with `GPU_type_matches_ex` where appropriate. */
bool GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver);
bool GPU_type_matches_ex(GPUDeviceType device,
                         GPUOSType os,
                         GPUDriverType driver,
                         GPUBackendType backend);

GPUSupportLevel GPU_platform_support_level();
const char *GPU_platform_vendor();
const char *GPU_platform_renderer();
const char *GPU_platform_version();
const char *GPU_platform_support_level_key();
const char *GPU_platform_gpu_name();
GPUArchitectureType GPU_platform_architecture();
blender::Span<GPUDevice> GPU_platform_devices_list();

/* The UUID of the device. Can be an empty array, since it is not supported on all platforms. */
blender::Span<uint8_t> GPU_platform_uuid();
/* The LUID of the device. Can be an empty array, since it is not supported on all platforms. */
blender::Span<uint8_t> GPU_platform_luid();
/* A bit field with the nth bit active identifying the nth device with the same LUID. Only matters
 * if LUID is defined. */
uint32_t GPU_platform_luid_node_mask();
