/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "vk_mem_alloc.h"

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

namespace blender::gpu {

class VKContext : public Context {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  uint32_t graphic_queue_familly_ = 0;

  /** Allocator used for texture and buffers and other resources. */
  VmaAllocator mem_allocator_ = VK_NULL_HANDLE;

 public:
  VKContext(void *ghost_window, void *ghost_context);
  virtual ~VKContext();

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;

  static VKContext *get(void)
  {
    return static_cast<VKContext *>(Context::get());
  }

  VkDevice device_get() const
  {
    return device_;
  }

  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }
};

}  // namespace blender::gpu