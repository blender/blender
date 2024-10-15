/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_fence.hh"
#include "vk_backend.hh"
#include "vk_common.hh"
#include "vk_context.hh"

namespace blender::gpu {

VKFence::~VKFence()
{
  if (vk_fence_ != VK_NULL_HANDLE) {
    VKDevice &device = VKBackend::get().device;
    vkDestroyFence(device.vk_handle(), vk_fence_, nullptr);
    vk_fence_ = VK_NULL_HANDLE;
  }
}

void VKFence::signal()
{
  if (vk_fence_ == VK_NULL_HANDLE) {
    VKDevice &device = VKBackend::get().device;
    VkFenceCreateInfo vk_fence_create_info = {};
    vk_fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk_fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device.vk_handle(), &vk_fence_create_info, nullptr, &vk_fence_);
  }
  VKContext &context = *VKContext::get();
  context.rendering_end();
  context.descriptor_set_get().upload_descriptor_sets();
  context.render_graph.submit_synchronization_event(vk_fence_);
  signalled_ = true;
}

void VKFence::wait()
{
  if (!signalled_) {
    return;
  }
  VKContext &context = *VKContext::get();
  context.render_graph.wait_synchronization_event(vk_fence_);
  signalled_ = false;
}

}  // namespace blender::gpu
