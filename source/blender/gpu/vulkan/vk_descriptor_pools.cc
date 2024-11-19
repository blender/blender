/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_descriptor_pools.hh"
#include "vk_backend.hh"
#include "vk_device.hh"

namespace blender::gpu {
VKDescriptorPools::VKDescriptorPools() {}

VKDescriptorPools::~VKDescriptorPools()
{
  const VKDevice &device = VKBackend::get().device;
  for (const VkDescriptorPool vk_descriptor_pool : pools_) {
    vkDestroyDescriptorPool(device.vk_handle(), vk_descriptor_pool, nullptr);
  }
}

void VKDescriptorPools::init(const VKDevice &device)
{
  BLI_assert(pools_.is_empty());
  add_new_pool(device);
}

void VKDescriptorPools::reset()
{
  const VKDevice &device = VKBackend::get().device;
  for (const VkDescriptorPool vk_descriptor_pool : pools_) {
    vkResetDescriptorPool(device.vk_handle(), vk_descriptor_pool, 0);
  }

  active_pool_index_ = 0;
}

void VKDescriptorPools::add_new_pool(const VKDevice &device)
{
  Vector<VkDescriptorPoolSize> pool_sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, POOL_SIZE_STORAGE_BUFFER},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, POOL_SIZE_STORAGE_IMAGE},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_SIZE_COMBINED_IMAGE_SAMPLER},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, POOL_SIZE_UNIFORM_BUFFER},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, POOL_SIZE_UNIFORM_TEXEL_BUFFER},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, POOL_SIZE_INPUT_ATTACHMENT}};
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = POOL_SIZE_DESCRIPTOR_SETS;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkResult result = vkCreateDescriptorPool(
      device.vk_handle(), &pool_info, nullptr, &descriptor_pool);
  UNUSED_VARS(result);
  pools_.append(descriptor_pool);
}

VkDescriptorPool VKDescriptorPools::active_pool_get()
{
  BLI_assert(!pools_.is_empty());
  return pools_[active_pool_index_];
}

void VKDescriptorPools::activate_next_pool()
{
  BLI_assert(!is_last_pool_active());
  active_pool_index_ += 1;
}

void VKDescriptorPools::activate_last_pool()
{
  active_pool_index_ = pools_.size() - 1;
}

bool VKDescriptorPools::is_last_pool_active()
{
  return active_pool_index_ == pools_.size() - 1;
}

VkDescriptorSet VKDescriptorPools::allocate(const VkDescriptorSetLayout descriptor_set_layout)
{
  BLI_assert(descriptor_set_layout != VK_NULL_HANDLE);
  const VKDevice &device = VKBackend::get().device;

  VkDescriptorSetAllocateInfo allocate_info = {};
  VkDescriptorPool pool = active_pool_get();
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = pool;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &descriptor_set_layout;
  VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
  VkResult result = vkAllocateDescriptorSets(
      device.vk_handle(), &allocate_info, &vk_descriptor_set);

  if (ELEM(result, VK_ERROR_OUT_OF_POOL_MEMORY, VK_ERROR_FRAGMENTED_POOL)) {
    if (is_last_pool_active()) {
      add_new_pool(device);
      activate_last_pool();
    }
    else {
      activate_next_pool();
    }
    return allocate(descriptor_set_layout);
  }

  return vk_descriptor_set;
}

}  // namespace blender::gpu
