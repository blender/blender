/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

namespace blender::gpu {
/** Information about an exported buffer/image. */
struct VKMemoryExport {
  /** Handle that has been exported. */
  uint64_t handle;
  /**
   * Allocated memory size. Allocation size can be larger than actually requested due to memory
   * alignment/allocation rules.
   */
  VkDeviceSize memory_size;
  /**
   * Actually content offset inside the exported memory. A memory allocation can contain multiple
   * buffers or images. The offset points to the specific buffer/image that is exported.
   */
  VkDeviceSize memory_offset;
};

}  // namespace blender::gpu
