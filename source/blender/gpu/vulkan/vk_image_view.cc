/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_image_view.hh"
#include "vk_backend.hh"
#include "vk_common.hh"
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

  /* When extended usage is enabled for storage images, it means the original format
   * is not supported. So we must strip USAGE_STORAGE for the image view that has the
   * same format as the image, and only leave it for the other image view with the
   * format that will be used for storage. */
  VkImageViewUsageCreateInfo view_usage_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
  if (vk_need_extended_usage_for_storage_image(texture.usage_get(), texture.format_flag_get()) &&
      info.vk_format == to_vk_format(texture.device_format_get()))
  {
    view_usage_info.usage = texture.vk_image_usage_get() & ~VK_IMAGE_USAGE_STORAGE_BIT;
    BLI_assert(view_usage_info.usage != 0);
    view_usage_info.pNext = image_view_info.pNext;
    image_view_info.pNext = &view_usage_info;
  }

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
