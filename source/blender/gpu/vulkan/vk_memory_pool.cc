/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_memory_pool.hh"

#include "vk_device.hh"

namespace blender::gpu {

void VKMemoryPools::init(VKDevice &device)
{
  if (device.extensions_get().external_memory) {
    init_external_memory_image(device);
    init_external_memory_pixel_buffer(device);
  }
}

void VKMemoryPools::init_external_memory_image(VKDevice &device)
{
  VkExternalMemoryImageCreateInfo external_image_create_info = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      nullptr,
      vk_external_memory_handle_type()};
  VkImageCreateInfo image_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                         &external_image_create_info,
                                         0,
                                         VK_IMAGE_TYPE_2D,
                                         VK_FORMAT_R8G8B8A8_UNORM,
                                         {1024, 1024, 1},
                                         1,
                                         1,
                                         VK_SAMPLE_COUNT_1_BIT,
                                         VK_IMAGE_TILING_OPTIMAL,
                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                             VK_IMAGE_USAGE_SAMPLED_BIT,
                                         VK_SHARING_MODE_EXCLUSIVE,
                                         0,
                                         nullptr,
                                         VK_IMAGE_LAYOUT_UNDEFINED};
  VmaAllocationCreateInfo allocation_create_info = {};
  allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  uint32_t memory_type_index;
  vmaFindMemoryTypeIndexForImageInfo(
      device.mem_allocator_get(), &image_create_info, &allocation_create_info, &memory_type_index);

  external_memory_image.info.handleTypes = vk_external_memory_handle_type();
  VmaPoolCreateInfo pool_create_info = {};
  pool_create_info.memoryTypeIndex = memory_type_index;
  pool_create_info.pMemoryAllocateNext = &external_memory_image.info;
  pool_create_info.priority = 1.0f;
  vmaCreatePool(device.mem_allocator_get(), &pool_create_info, &external_memory_image.pool);
}

void VKMemoryPools::init_external_memory_pixel_buffer(VKDevice &device)
{
  VkExternalMemoryBufferCreateInfo external_buffer_create_info = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
      nullptr,
      vk_external_memory_handle_type()};
  VkBufferCreateInfo buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           &external_buffer_create_info,
                                           0,
                                           1024,
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0,
                                           nullptr};
  VmaAllocationCreateInfo allocation_create_info = {};
  allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  uint32_t memory_type_index;
  vmaFindMemoryTypeIndexForBufferInfo(device.mem_allocator_get(),
                                      &buffer_create_info,
                                      &allocation_create_info,
                                      &memory_type_index);

  external_memory_pixel_buffer.info.handleTypes = vk_external_memory_handle_type();
  VmaPoolCreateInfo pool_create_info = {};
  pool_create_info.memoryTypeIndex = memory_type_index;
  pool_create_info.pMemoryAllocateNext = &external_memory_pixel_buffer.info;
  pool_create_info.priority = 1.0f;
  vmaCreatePool(device.mem_allocator_get(), &pool_create_info, &external_memory_pixel_buffer.pool);
}

void VKMemoryPools::deinit(VKDevice &device)
{
  external_memory_image.deinit(device);
  external_memory_pixel_buffer.deinit(device);
}

void VKMemoryPool::deinit(VKDevice &device)
{
  vmaDestroyPool(device.mem_allocator_get(), pool);
}

}  // namespace blender::gpu
