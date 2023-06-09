/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
  VKImageView(VKTexture &texture, int mip_level, StringRefNull name);

  /**
   * Wrap the given vk_image_view handle. Note that the vk_image_view handle ownership is
   * transferred to VKImageView.
   */
  VKImageView(VkImageView vk_image_view);

  VKImageView(VKImageView &&other);
  ~VKImageView();

  VkImageView vk_handle() const
  {
    BLI_assert(vk_image_view_ != VK_NULL_HANDLE);
    return vk_image_view_;
  }

 private:
  static VkImageView create_vk_image_view(VKTexture &texture, int mip_level, StringRefNull name);
};

}  // namespace blender::gpu
