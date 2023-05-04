/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <optional>

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_descriptor_set.hh"
#include "vk_push_constants.hh"

namespace blender::gpu {
class VKContext;

class VKPipeline : NonCopyable {
  VkPipeline vk_pipeline_ = VK_NULL_HANDLE;
  VKDescriptorSetTracker descriptor_set_;
  VKPushConstants push_constants_;

 public:
  VKPipeline() = default;

  virtual ~VKPipeline();
  VKPipeline(VkPipeline vk_pipeline,
             VkDescriptorSetLayout vk_descriptor_set_layout,
             VKPushConstants &&push_constants);
  VKPipeline &operator=(VKPipeline &&other)
  {
    vk_pipeline_ = other.vk_pipeline_;
    other.vk_pipeline_ = VK_NULL_HANDLE;
    descriptor_set_ = std::move(other.descriptor_set_);
    push_constants_ = std::move(other.push_constants_);
    return *this;
  }

  static VKPipeline create_compute_pipeline(VkShaderModule compute_module,
                                            VkDescriptorSetLayout &descriptor_set_layout,
                                            VkPipelineLayout &pipeline_layouts,
                                            const VKPushConstants::Layout &push_constants_layout);

  VKDescriptorSetTracker &descriptor_set_get()
  {
    return descriptor_set_;
  }

  VKPushConstants &push_constants_get()
  {
    return push_constants_;
  }

  VkPipeline vk_handle() const;
  bool is_valid() const;
};

}  // namespace blender::gpu
