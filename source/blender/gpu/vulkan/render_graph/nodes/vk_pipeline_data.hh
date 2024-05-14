/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

namespace blender::gpu::render_graph {
class VKCommandBufferInterface;

/**
 * Container for storing shader descriptor set and push constants.
 *
 * Compute and graphic shaders use the same structure to setup the pipeline for execution.
 */
struct VKPipelineData {
  VkPipeline vk_pipeline;
  VkPipelineLayout vk_pipeline_layout;
  VkDescriptorSet vk_descriptor_set;
  uint32_t push_constants_size;
  const void *push_constants_data;
};

/** Resources bound for a compute/graphics pipeline. */
struct VKBoundPipeline {
  VkPipeline vk_pipeline;
  VkDescriptorSet vk_descriptor_set;
};

/**
 * Vulkan keeps track of bound resources for graphics separate from compute.
 * This struct store last bound resources for both bind points.
 */
struct VKBoundPipelines {
  /** Last bound resources for compute pipeline. */
  VKBoundPipeline compute;
  /** Last bound resources for graphics pipeline. */
  VKBoundPipeline graphics;
};

/**
 * Copy src pipeline data into dst. The push_constant_data will be duplicated and needs to be freed
 * using `vk_pipeline_data_free`.
 *
 * Memory duplication isn't used as push_constant_data in the src doesn't need to be allocated via
 * guardedalloc.
 */
void vk_pipeline_data_copy(VKPipelineData &dst, const VKPipelineData &src);

/**
 * Record the commands to the given command buffer to bind the descriptor set, pipeline and push
 * constants.
 *
 * Descriptor set and pipeline are only bound, when they are different than the last bound. The
 * r_bound_pipelines are checked to identify if they are the last bound. Descriptor set and
 * pipeline are bound at the given pipeline bind point.
 *
 * Any available push constants in the pipeline data always update the shader stages provided by
 * `vk_shader_stage_flags`.
 */
void vk_pipeline_data_build_commands(VKCommandBufferInterface &command_buffer,
                                     const VKPipelineData &pipeline_data,
                                     VKBoundPipelines &r_bound_pipelines,
                                     VkPipelineBindPoint vk_pipeline_bind_point,
                                     VkShaderStageFlags vk_shader_stage_flags);

/**
 * Free localized data created by `vk_pipeline_data_copy`.
 */
void vk_pipeline_data_free(VKPipelineData &data);

}  // namespace blender::gpu::render_graph
