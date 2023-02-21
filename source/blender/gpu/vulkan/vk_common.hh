/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

#include "gpu_texture_private.hh"

namespace blender::gpu {

VkImageAspectFlagBits to_vk_image_aspect_flag_bits(const eGPUTextureFormat format);
VkFormat to_vk_format(const eGPUTextureFormat format);
VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat format);
VkImageViewType to_vk_image_view_type(const eGPUTextureType type);
VkImageType to_vk_image_type(const eGPUTextureType type);

}  // namespace blender::gpu
