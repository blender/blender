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

struct VKImageViewInfo {
  eImageViewUsage usage;
  IndexRange layer_range;
  IndexRange mip_range;
  char swizzle[4];
  bool use_stencil;
  bool use_srgb;

  bool operator==(const VKImageViewInfo &other) const
  {
    return usage == other.usage && layer_range == other.layer_range &&
           mip_range == other.mip_range && strncmp(swizzle, other.swizzle, sizeof(swizzle)) &&
           use_stencil == other.use_stencil && use_srgb == other.use_srgb;
  }
};

class VKImageView : NonCopyable {
  VkImageView vk_image_view_ = VK_NULL_HANDLE;
  VkFormat vk_format_ = VK_FORMAT_UNDEFINED;

 public:
  const VKImageViewInfo info;

  VKImageView(VKTexture &texture, const VKImageViewInfo &info, StringRefNull name);

  VKImageView(VKImageView &&other);
  ~VKImageView();

  VkImageView vk_handle() const
  {
    BLI_assert(vk_image_view_ != VK_NULL_HANDLE);
    return vk_image_view_;
  }

  VkFormat vk_format() const
  {
    return vk_format_;
  }
};

}  // namespace blender::gpu
