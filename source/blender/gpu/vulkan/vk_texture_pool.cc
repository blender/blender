/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_capabilities.hh"

#include "vk_backend.hh"
#include "vk_texture.hh"
#include "vk_texture_pool.hh"

namespace blender::gpu {

bool VKTexturePool::AllocationHandle::alloc(VkMemoryRequirements memory_requirements)
{
  VKDevice &device = VKBackend::get().device;
  VmaAllocationCreateInfo create_info = {};
  create_info.priority = 1.0f;
  create_info.memoryTypeBits = memory_requirements.memoryTypeBits;
  create_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkResult result = vmaAllocateMemory(device.mem_allocator_get(),
                                      &memory_requirements,
                                      &create_info,
                                      &allocation,
                                      &allocation_info);
  return result == VK_SUCCESS;
}

void VKTexturePool::AllocationHandle::free()
{
  VKDevice &device = VKBackend::get().device;
  /* TODO(not_mark): allocation needs to go to discard pool, but for that it needs to be tracked.
   * This is only OK right now because `max_unused_cycles_` is sufficiently large. */
  vmaFreeMemory(device.mem_allocator_get(), allocation);
}

bool VKTexturePool::TextureHandle::alloc(int2 extent,
                                         TextureFormat format,
                                         eGPUTextureUsage usage,
                                         const char *name)
{
  VKDevice &device = VKBackend::get().device;

  texture = new VKTexture(name);
  texture->w_ = extent.x;
  texture->h_ = extent.y;
  texture->d_ = 0;
  texture->format_ = format;
  texture->format_flag_ = to_format_flag(format);
  texture->type_ = GPU_TEXTURE_2D;
  texture->gpu_image_usage_flags_ = usage;

  /* R16G16F16 formats are typically not supported (<1%). */
  texture->device_format_ = format;
  if (texture->device_format_ == TextureFormat::SFLOAT_16_16_16) {
    texture->device_format_ = TextureFormat::SFLOAT_16_16_16_16;
  }
  if (texture->device_format_ == TextureFormat::SFLOAT_32_32_32) {
    texture->device_format_ = TextureFormat::SFLOAT_32_32_32_32;
  }

  /* Mirrors behavior in gpu::Texture::init_2d(...). */
  if ((texture->format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    texture->sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  }

  /* Create a VkImage object. */
  VkImageCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create_info.flags = to_vk_image_create(GPU_TEXTURE_2D, to_format_flag(format), usage);
  create_info.usage = to_vk_image_usage(usage, to_format_flag(format), false);
  create_info.format = to_vk_format(format);
  create_info.arrayLayers = 1;
  create_info.mipLevels = 1;
  create_info.imageType = VK_IMAGE_TYPE_2D;
  create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  create_info.extent.width = static_cast<uint32_t>(extent.x);
  create_info.extent.height = static_cast<uint32_t>(extent.y);
  create_info.extent.depth = 1u;
  VkResult result = vkCreateImage(
      device.vk_handle(), &create_info, nullptr, &(texture->vk_image_));

  return result == VK_SUCCESS;
}

void VKTexturePool::TextureHandle::free()
{
  /* The image is forwarded for discard, but the allocation is not. It is
   * safe to not unbind an image from an allocation in VMA when freeing it. */
  VKDiscardPool::discard_pool_get().discard_image(texture->vk_image_, VK_NULL_HANDLE);

  /* VKTexture destructor is skipped as `VKTexture::allocation_` is `VK_NULL_HANDLE`. */
  delete texture;
}

VKTexturePool::~VKTexturePool()
{
  for (auto &handle : acquired_) {
    release_texture(wrap(handle.texture));
  }
  for (auto &handle : pool_) {
    handle.free();
  }
}

Texture *VKTexturePool::acquire_texture(int2 extent, TextureFormat format, eGPUTextureUsage usage)
{
  VKDevice &device = VKBackend::get().device;

  /* Create texture object with no backing allocation, wrapped in `TextureHandle`. */
  char name[16] = "TexFromPool";
  if (G.debug & G_DEBUG_GPU) {
    int texture_id = acquired_.size();
    SNPRINTF(name, "TexFromPool_%d", texture_id);
  }
  TextureHandle texture_handle;
  texture_handle.alloc(extent, format, usage, name);

  /* Query the requirements for this specific image */
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(
      device.vk_handle(), texture_handle.texture->vk_image_, &memory_requirements);

  /* TODO(not_mark): naive, but first compatible works better than smallest compatible. */
  int64_t match_index = -1;
  for (uint64_t i : pool_.index_range()) {
    const auto &handle = pool_[i];
    if (handle.allocation_info.size >= memory_requirements.size) {
      /* `memory_requirements.memoryTypeBits` has bits set for every type of supported memory;
       * only one needs to match for the allocation to be compatible to the image. */
      if (handle.allocation_info.memoryType == 0 ||
          bool(handle.allocation_info.memoryType & memory_requirements.memoryTypeBits))
      {
        match_index = i;
        break;
      }
    }
  }

  /* Acquire the compatible allocation, or allocate as a last resort. */
  AllocationHandle &allocation_handle = texture_handle.allocation_handle;
  if (match_index != -1) {
    allocation_handle = pool_[match_index];
    pool_.remove_and_reorder(match_index);
  }
  else {
    allocation_handle.alloc(memory_requirements);
  }

  /* Bind VkImage to allocation. */
  vmaBindImageMemory(
      device.mem_allocator_get(), allocation_handle.allocation, texture_handle.texture->vk_image_);
  debug::object_label(texture_handle.texture->vk_image_, texture_handle.texture->name_);
  device.resources.add_image(
      texture_handle.texture->vk_image_, false, texture_handle.texture->name_);

  acquired_.add(texture_handle);
  return wrap(texture_handle.texture);
}

void VKTexturePool::release_texture(Texture *tex)
{
  BLI_assert_msg(acquired_.contains({unwrap(tex)}),
                 "Unacquired texture passed to VKTexturePool::offset_users_count()");
  auto texture_handle = acquired_.lookup_key({unwrap(tex)});

  /* Move allocation back to `pool_`. */
  AllocationHandle allocation_handle = texture_handle.allocation_handle;
  allocation_handle.unused_cycles_count = 0;
  pool_.append(allocation_handle);

  /* Clear out acquired texture object. */
  acquired_.remove(texture_handle);
  texture_handle.free();
}

void VKTexturePool::offset_users_count(Texture *tex, int offset)
{
  BLI_assert_msg(acquired_.contains({unwrap(tex)}),
                 "Unacquired texture passed to VKTexturePool::offset_users_count()");
  auto texture_handle = acquired_.lookup_key({unwrap(tex)});
  texture_handle.users_count += offset;
  acquired_.add_overwrite(texture_handle);
}

void VKTexturePool::reset(bool force_free)
{
#ifndef NDEBUG
  /* Iterate acquired textures, and ensure the internal counter equals 0; otherwise
   * this indicates a missing `::retain()` or `::release()`. */
  for (const TextureHandle &tex : acquired_) {
    BLI_assert_msg(tex.users_count == 0,
                   "Missing texture release/retain. Likely TextureFromPool::release(), "
                   "TextureFromPool::retain() or TexturePool::release_texture().");
  }
#endif

  /* Reverse iterate unused allocations, to make sure we only reorder known good handles. */
  for (int i = pool_.size() - 1; i >= 0; i--) {
    AllocationHandle &handle = pool_[i];
    if (handle.unused_cycles_count >= max_unused_cycles_ || force_free) {
      handle.free();
      pool_.remove_and_reorder(i);
    }
    else {
      handle.unused_cycles_count++;
    }
  }
}

}  // namespace blender::gpu
