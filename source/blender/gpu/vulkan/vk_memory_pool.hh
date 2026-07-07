/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

namespace blender::gpu {
class VKDevice;

constexpr VkExternalMemoryHandleTypeFlags vk_external_memory_handle_type()
{
#ifdef _WIN32
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
}

/**
 * VMA related data for a memory pool.
 */
struct VKMemoryPool {
  /* NOTE: This attribute needs to be kept alive as it will be read by VMA when allocating inside
   * the pool. */
  VkExportMemoryAllocateInfoKHR info = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR};
  VmaPool pool = VK_NULL_HANDLE;

  void deinit(VKDevice &device);
};

struct VKMemoryPools {
  VKMemoryPool external_memory_image = {};
  VKMemoryPool external_memory_pixel_buffer = {};

  void init(VKDevice &device);
  void deinit(VKDevice &device);

 private:
  void init_external_memory_image(VKDevice &device);
  void init_external_memory_pixel_buffer(VKDevice &device);
};

}  // namespace blender::gpu
