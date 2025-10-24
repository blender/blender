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
struct VKRenderGraphNodeLinks;
class VKResourceStateTracker;

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

/**
 * Container for storing viewport and scissor data used for
 * draw nodes.
 */
struct VKViewportData {
  Vector<VkViewport> viewports;
  Vector<VkRect2D> scissors;

  bool operator==(const VKViewportData &other) const
  {
    if (viewports.size() != other.viewports.size() && scissors.size() != other.scissors.size()) {
      return false;
    }

    if (memcmp(viewports.data(), other.viewports.data(), viewports.size() * sizeof(VkViewport)) !=
        0)
    {
      return false;
    }

    if (memcmp(scissors.data(), other.scissors.data(), scissors.size() * sizeof(VkRect2D)) != 0) {
      return false;
    }

    return true;
  }

  bool operator!=(const VKViewportData &other) const
  {
    return !(*this == other);
  }
};

struct VKPipelineDataGraphics {
  VKPipelineData pipeline_data;
  VKViewportData viewport;
  std::optional<float> line_width;
};

/** Resources bound for a compute/graphics pipeline. */
struct VKBoundPipeline {
  VkPipeline vk_pipeline;
  VkDescriptorSet vk_descriptor_set;
};

struct VKIndexBufferBinding {
  VkBuffer buffer;
  VkIndexType index_type;

  bool operator==(const VKIndexBufferBinding &other) const
  {
    return buffer == other.buffer && index_type == other.index_type;
  }
  bool operator!=(const VKIndexBufferBinding &other) const
  {
    return !(*this == other);
  }
};

struct VKVertexBufferBindings {
  uint32_t buffer_count;
  VkBuffer buffer[16];
  VkDeviceSize offset[16];

  bool operator==(const VKVertexBufferBindings &other) const
  {
    return buffer_count == other.buffer_count &&
           Span<VkBuffer>(buffer, buffer_count) ==
               Span<VkBuffer>(other.buffer, other.buffer_count) &&
           Span<VkDeviceSize>(offset, buffer_count) ==
               Span<VkDeviceSize>(other.offset, buffer_count);
  }
  bool operator!=(const VKVertexBufferBindings &other) const
  {
    return !(*this == other);
  }
};

/**
 * Vulkan keeps track of bound resources for graphics separate from compute.
 * This struct store last bound resources for both bind points.
 */
struct VKBoundPipelines {
  /** Last bound resources for compute pipeline. */
  VKBoundPipeline compute;
  /** Last bound resources for graphics pipeline. */
  struct {
    VKBoundPipeline pipeline;
    VKIndexBufferBinding index_buffer;
    VKVertexBufferBindings vertex_buffers;
    VKViewportData viewport_state;
    std::optional<float> line_width;
  } graphics;
};

/**
 * Copy src pipeline data into dst. The push_constant_data will be duplicated and needs to be freed
 * using `vk_pipeline_data_free`.
 *
 * Memory duplication isn't used as push_constant_data in the src doesn't need to be allocated via
 * guardedalloc.
 */
void vk_pipeline_data_copy(VKPipelineData &dst, const VKPipelineData &src);
static inline void vk_pipeline_data_copy(VKPipelineDataGraphics &dst,
                                         const VKPipelineDataGraphics &src)
{
  vk_pipeline_data_copy(dst.pipeline_data, src.pipeline_data);
}

/**
 * Record commands that update the dynamic state.
 *
 * - viewports
 * - scissors
 * - line width
 */
void vk_pipeline_dynamic_graphics_build_commands(VKCommandBufferInterface &command_buffer,
                                                 const VKViewportData &viewport,
                                                 const std::optional<float> line_width,
                                                 VKBoundPipelines &r_bound_pipelines);

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
                                     VKBoundPipeline &r_bound_pipeline,
                                     VkPipelineBindPoint vk_pipeline_bind_point,
                                     VkShaderStageFlags vk_shader_stage_flags);

/**
 * Free localized data created by `vk_pipeline_data_copy`.
 */
void vk_pipeline_data_free(VKPipelineData &data);
static inline void vk_pipeline_data_free(VKPipelineDataGraphics &data)
{
  vk_pipeline_data_free(data.pipeline_data);
}

void vk_index_buffer_binding_build_links(VKResourceStateTracker &resources,
                                         VKRenderGraphNodeLinks &node_links,
                                         const VKIndexBufferBinding &index_buffer_binding);
void vk_index_buffer_binding_build_commands(VKCommandBufferInterface &command_buffer,
                                            const VKIndexBufferBinding &index_buffer_binding,
                                            VKIndexBufferBinding &r_bound_index_buffer);
void vk_vertex_buffer_bindings_build_links(VKResourceStateTracker &resources,
                                           VKRenderGraphNodeLinks &node_links,
                                           const VKVertexBufferBindings &vertex_buffer_bindings);
void vk_vertex_buffer_bindings_build_commands(VKCommandBufferInterface &command_buffer,
                                              const VKVertexBufferBindings &vertex_buffer_bindings,
                                              VKVertexBufferBindings &r_bound_vertex_buffers);

}  // namespace blender::gpu::render_graph
