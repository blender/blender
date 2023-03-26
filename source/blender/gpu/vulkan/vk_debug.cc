/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_backend.hh"
#include "vk_context.hh"

namespace blender::gpu {
void VKContext::debug_group_begin(const char *, int)
{
}

void VKContext::debug_group_end()
{
}

bool VKContext::debug_capture_begin()
{
  return VKBackend::get().debug_capture_begin(vk_instance_);
}

bool VKBackend::debug_capture_begin(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  return renderdoc_api_.start_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
  return false;
#endif
}

void VKContext::debug_capture_end()
{
  VKBackend::get().debug_capture_end(vk_instance_);
}

void VKBackend::debug_capture_end(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  renderdoc_api_.end_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
#endif
}

void *VKContext::debug_capture_scope_create(const char * /*name*/)
{
  return nullptr;
}

bool VKContext::debug_capture_scope_begin(void * /*scope*/)
{
  return false;
}

void VKContext::debug_capture_scope_end(void * /*scope*/)
{
}
}  // namespace blender::gpu
