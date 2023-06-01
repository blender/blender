/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_sampler.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory.hh"

namespace blender::gpu {
VKSampler::~VKSampler()
{
  free();
}

void VKSampler::create()
{
  BLI_assert(vk_sampler_ == VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS

  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  const VKDevice &device = VKBackend::get().device_get();
  vkCreateSampler(device.device_get(), &sampler_info, vk_allocation_callbacks, &vk_sampler_);
  debug::object_label(vk_sampler_, "DummySampler");
}

void VKSampler::free()
{
  VK_ALLOCATION_CALLBACKS

  if (vk_sampler_ != VK_NULL_HANDLE) {
    const VKDevice &device = VKBackend::get().device_get();
    if (device.device_get() != VK_NULL_HANDLE) {
      vkDestroySampler(device.device_get(), vk_sampler_, vk_allocation_callbacks);
    }
    vk_sampler_ = VK_NULL_HANDLE;
  }
}

}  // namespace blender::gpu
