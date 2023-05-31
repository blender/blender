/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_private.hh"

#include "vk_common.hh"

#include "BLI_utility_mixins.hh"

namespace blender::gpu {
class VKContext;

class VKSampler : public NonCopyable {
  VkSampler vk_sampler_ = VK_NULL_HANDLE;

 public:
  virtual ~VKSampler();
  void create();
  void free();

  VkSampler vk_handle()
  {
    BLI_assert(vk_sampler_ != VK_NULL_HANDLE);
    return vk_sampler_;
  }
};

}  // namespace blender::gpu
