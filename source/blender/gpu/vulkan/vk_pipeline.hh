/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_descriptor_set.hh"

namespace blender::gpu {
class VKContext;

class VKPipeline : NonCopyable {
  VKDescriptorSet descriptor_set_;
  VkPipeline vk_pipeline_ = VK_NULL_HANDLE;

 public:
  VKPipeline() = default;

  virtual ~VKPipeline();
  VKPipeline(VkPipeline vk_pipeline, VKDescriptorSet &&vk_descriptor_set);
  VKPipeline &operator=(VKPipeline &&other)
  {
    vk_pipeline_ = other.vk_pipeline_;
    other.vk_pipeline_ = VK_NULL_HANDLE;
    descriptor_set_ = std::move(other.descriptor_set_);
    return *this;
  }

  static VKPipeline create_compute_pipeline(VKContext &context,
                                            VkShaderModule compute_module,
                                            VkDescriptorSetLayout &descriptor_set_layout,
                                            VkPipelineLayout &pipeline_layouts);

  VKDescriptorSet &descriptor_set_get()
  {
    return descriptor_set_;
  }

  VkPipeline vk_handle() const;
  bool is_valid() const;
};

}  // namespace blender::gpu
