/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_pipeline.hh"
#include "vk_backend.hh"
#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_memory.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_attribute_object.hh"

namespace blender::gpu {

VKPipeline::VKPipeline(VkDescriptorSetLayout vk_descriptor_set_layout,
                       VKPushConstants &&push_constants)
    : active_vk_pipeline_(VK_NULL_HANDLE),
      descriptor_set_(vk_descriptor_set_layout),
      push_constants_(std::move(push_constants))
{
}

VKPipeline::VKPipeline(VkPipeline vk_pipeline,
                       VkDescriptorSetLayout vk_descriptor_set_layout,
                       VKPushConstants &&push_constants)
    : active_vk_pipeline_(vk_pipeline),
      descriptor_set_(vk_descriptor_set_layout),
      push_constants_(std::move(push_constants))
{
  vk_pipelines_.append(vk_pipeline);
}

VKPipeline::~VKPipeline()
{
  VK_ALLOCATION_CALLBACKS
  const VKDevice &device = VKBackend::get().device_get();
  for (VkPipeline vk_pipeline : vk_pipelines_) {
    vkDestroyPipeline(device.device_get(), vk_pipeline, vk_allocation_callbacks);
  }
}

VKPipeline VKPipeline::create_compute_pipeline(
    VkShaderModule compute_module,
    VkDescriptorSetLayout &descriptor_set_layout,
    VkPipelineLayout &pipeline_layout,
    const VKPushConstants::Layout &push_constants_layout)
{
  VK_ALLOCATION_CALLBACKS
  const VKDevice &device = VKBackend::get().device_get();
  VkComputePipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.flags = 0;
  pipeline_info.stage = {};
  pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stage.flags = 0;
  pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_info.stage.module = compute_module;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.stage.pName = "main";

  VkPipeline vk_pipeline;
  if (vkCreateComputePipelines(device.device_get(),
                               nullptr,
                               1,
                               &pipeline_info,
                               vk_allocation_callbacks,
                               &vk_pipeline) != VK_SUCCESS)
  {
    return VKPipeline();
  }

  VKPushConstants push_constants(&push_constants_layout);
  return VKPipeline(vk_pipeline, descriptor_set_layout, std::move(push_constants));
}

VKPipeline VKPipeline::create_graphics_pipeline(
    VkDescriptorSetLayout &descriptor_set_layout,
    const VKPushConstants::Layout &push_constants_layout)
{
  VKPushConstants push_constants(&push_constants_layout);
  return VKPipeline(descriptor_set_layout, std::move(push_constants));
}

VkPipeline VKPipeline::vk_handle() const
{
  return active_vk_pipeline_;
}

bool VKPipeline::is_valid() const
{
  return active_vk_pipeline_ != VK_NULL_HANDLE;
}

void VKPipeline::finalize(VKContext &context,
                          VkShaderModule vertex_module,
                          VkShaderModule geometry_module,
                          VkShaderModule fragment_module,
                          VkPipelineLayout &pipeline_layout,
                          const GPUPrimType prim_type,
                          const VKVertexAttributeObject &vertex_attribute_object)
{
  BLI_assert(vertex_module != VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS

  Vector<VkPipelineShaderStageCreateInfo> pipeline_stages;
  VkPipelineShaderStageCreateInfo vertex_stage_info = {};
  vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage_info.module = vertex_module;
  vertex_stage_info.pName = "main";
  pipeline_stages.append(vertex_stage_info);

  if (geometry_module != VK_NULL_HANDLE) {
    VkPipelineShaderStageCreateInfo geometry_stage_info = {};
    geometry_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    geometry_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    geometry_stage_info.module = geometry_module;
    geometry_stage_info.pName = "main";
    pipeline_stages.append(geometry_stage_info);
  }

  if (fragment_module != VK_NULL_HANDLE) {
    VkPipelineShaderStageCreateInfo fragment_stage_info = {};
    fragment_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage_info.module = fragment_module;
    fragment_stage_info.pName = "main";
    pipeline_stages.append(fragment_stage_info);
  }

  VKFrameBuffer &framebuffer = *context.active_framebuffer_get();

  VkGraphicsPipelineCreateInfo pipeline_create_info = {};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.stageCount = pipeline_stages.size();
  pipeline_create_info.pStages = pipeline_stages.data();
  pipeline_create_info.layout = pipeline_layout;
  pipeline_create_info.renderPass = framebuffer.vk_render_pass_get();
  pipeline_create_info.subpass = 0;

  /* Vertex input state. */
  VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
  vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state.vertexBindingDescriptionCount = vertex_attribute_object.bindings.size();
  vertex_input_state.pVertexBindingDescriptions = vertex_attribute_object.bindings.data();
  vertex_input_state.vertexAttributeDescriptionCount = vertex_attribute_object.attributes.size();
  vertex_input_state.pVertexAttributeDescriptions = vertex_attribute_object.attributes.data();
  pipeline_create_info.pVertexInputState = &vertex_input_state;

  /* Input assembly state. */
  VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly = {};
  pipeline_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  pipeline_input_assembly.topology = to_vk_primitive_topology(prim_type);
  pipeline_input_assembly.primitiveRestartEnable =
      ELEM(prim_type, GPU_PRIM_TRIS, GPU_PRIM_LINES, GPU_PRIM_POINTS) ? VK_FALSE : VK_TRUE;
  pipeline_create_info.pInputAssemblyState = &pipeline_input_assembly;

  /* Viewport state. */
  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VkViewport viewport = framebuffer.vk_viewport_get();
  viewport_state.pViewports = &viewport;
  viewport_state.viewportCount = 1;
  VkRect2D scissor = framebuffer.vk_render_area_get();
  viewport_state.pScissors = &scissor;
  viewport_state.scissorCount = 1;
  pipeline_create_info.pViewportState = &viewport_state;

  /* Multi-sample state. */
  VkPipelineMultisampleStateCreateInfo multisample_state = {};
  multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample_state.minSampleShading = 1.0f;
  pipeline_create_info.pMultisampleState = &multisample_state;

  /* States from the state manager. */
  const VKPipelineStateManager &state_manager = state_manager_get();
  pipeline_create_info.pColorBlendState = &state_manager.pipeline_color_blend_state;
  pipeline_create_info.pRasterizationState = &state_manager.rasterization_state;
  pipeline_create_info.pDepthStencilState = &state_manager.depth_stencil_state;

  const VKDevice &device = VKBackend::get().device_get();
  vkCreateGraphicsPipelines(device.device_get(),
                            VK_NULL_HANDLE,
                            1,
                            &pipeline_create_info,
                            vk_allocation_callbacks,
                            &active_vk_pipeline_);
  /* TODO: we should cache several pipeline instances and detect pipelines we can reuse. This might
   * also be done using a VkPipelineCache. For now we just destroy any available pipeline so it
   * won't be overwritten by the newly created one. */
  vk_pipelines_.append(active_vk_pipeline_);
  debug::object_label(active_vk_pipeline_, "GraphicsPipeline");
}

void VKPipeline::update_and_bind(VKContext &context,
                                 VkPipelineLayout vk_pipeline_layout,
                                 VkPipelineBindPoint vk_pipeline_bind_point)
{
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.bind(*this, vk_pipeline_bind_point);
  push_constants_.update(context);
  if (descriptor_set_.has_layout()) {
    descriptor_set_.update(context);
    command_buffer.bind(
        *descriptor_set_.active_descriptor_set(), vk_pipeline_layout, vk_pipeline_bind_point);
  }
}

}  // namespace blender::gpu
