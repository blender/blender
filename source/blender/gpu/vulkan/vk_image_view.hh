/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

namespace blender::gpu {
class VKTexture;

class VKImageView : NonCopyable {
  VkImageView vk_image_view_ = VK_NULL_HANDLE;

 public:
  VKImageView(VKTexture &texture,
              eImageViewUsage usage,
              IndexRange layer_range,
              IndexRange mip_range,
              StringRefNull name);

  VKImageView(VKImageView &&other);
  ~VKImageView();

  VkImageView vk_handle() const
  {
    BLI_assert(vk_image_view_ != VK_NULL_HANDLE);
    return vk_image_view_;
  }
};

}  // namespace blender::gpu
