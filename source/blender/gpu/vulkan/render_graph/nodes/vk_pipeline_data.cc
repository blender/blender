/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "render_graph/nodes/vk_pipeline_data.hh"
#include "render_graph/vk_command_buffer_wrapper.hh"

namespace blender::gpu::render_graph {
void vk_pipeline_data_copy(VKPipelineData &dst, const VKPipelineData &src)
{
  dst.push_constants_data = nullptr;
  dst.push_constants_size = src.push_constants_size;
  if (src.push_constants_size) {
    BLI_assert(src.push_constants_data);
    void *data = MEM_mallocN(src.push_constants_size, __func__);
    memcpy(data, src.push_constants_data, src.push_constants_size);
    dst.push_constants_data = data;
  }
}

void vk_pipeline_data_build_commands(VKCommandBufferInterface &command_buffer,
                                     const VKPipelineData &pipeline_data,
                                     VKBoundPipelines &r_bound_pipelines,
                                     VkPipelineBindPoint vk_pipeline_bind_point)
{
  if (assign_if_different(r_bound_pipelines.compute.vk_pipeline, pipeline_data.vk_pipeline)) {
    command_buffer.bind_pipeline(vk_pipeline_bind_point, r_bound_pipelines.compute.vk_pipeline);
  }

  if (assign_if_different(r_bound_pipelines.compute.vk_descriptor_set,
                          pipeline_data.vk_descriptor_set))
  {
    command_buffer.bind_descriptor_sets(vk_pipeline_bind_point,
                                        pipeline_data.vk_pipeline_layout,
                                        0,
                                        1,
                                        &r_bound_pipelines.compute.vk_descriptor_set,
                                        0,
                                        nullptr);
  }

  if (pipeline_data.push_constants_size) {
    command_buffer.push_constants(pipeline_data.vk_pipeline_layout,
                                  vk_pipeline_bind_point,
                                  0,
                                  pipeline_data.push_constants_size,
                                  pipeline_data.push_constants_data);
  }
}

void vk_pipeline_data_free(VKPipelineData &data)
{
  MEM_SAFE_FREE(data.push_constants_data);
}

}  // namespace blender::gpu::render_graph
