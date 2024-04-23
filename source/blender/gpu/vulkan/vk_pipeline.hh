/* SPDX-FileCopyrightText: 2023 Blender Authors
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
  VKPipelineStateManager state_manager_;

 public:
  VKPipeline() = default;

  virtual ~VKPipeline();
  VKPipeline(VkPipeline vk_pipeline);
  VKPipeline &operator=(VKPipeline &&other)
  {
    active_vk_pipeline_ = other.active_vk_pipeline_;
    other.active_vk_pipeline_ = VK_NULL_HANDLE;
    vk_pipelines_ = std::move(other.vk_pipelines_);
    other.vk_pipelines_.clear();
    return *this;
  }

  static VKPipeline create_compute_pipeline(VkShaderModule compute_module,
                                            VkPipelineLayout &pipeline_layouts);
  static VKPipeline create_graphics_pipeline();

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

  void bind(VKContext &context, VkPipelineBindPoint vk_pipeline_bind_point);
};

}  // namespace blender::gpu
