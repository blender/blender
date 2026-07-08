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
#include "vk_texture.hh"

namespace blender::gpu {

VKImageView::VKImageView(VKTexture &texture, const VKImageViewInfo &info, StringRefNull name)
    : info(info)
{
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = texture.vk_image_handle();
  image_view_info.viewType = to_vk_image_view_type(texture.type_get(), info.usage, info.arrayed);
  image_view_info.format = info.vk_format;
  image_view_info.components.r = to_vk_component_swizzle(info.swizzle[0]);
  image_view_info.components.g = to_vk_component_swizzle(info.swizzle[1]);
  image_view_info.components.b = to_vk_component_swizzle(info.swizzle[2]);
  image_view_info.components.a = to_vk_component_swizzle(info.swizzle[3]);
  image_view_info.subresourceRange.aspectMask = info.vk_image_aspects;
  image_view_info.subresourceRange.baseMipLevel = info.mip_range.first();
  image_view_info.subresourceRange.levelCount = info.mip_range.size();
  image_view_info.subresourceRange.baseArrayLayer = info.layer_range.first();
  image_view_info.subresourceRange.layerCount = info.layer_range.size();

  const VKDevice &device = VKBackend::get().device;
  device.functions.vkCreateImageView(
      device.vk_handle(), &image_view_info, nullptr, &vk_image_view_);
  debug::object_label(vk_image_view_, name.c_str());
}

VKImageView::VKImageView(VKImageView &&other) : info(other.info)
{
  vk_image_view_ = other.vk_image_view_;
  other.vk_image_view_ = VK_NULL_HANDLE;
  vk_format_ = other.vk_format_;
  other.vk_format_ = VK_FORMAT_UNDEFINED;
}

VKImageView::~VKImageView()
{
  if (vk_image_view_ != VK_NULL_HANDLE) {
    VKDiscardPool::discard_pool_get().discard_image_view(vk_image_view_);
    vk_image_view_ = VK_NULL_HANDLE;
  }
  vk_format_ = VK_FORMAT_UNDEFINED;
}

}  // namespace blender::gpu
