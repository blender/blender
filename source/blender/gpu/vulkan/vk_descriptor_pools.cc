/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_capabilities.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_descriptor_pools.hh"
#include "vk_device.hh"

namespace blender::gpu {

void VKDescriptorPools::deinit(const VKDevice &device)
{
  std::scoped_lock lock(mutex_);
  for (const VkDescriptorPool vk_descriptor_pool : recycled_pools_) {
    device.functions.vkDestroyDescriptorPool(device.vk_handle(), vk_descriptor_pool, nullptr);
  }
  recycled_pools_.clear();
}

VkDescriptorPool VKDescriptorPools::acquire_pool(const VKDevice &device)
{
  std::scoped_lock lock(mutex_);
  if (!recycled_pools_.is_empty()) {
    return recycled_pools_.pop_last();
  }

  Vector<VkDescriptorPoolSize> pool_sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, POOL_SIZE_STORAGE_BUFFER},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, POOL_SIZE_STORAGE_IMAGE},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_SIZE_COMBINED_IMAGE_SAMPLER},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, POOL_SIZE_UNIFORM_BUFFER},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, POOL_SIZE_UNIFORM_TEXEL_BUFFER},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, POOL_SIZE_INPUT_ATTACHMENT}};
  if (GPU_ray_query_support()) {
    pool_sizes.append(
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, POOL_SIZE_ACCELERATION_STRUCTURE});
  }
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = POOL_SIZE_DESCRIPTOR_SETS;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
  device.functions.vkCreateDescriptorPool(
      device.vk_handle(), &pool_info, nullptr, &vk_descriptor_pool);
  return vk_descriptor_pool;
}

void VKDescriptorPools::discard_active_pool(VKContext &context)
{
  if (context.vk_descriptor_pool_ == VK_NULL_HANDLE) {
    return;
  }
  context.discard_pool.discard_descriptor_pool_for_reuse(context.vk_descriptor_pool_);
  context.vk_descriptor_pool_ = VK_NULL_HANDLE;
}

void VKDescriptorPools::recycle(VkDescriptorPool vk_descriptor_pool)
{
  const VKDevice &device = VKBackend::get().device;
  device.functions.vkResetDescriptorPool(device.vk_handle(), vk_descriptor_pool, 0);
  std::scoped_lock lock(mutex_);
  recycled_pools_.append(vk_descriptor_pool);
}

VkDescriptorSet VKDescriptorPools::allocate(VKContext &context,
                                            const VkDescriptorSetLayout descriptor_set_layout)
{
  BLI_assert(descriptor_set_layout != VK_NULL_HANDLE);
  const VKDevice &device = VKBackend::get().device;
  if (context.vk_descriptor_pool_ == VK_NULL_HANDLE) {
    context.vk_descriptor_pool_ = acquire_pool(device);
  }

  VkDescriptorSetAllocateInfo allocate_info = {};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = context.vk_descriptor_pool_;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &descriptor_set_layout;
  VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
  VkResult result = device.functions.vkAllocateDescriptorSets(
      device.vk_handle(), &allocate_info, &vk_descriptor_set);

  if (ELEM(result, VK_ERROR_OUT_OF_POOL_MEMORY, VK_ERROR_FRAGMENTED_POOL)) {
    discard_active_pool(context);
    return allocate(context, descriptor_set_layout);
  }

  return vk_descriptor_set;
}

}  // namespace blender::gpu
