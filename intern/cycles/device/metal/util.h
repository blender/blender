/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_METAL

#  include <Metal/Metal.h>
#  include <string>

#  include "device/metal/device.h"
#  include "device/metal/kernel.h"
#  include "device/queue.h"

#  include "util/thread.h"

#  define metal_printf LOG_STATS << string_printf

CCL_NAMESPACE_BEGIN

enum AppleGPUArchitecture {
  APPLE_M1,
  APPLE_M2,
  APPLE_M2_BIG,
  APPLE_M3,
  /* Keep APPLE_UNKNOWN at the end of this enum to ensure that unknown future architectures get
   * the most recent defaults when using comparison operators. */
  APPLE_UNKNOWN,
};

/* Contains static Metal helper functions. */
struct MetalInfo {
  static const vector<id<MTLDevice>> &get_usable_devices();
  static int get_apple_gpu_core_count(id<MTLDevice> device);
  static AppleGPUArchitecture get_apple_gpu_architecture(id<MTLDevice> device);
  static int optimal_sort_partition_elements();
  static string get_device_name(id<MTLDevice> device);
};

void metal_gpu_address_helper_init(id<MTLDevice> device);

uint64_t metal_gpuAddress(id<MTLBuffer> buffer);
uint64_t metal_gpuResourceID(id<MTLTexture> texture);
uint64_t metal_gpuResourceID(id<MTLAccelerationStructure> accel_struct);
uint64_t metal_gpuResourceID(id<MTLIntersectionFunctionTable> ift);

CCL_NAMESPACE_END

#endif /* WITH_METAL */
