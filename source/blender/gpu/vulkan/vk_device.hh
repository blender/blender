/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_descriptor_pools.hh"

namespace blender::gpu {
class VKBackend;

struct VKWorkarounds {
  /**
   * Some devices don't support pixel formats that are aligned to 24 and 48 bits.
   * In this case we need to use a different texture format.
   *
   * If set to true we should work around this issue by using a different texture format.
   */
  bool not_aligned_pixel_formats = false;
};

class VKDevice : public NonCopyable {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  uint32_t vk_queue_family_ = 0;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /** Allocator used for texture and buffers and other resources. */
  VmaAllocator mem_allocator_ = VK_NULL_HANDLE;
  VKDescriptorPools descriptor_pools_;

  /** Limits of the device linked to this context. */
  VkPhysicalDeviceProperties vk_physical_device_properties_ = {};

  /** Functions of vk_ext_debugutils for this device/instance. */
  debug::VKDebuggingTools debugging_tools_;

  /* Workarounds */
  VKWorkarounds workarounds_;

 public:
  VkPhysicalDevice physical_device_get() const
  {
    return vk_physical_device_;
  }

  const VkPhysicalDeviceProperties &physical_device_properties_get() const
  {
    return vk_physical_device_properties_;
  }

  VkInstance instance_get() const
  {
    return vk_instance_;
  };

  VkDevice device_get() const
  {
    return vk_device_;
  }

  VkQueue queue_get() const
  {
    return vk_queue_;
  }

  VKDescriptorPools &descriptor_pools_get()
  {
    return descriptor_pools_;
  }

  const uint32_t *queue_family_ptr_get() const
  {
    return &vk_queue_family_;
  }

  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }

  debug::VKDebuggingTools &debugging_tools_get()
  {
    return debugging_tools_;
  }

  const debug::VKDebuggingTools &debugging_tools_get() const
  {
    return debugging_tools_;
  }

  bool is_initialized() const;
  void init(void *ghost_context);
  void deinit();

  eGPUDeviceType device_type() const;
  eGPUDriverType driver_type() const;
  std::string vendor_name() const;
  std::string driver_version() const;

  const VKWorkarounds &workarounds_get() const
  {
    return workarounds_;
  }

 private:
  void init_physical_device_properties();
  void init_debug_callbacks();
  void init_memory_allocator();
  void init_descriptor_pools();

  /* During initialization the backend requires access to update the workarounds. */
  friend VKBackend;
};

}  // namespace blender::gpu
