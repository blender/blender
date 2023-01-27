/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_context.hh"

#include "vk_backend.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

VKContext::VKContext(void *ghost_window, void *ghost_context)
{
  ghost_window_ = ghost_window;
  if (ghost_window) {
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }

  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
                         &instance_,
                         &physical_device_,
                         &device_,
                         &graphic_queue_familly_);

  /* Initialize the memory allocator. */
  VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST. */
  info.vulkanApiVersion = VK_API_VERSION_1_2;
  info.physicalDevice = physical_device_;
  info.device = device_;
  info.instance = instance_;
  vmaCreateAllocator(&info, &mem_allocator_);

  VKBackend::capabilities_init(*this);
}

VKContext::~VKContext()
{
  vmaDestroyAllocator(mem_allocator_);
}

void VKContext::activate()
{
}

void VKContext::deactivate()
{
}

void VKContext::begin_frame()
{
}

void VKContext::end_frame()
{
}

void VKContext::flush()
{
}

void VKContext::finish()
{
}

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/)
{
}

void VKContext::debug_group_begin(const char *, int)
{
}

void VKContext::debug_group_end()
{
}

}  // namespace blender::gpu