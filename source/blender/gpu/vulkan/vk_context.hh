/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "vk_command_buffer.hh"
#include "vk_descriptor_pools.hh"

#include "vk_mem_alloc.h"

namespace blender::gpu {

class VKContext : public Context {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VKCommandBuffer command_buffer_;
  uint32_t vk_queue_family_ = 0;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /** Allocator used for texture and buffers and other resources. */
  VmaAllocator mem_allocator_ = VK_NULL_HANDLE;
  VKDescriptorPools descriptor_pools_;

  void *ghost_context_;

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

  VkPhysicalDevice physical_device_get() const
  {
    return vk_physical_device_;
  }

  VkDevice device_get() const
  {
    return vk_device_;
  }

  VKCommandBuffer &command_buffer_get()
  {
    return command_buffer_;
  }

  VkQueue queue_get() const
  {
    return vk_queue_;
  }

  const uint32_t *queue_family_ptr_get() const
  {
    return &vk_queue_family_;
  }

  VKDescriptorPools &descriptor_pools_get()
  {
    return descriptor_pools_;
  }

  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }
};

}  // namespace blender::gpu
