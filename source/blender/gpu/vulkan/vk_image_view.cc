/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_image_view.hh"
#include "vk_backend.hh"
#include "vk_debug.hh"
#include "vk_device.hh"
#include "vk_memory.hh"
#include "vk_texture.hh"

namespace blender::gpu {

VKImageView::VKImageView(VKTexture &texture, int layer, int mip_level, StringRefNull name)
    : vk_image_view_(create_vk_image_view(texture, layer, mip_level, name))
{
  BLI_assert(vk_image_view_ != VK_NULL_HANDLE);
}

VKImageView::VKImageView(VkImageView vk_image_view) : vk_image_view_(vk_image_view)
{
  BLI_assert(vk_image_view_ != VK_NULL_HANDLE);
}

VKImageView::VKImageView(VKImageView &&other)
{
  vk_image_view_ = other.vk_image_view_;
  other.vk_image_view_ = VK_NULL_HANDLE;
}

VKImageView::~VKImageView()
{
  if (vk_image_view_ != VK_NULL_HANDLE) {
    VK_ALLOCATION_CALLBACKS
    const VKDevice &device = VKBackend::get().device_get();
    vkDestroyImageView(device.device_get(), vk_image_view_, vk_allocation_callbacks);
  }
}
VkImageView VKImageView::create_vk_image_view(VKTexture &texture,
                                              int layer,
                                              int mip_level,
                                              StringRefNull name)
{

  VK_ALLOCATION_CALLBACKS
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = texture.vk_image_handle();
  image_view_info.viewType = to_vk_image_view_type(texture.type_get(),
                                                   eImageViewUsage::Attachment);
  image_view_info.format = to_vk_format(texture.format_get());
  image_view_info.components = to_vk_component_mapping(texture.format_get());
  image_view_info.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(texture.format_get());
  image_view_info.subresourceRange.baseMipLevel = mip_level;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = layer == -1 ? 0 : layer;
  image_view_info.subresourceRange.layerCount = 1;

  const VKDevice &device = VKBackend::get().device_get();
  VkImageView image_view = VK_NULL_HANDLE;
  vkCreateImageView(device.device_get(), &image_view_info, vk_allocation_callbacks, &image_view);
  debug::object_label(image_view, name.c_str());
  return image_view;
}

}  // namespace blender::gpu
