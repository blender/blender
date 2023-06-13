/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <optional>

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_descriptor_set.hh"
#include "vk_pipeline_state.hh"
#include "vk_push_constants.hh"

namespace blender::gpu {
class VKContext;
class VKShader;
class VKVertexAttributeObject;
class VKBatch;

/**
 * Pipeline can be a compute pipeline or a graphic pipeline.
 *
 * Compute pipelines can be constructed early on, but graphics
 * pipelines depends on the actual GPU state/context.
 *
 * - TODO: we should sanitize the interface. There we can also
 *   use late construction for compute pipelines.
 */
class VKPipeline : NonCopyable {
  /* Active pipeline handle. */
  VkPipeline active_vk_pipeline_ = VK_NULL_HANDLE;
  /** Keep track of all pipelines as they can still be in flight. */
  Vector<VkPipeline> vk_pipelines_;
  VKDescriptorSetTracker descriptor_set_;
  VKPushConstants push_constants_;
  VKPipelineStateManager state_manager_;

 public:
  VKPipeline() = default;

  virtual ~VKPipeline();
  VKPipeline(VkDescriptorSetLayout vk_descriptor_set_layout, VKPushConstants &&push_constants);
  VKPipeline(VkPipeline vk_pipeline,
             VkDescriptorSetLayout vk_descriptor_set_layout,
             VKPushConstants &&push_constants);
  VKPipeline &operator=(VKPipeline &&other)
  {
    active_vk_pipeline_ = other.active_vk_pipeline_;
    other.active_vk_pipeline_ = VK_NULL_HANDLE;
    descriptor_set_ = std::move(other.descriptor_set_);
    push_constants_ = std::move(other.push_constants_);
    vk_pipelines_ = std::move(other.vk_pipelines_);
    other.vk_pipelines_.clear();
    return *this;
  }

  static VKPipeline create_compute_pipeline(VkShaderModule compute_module,
                                            VkDescriptorSetLayout &descriptor_set_layout,
                                            VkPipelineLayout &pipeline_layouts,
                                            const VKPushConstants::Layout &push_constants_layout);
  static VKPipeline create_graphics_pipeline(VkDescriptorSetLayout &descriptor_set_layout,
                                             const VKPushConstants::Layout &push_constants_layout);

  VKDescriptorSetTracker &descriptor_set_get()
  {
    return descriptor_set_;
  }

  VKPushConstants &push_constants_get()
  {
    return push_constants_;
  }

  VKPipelineStateManager &state_manager_get()
  {
    return state_manager_;
  }

  VkPipeline vk_handle() const;
  bool is_valid() const;

  void finalize(VKContext &context,
                VkShaderModule vertex_module,
                VkShaderModule geometry_module,
                VkShaderModule fragment_module,
                VkPipelineLayout &pipeline_layout,
                const GPUPrimType prim_type,
                const VKVertexAttributeObject &vertex_attribute_object);

  /**
   * Update PushConstants, DescriptorSets and bind pipeline to command buffer.
   */
  void update_and_bind(VKContext &context,
                       VkPipelineLayout vk_pipeline_layout,
                       VkPipelineBindPoint vk_pipeline_bind_point);
};

}  // namespace blender::gpu
