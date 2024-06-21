/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_pipeline_pool.hh"
#include "vk_backend.hh"
#include "vk_memory.hh"

namespace blender::gpu {

VKPipelinePool::VKPipelinePool()
{
  /* Initialize VkComputePipelineCreateInfo*/
  vk_compute_pipeline_create_info_.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  vk_compute_pipeline_create_info_.pNext = nullptr;
  vk_compute_pipeline_create_info_.flags = 0;
  vk_compute_pipeline_create_info_.layout = VK_NULL_HANDLE;
  vk_compute_pipeline_create_info_.basePipelineHandle = VK_NULL_HANDLE;
  vk_compute_pipeline_create_info_.basePipelineIndex = 0;
  VkPipelineShaderStageCreateInfo &vk_pipeline_shader_stage_create_info =
      vk_compute_pipeline_create_info_.stage;
  vk_pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vk_pipeline_shader_stage_create_info.pNext = nullptr;
  vk_pipeline_shader_stage_create_info.flags = 0;
  vk_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  vk_pipeline_shader_stage_create_info.module = VK_NULL_HANDLE;
  vk_pipeline_shader_stage_create_info.pName = "main";

  /* Initialize VkGraphicsPipelineCreateInfo */
  vk_graphics_pipeline_create_info_ = {};
  vk_graphics_pipeline_create_info_.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  vk_graphics_pipeline_create_info_.pNext = &vk_pipeline_rendering_create_info_;
  vk_graphics_pipeline_create_info_.stageCount = 0;
  vk_graphics_pipeline_create_info_.pStages = vk_pipeline_shader_stage_create_info_;
  vk_graphics_pipeline_create_info_.pInputAssemblyState =
      &vk_pipeline_input_assembly_state_create_info_;
  vk_graphics_pipeline_create_info_.pVertexInputState =
      &vk_pipeline_vertex_input_state_create_info_;
  vk_graphics_pipeline_create_info_.pRasterizationState =
      &vk_pipeline_rasterization_state_create_info_;
  vk_graphics_pipeline_create_info_.pViewportState = &vk_pipeline_viewport_state_create_info_;
  vk_graphics_pipeline_create_info_.pMultisampleState =
      &vk_pipeline_multisample_state_create_info_;
  vk_graphics_pipeline_create_info_.pColorBlendState = &vk_pipeline_color_blend_state_create_info_;

  /* Initialize VkPipelineRenderingCreateInfo */
  vk_pipeline_rendering_create_info_ = {};
  vk_pipeline_rendering_create_info_.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

  /* Initialize VkPipelineShaderStageCreateInfo */
  for (int i : IndexRange(3)) {
    vk_pipeline_shader_stage_create_info_[i].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vk_pipeline_shader_stage_create_info_[i].pNext = nullptr;
    vk_pipeline_shader_stage_create_info_[i].flags = 0;
    vk_pipeline_shader_stage_create_info_[i].module = VK_NULL_HANDLE;
    vk_pipeline_shader_stage_create_info_[i].pName = "main";
  }
  vk_pipeline_shader_stage_create_info_[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  vk_pipeline_shader_stage_create_info_[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  vk_pipeline_shader_stage_create_info_[2].stage = VK_SHADER_STAGE_GEOMETRY_BIT;

  /* Initialize VkPipelineInputAssemblyStateCreateInfo */
  vk_pipeline_input_assembly_state_create_info_ = {};
  vk_pipeline_input_assembly_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

  /* Initialize VkPipelineVertexInputStateCreateInfo */
  vk_pipeline_vertex_input_state_create_info_ = {};
  vk_pipeline_vertex_input_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  /* Initialize VkPipelineRasterizationStateCreateInfo */
  vk_pipeline_rasterization_state_create_info_ = {};
  vk_pipeline_rasterization_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  vk_pipeline_rasterization_state_create_info_.lineWidth = 1.0f;
  vk_pipeline_rasterization_state_create_info_.frontFace = VK_FRONT_FACE_CLOCKWISE;

  vk_pipeline_viewport_state_create_info_ = {};
  vk_pipeline_viewport_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

  /* Initialize VkPipelineMultisampleStateCreateInfo */
  vk_pipeline_multisample_state_create_info_ = {};
  vk_pipeline_multisample_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  vk_pipeline_multisample_state_create_info_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  vk_pipeline_multisample_state_create_info_.minSampleShading = 1.0f;

  /* Initialize VkPipelineColorBlendStateCreateInfo */
  vk_pipeline_color_blend_state_create_info_ = {};
  vk_pipeline_color_blend_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  vk_pipeline_color_blend_attachment_state_template_.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                      VK_COLOR_COMPONENT_G_BIT |
                                                                      VK_COLOR_COMPONENT_B_BIT |
                                                                      VK_COLOR_COMPONENT_A_BIT;
  /* Initialize VkPipelineDepthStencilStateCreateInfo */
  vk_pipeline_depth_stencil_state_create_info_ = {};
  vk_pipeline_depth_stencil_state_create_info_.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

  /* Initialize VkSpecializationInfo. */
  vk_specialization_info_.mapEntryCount = 0;
  vk_specialization_info_.pMapEntries = nullptr;
  vk_specialization_info_.dataSize = 0;
  vk_specialization_info_.pData = nullptr;

  vk_push_constant_range_.stageFlags = 0;
  vk_push_constant_range_.offset = 0;
  vk_push_constant_range_.size = 0;
}

VkSpecializationInfo *VKPipelinePool::specialization_info_update(
    Span<shader::SpecializationConstant::Value> specialization_constants)
{
  if (specialization_constants.is_empty()) {
    return nullptr;
  }

  while (vk_specialization_map_entries_.size() < specialization_constants.size()) {
    uint32_t constant_id = vk_specialization_map_entries_.size();
    VkSpecializationMapEntry vk_specialization_map_entry = {};
    vk_specialization_map_entry.constantID = constant_id;
    vk_specialization_map_entry.offset = constant_id * sizeof(uint32_t);
    vk_specialization_map_entry.size = sizeof(uint32_t);
    vk_specialization_map_entries_.append(vk_specialization_map_entry);
  }
  vk_specialization_info_.dataSize = specialization_constants.size() * sizeof(uint32_t);
  vk_specialization_info_.pData = specialization_constants.data();
  vk_specialization_info_.mapEntryCount = specialization_constants.size();
  vk_specialization_info_.pMapEntries = vk_specialization_map_entries_.data();
  return &vk_specialization_info_;
}

void VKPipelinePool::specialization_info_reset()
{
  vk_specialization_info_.dataSize = 0;
  vk_specialization_info_.pData = nullptr;
  vk_specialization_info_.mapEntryCount = 0;
  vk_specialization_info_.pMapEntries = nullptr;
}

VkPipeline VKPipelinePool::get_or_create_compute_pipeline(VKComputeInfo &compute_info,
                                                          VkPipeline vk_pipeline_base)
{
  std::scoped_lock lock(mutex_);
  const VkPipeline *found_pipeline = compute_pipelines_.lookup_ptr(compute_info);
  if (found_pipeline) {
    VkPipeline result = *found_pipeline;
    BLI_assert(result != VK_NULL_HANDLE);
    return result;
  }

  vk_compute_pipeline_create_info_.layout = compute_info.vk_pipeline_layout;
  vk_compute_pipeline_create_info_.stage.module = compute_info.vk_shader_module;
  vk_compute_pipeline_create_info_.basePipelineHandle = vk_pipeline_base;
  vk_compute_pipeline_create_info_.stage.pSpecializationInfo = specialization_info_update(
      compute_info.specialization_constants);

  /* Build pipeline. */
  VKBackend &backend = VKBackend::get();
  VKDevice &device = backend.device;
  VK_ALLOCATION_CALLBACKS;

  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateComputePipelines(device.vk_handle(),
                           device.vk_pipeline_cache_get(),
                           1,
                           &vk_compute_pipeline_create_info_,
                           vk_allocation_callbacks,
                           &pipeline);
  compute_pipelines_.add(compute_info, pipeline);

  /* Reset values to initial value. */
  vk_compute_pipeline_create_info_.layout = VK_NULL_HANDLE;
  vk_compute_pipeline_create_info_.stage.module = VK_NULL_HANDLE;
  vk_compute_pipeline_create_info_.stage.pSpecializationInfo = nullptr;
  vk_compute_pipeline_create_info_.basePipelineHandle = VK_NULL_HANDLE;
  specialization_info_reset();

  return pipeline;
}

VkPipeline VKPipelinePool::get_or_create_graphics_pipeline(VKGraphicsInfo &graphics_info,
                                                           VkPipeline vk_pipeline_base)
{
  std::scoped_lock lock(mutex_);
  const VkPipeline *found_pipeline = graphic_pipelines_.lookup_ptr(graphics_info);
  if (found_pipeline) {
    VkPipeline result = *found_pipeline;
    BLI_assert(result != VK_NULL_HANDLE);
    return result;
  }

  /* Specialization constants */
  VkSpecializationInfo *specialization_info = specialization_info_update(
      graphics_info.specialization_constants);

  /* Shader stages */
  vk_graphics_pipeline_create_info_.stageCount =
      graphics_info.pre_rasterization.vk_geometry_module == VK_NULL_HANDLE ? 2 : 3;
  vk_pipeline_shader_stage_create_info_[0].module =
      graphics_info.pre_rasterization.vk_vertex_module;
  vk_pipeline_shader_stage_create_info_[0].pSpecializationInfo = specialization_info;
  vk_pipeline_shader_stage_create_info_[1].module =
      graphics_info.fragment_shader.vk_fragment_module;
  vk_pipeline_shader_stage_create_info_[1].pSpecializationInfo = specialization_info;
  vk_pipeline_shader_stage_create_info_[2].module =
      graphics_info.pre_rasterization.vk_geometry_module;
  vk_pipeline_shader_stage_create_info_[2].pSpecializationInfo = specialization_info;

  /* Input assembly */
  vk_pipeline_input_assembly_state_create_info_.topology = graphics_info.vertex_in.vk_topology;
  vk_pipeline_input_assembly_state_create_info_.primitiveRestartEnable =
      ELEM(graphics_info.vertex_in.vk_topology,
           VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
           VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
           VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY) ?
          VK_FALSE :
          VK_TRUE;
  vk_pipeline_vertex_input_state_create_info_.pVertexAttributeDescriptions =
      graphics_info.vertex_in.attributes.data();
  vk_pipeline_vertex_input_state_create_info_.vertexAttributeDescriptionCount =
      graphics_info.vertex_in.attributes.size();
  vk_pipeline_vertex_input_state_create_info_.pVertexBindingDescriptions =
      graphics_info.vertex_in.bindings.data();
  vk_pipeline_vertex_input_state_create_info_.vertexBindingDescriptionCount =
      graphics_info.vertex_in.bindings.size();

  /* Rasterization state */
  vk_pipeline_rasterization_state_create_info_.frontFace = graphics_info.state.invert_facing ?
                                                               VK_FRONT_FACE_COUNTER_CLOCKWISE :
                                                               VK_FRONT_FACE_CLOCKWISE;
  vk_pipeline_rasterization_state_create_info_.cullMode = to_vk_cull_mode_flags(
      static_cast<eGPUFaceCullTest>(graphics_info.state.culling_test));
  if (graphics_info.state.shadow_bias) {
    vk_pipeline_rasterization_state_create_info_.depthBiasEnable = VK_TRUE;
    vk_pipeline_rasterization_state_create_info_.depthBiasSlopeFactor = 2.0f;
    vk_pipeline_rasterization_state_create_info_.depthBiasConstantFactor = 1.0f;
    vk_pipeline_rasterization_state_create_info_.depthBiasClamp = 0.0f;
  }
  else {
    vk_pipeline_rasterization_state_create_info_.depthBiasEnable = VK_FALSE;
  }
  vk_pipeline_rasterization_state_create_info_.frontFace = graphics_info.state.invert_facing ?
                                                               VK_FRONT_FACE_COUNTER_CLOCKWISE :
                                                               VK_FRONT_FACE_CLOCKWISE;

  /* Viewport state */
  vk_pipeline_viewport_state_create_info_.pViewports =
      graphics_info.fragment_shader.viewports.data();
  vk_pipeline_viewport_state_create_info_.viewportCount =
      graphics_info.fragment_shader.viewports.size();
  vk_pipeline_viewport_state_create_info_.pScissors =
      graphics_info.fragment_shader.scissors.data();
  vk_pipeline_viewport_state_create_info_.scissorCount =
      graphics_info.fragment_shader.scissors.size();

  /* Color blending */
  {
    VkPipelineColorBlendStateCreateInfo &cb = vk_pipeline_color_blend_state_create_info_;
    VkPipelineColorBlendAttachmentState &att_state =
        vk_pipeline_color_blend_attachment_state_template_;

    att_state.blendEnable = VK_TRUE;
    att_state.alphaBlendOp = VK_BLEND_OP_ADD;
    att_state.colorBlendOp = VK_BLEND_OP_ADD;
    att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att_state.colorWriteMask = 0;
    cb.blendConstants[0] = 1.0f;
    cb.blendConstants[1] = 1.0f;
    cb.blendConstants[2] = 1.0f;
    cb.blendConstants[3] = 1.0f;

    switch (graphics_info.state.blend) {
      default:
      case GPU_BLEND_ALPHA:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_ALPHA_PREMULT:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_ADDITIVE:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_SUBTRACT:
      case GPU_BLEND_ADDITIVE_PREMULT:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_MULTIPLY:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        break;

      case GPU_BLEND_INVERT:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_OIT:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_BACKGROUND:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_ALPHA_UNDER_PREMUL:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_CUSTOM:
        att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;
        att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
        break;
    }

    if (graphics_info.state.blend == GPU_BLEND_SUBTRACT) {
      att_state.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
      att_state.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    }
    else {
      att_state.alphaBlendOp = VK_BLEND_OP_ADD;
      att_state.colorBlendOp = VK_BLEND_OP_ADD;
    }

    if (graphics_info.state.blend != GPU_BLEND_NONE) {
      att_state.blendEnable = VK_TRUE;
    }
    else {
      att_state.blendEnable = VK_FALSE;
    }

    /* Adjust the template with the color components in the write mask. */
    if ((graphics_info.state.write_mask & GPU_WRITE_RED) != 0) {
      att_state.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    }
    if ((graphics_info.state.write_mask & GPU_WRITE_GREEN) != 0) {
      att_state.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    }
    if ((graphics_info.state.write_mask & GPU_WRITE_BLUE) != 0) {
      att_state.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    }
    if ((graphics_info.state.write_mask & GPU_WRITE_ALPHA) != 0) {
      att_state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    }

    vk_pipeline_color_blend_attachment_states_.clear();
    vk_pipeline_color_blend_attachment_states_.append_n_times(
        vk_pipeline_color_blend_attachment_state_template_,
        graphics_info.fragment_out.color_attachment_formats.size());
    vk_pipeline_color_blend_state_create_info_.attachmentCount =
        vk_pipeline_color_blend_attachment_states_.size();
    vk_pipeline_color_blend_state_create_info_.pAttachments =
        vk_pipeline_color_blend_attachment_states_.data();
  }

  if (graphics_info.fragment_out.depth_attachment_format != VK_FORMAT_UNDEFINED) {
    vk_graphics_pipeline_create_info_.pDepthStencilState =
        &vk_pipeline_depth_stencil_state_create_info_;
    vk_pipeline_depth_stencil_state_create_info_.depthWriteEnable =
        (graphics_info.state.write_mask & GPU_WRITE_DEPTH) ? VK_TRUE : VK_FALSE;

    vk_pipeline_depth_stencil_state_create_info_.depthTestEnable = VK_TRUE;
    switch (graphics_info.state.depth_test) {
      case GPU_DEPTH_LESS:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_LESS;
        break;
      case GPU_DEPTH_LESS_EQUAL:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
      case GPU_DEPTH_EQUAL:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_EQUAL;
        break;
      case GPU_DEPTH_GREATER:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_GREATER;
        break;
      case GPU_DEPTH_GREATER_EQUAL:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp =
            VK_COMPARE_OP_GREATER_OR_EQUAL;
        break;
      case GPU_DEPTH_ALWAYS:
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        break;
      case GPU_DEPTH_NONE:
        vk_pipeline_depth_stencil_state_create_info_.depthTestEnable = VK_FALSE;
        vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_NEVER;
        break;
    }
  }

  if (graphics_info.fragment_out.stencil_attachment_format != VK_FORMAT_UNDEFINED) {
    vk_graphics_pipeline_create_info_.pDepthStencilState =
        &vk_pipeline_depth_stencil_state_create_info_;

    switch (graphics_info.state.stencil_test) {
      case GPU_STENCIL_NEQUAL:
        vk_pipeline_depth_stencil_state_create_info_.stencilTestEnable = VK_TRUE;
        vk_pipeline_depth_stencil_state_create_info_.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        break;
      case GPU_STENCIL_EQUAL:
        vk_pipeline_depth_stencil_state_create_info_.stencilTestEnable = VK_TRUE;
        vk_pipeline_depth_stencil_state_create_info_.front.compareOp = VK_COMPARE_OP_EQUAL;
        break;
      case GPU_STENCIL_ALWAYS:
        vk_pipeline_depth_stencil_state_create_info_.stencilTestEnable = VK_TRUE;
        vk_pipeline_depth_stencil_state_create_info_.front.compareOp = VK_COMPARE_OP_ALWAYS;
        break;
      case GPU_STENCIL_NONE:
        vk_pipeline_depth_stencil_state_create_info_.stencilTestEnable = VK_FALSE;
        vk_pipeline_depth_stencil_state_create_info_.front.compareOp = VK_COMPARE_OP_ALWAYS;
        break;
    }

    vk_pipeline_depth_stencil_state_create_info_.front.compareMask =
        graphics_info.mutable_state.stencil_compare_mask;
    vk_pipeline_depth_stencil_state_create_info_.front.reference =
        graphics_info.mutable_state.stencil_reference;
    vk_pipeline_depth_stencil_state_create_info_.front.writeMask =
        graphics_info.mutable_state.stencil_write_mask;

    switch (graphics_info.state.stencil_op) {
      case GPU_STENCIL_OP_REPLACE:
        vk_pipeline_depth_stencil_state_create_info_.front.failOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.passOp = VK_STENCIL_OP_REPLACE;
        vk_pipeline_depth_stencil_state_create_info_.front.depthFailOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.back =
            vk_pipeline_depth_stencil_state_create_info_.front;
        break;

      case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
        vk_pipeline_depth_stencil_state_create_info_.front.failOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.passOp =
            VK_STENCIL_OP_DECREMENT_AND_WRAP;
        vk_pipeline_depth_stencil_state_create_info_.front.depthFailOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.back =
            vk_pipeline_depth_stencil_state_create_info_.front;
        vk_pipeline_depth_stencil_state_create_info_.back.passOp =
            VK_STENCIL_OP_INCREMENT_AND_WRAP;
        break;

      case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
        vk_pipeline_depth_stencil_state_create_info_.front.failOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.passOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.depthFailOp =
            VK_STENCIL_OP_INCREMENT_AND_WRAP;
        vk_pipeline_depth_stencil_state_create_info_.back =
            vk_pipeline_depth_stencil_state_create_info_.front;
        vk_pipeline_depth_stencil_state_create_info_.back.depthFailOp =
            VK_STENCIL_OP_DECREMENT_AND_WRAP;
        break;

      case GPU_STENCIL_OP_NONE:
      default:
        vk_pipeline_depth_stencil_state_create_info_.front.failOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.passOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.front.depthFailOp = VK_STENCIL_OP_KEEP;
        vk_pipeline_depth_stencil_state_create_info_.back =
            vk_pipeline_depth_stencil_state_create_info_.front;
        break;
    }
  }

  /* VK_KHR_dynamic_rendering */
  vk_pipeline_rendering_create_info_.depthAttachmentFormat =
      graphics_info.fragment_out.depth_attachment_format;
  vk_pipeline_rendering_create_info_.stencilAttachmentFormat =
      graphics_info.fragment_out.stencil_attachment_format;
  vk_pipeline_rendering_create_info_.colorAttachmentCount =
      graphics_info.fragment_out.color_attachment_formats.size();
  vk_pipeline_rendering_create_info_.pColorAttachmentFormats =
      graphics_info.fragment_out.color_attachment_formats.data();

  /* Common values */
  vk_graphics_pipeline_create_info_.layout = graphics_info.vk_pipeline_layout;
  // TODO: based on `vk_pipeline_base` we should update the flags.
  vk_graphics_pipeline_create_info_.basePipelineHandle = vk_pipeline_base;

  /* Build pipeline. */
  VKBackend &backend = VKBackend::get();
  VKDevice &device = backend.device;
  VK_ALLOCATION_CALLBACKS;

  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateGraphicsPipelines(device.vk_handle(),
                            device.vk_pipeline_cache_get(),
                            1,
                            &vk_graphics_pipeline_create_info_,
                            vk_allocation_callbacks,
                            &pipeline);
  graphic_pipelines_.add(graphics_info, pipeline);

  /* Reset values to initial value. */
  specialization_info_reset();
  vk_graphics_pipeline_create_info_.stageCount = 0;
  vk_graphics_pipeline_create_info_.layout = VK_NULL_HANDLE;
  vk_graphics_pipeline_create_info_.basePipelineHandle = VK_NULL_HANDLE;
  for (VkPipelineShaderStageCreateInfo &info :
       MutableSpan<VkPipelineShaderStageCreateInfo>(vk_pipeline_shader_stage_create_info_, 3))
  {
    info.module = VK_NULL_HANDLE;
    info.pSpecializationInfo = nullptr;
  }
  vk_pipeline_input_assembly_state_create_info_.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  vk_pipeline_input_assembly_state_create_info_.primitiveRestartEnable = VK_TRUE;
  vk_pipeline_vertex_input_state_create_info_.pVertexAttributeDescriptions = nullptr;
  vk_pipeline_vertex_input_state_create_info_.vertexAttributeDescriptionCount = 0;
  vk_pipeline_vertex_input_state_create_info_.pVertexBindingDescriptions = nullptr;
  vk_pipeline_vertex_input_state_create_info_.vertexBindingDescriptionCount = 0;
  vk_pipeline_rasterization_state_create_info_.frontFace = VK_FRONT_FACE_CLOCKWISE;
  vk_pipeline_rasterization_state_create_info_.cullMode = VK_CULL_MODE_NONE;
  vk_pipeline_rasterization_state_create_info_.depthBiasEnable = VK_FALSE;
  vk_pipeline_rasterization_state_create_info_.depthBiasSlopeFactor = 0.0f;
  vk_pipeline_rasterization_state_create_info_.depthBiasConstantFactor = 0.0f;
  vk_pipeline_rasterization_state_create_info_.depthBiasClamp = 0.0f;
  vk_pipeline_rasterization_state_create_info_.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  vk_pipeline_viewport_state_create_info_.pScissors = nullptr;
  vk_pipeline_viewport_state_create_info_.scissorCount = 0;
  vk_pipeline_viewport_state_create_info_.pViewports = nullptr;
  vk_pipeline_viewport_state_create_info_.viewportCount = 0;
  vk_pipeline_color_blend_state_create_info_.attachmentCount = 0;
  vk_pipeline_color_blend_state_create_info_.pAttachments = nullptr;
  vk_pipeline_rendering_create_info_.colorAttachmentCount = 0;
  vk_pipeline_rendering_create_info_.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
  vk_pipeline_rendering_create_info_.pColorAttachmentFormats = nullptr;
  vk_pipeline_rendering_create_info_.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
  vk_pipeline_depth_stencil_state_create_info_.depthTestEnable = VK_FALSE;
  vk_pipeline_depth_stencil_state_create_info_.depthWriteEnable = VK_FALSE;
  vk_pipeline_depth_stencil_state_create_info_.depthCompareOp = VK_COMPARE_OP_NEVER;
  vk_pipeline_depth_stencil_state_create_info_.stencilTestEnable = VK_FALSE;
  vk_pipeline_depth_stencil_state_create_info_.front = {};
  vk_pipeline_depth_stencil_state_create_info_.back = {};
  vk_graphics_pipeline_create_info_.pDepthStencilState = nullptr;

  return pipeline;
}

void VKPipelinePool::remove(Span<VkShaderModule> vk_shader_modules)
{
  std::scoped_lock lock(mutex_);
  Vector<VkPipeline> pipelines_to_destroy;
  compute_pipelines_.remove_if([&](auto item) {
    if (vk_shader_modules.contains(item.key.vk_shader_module)) {
      pipelines_to_destroy.append(item.value);
      return true;
    }
    return false;
  });
  graphic_pipelines_.remove_if([&](auto item) {
    if (vk_shader_modules.contains(item.key.pre_rasterization.vk_vertex_module) ||
        vk_shader_modules.contains(item.key.pre_rasterization.vk_geometry_module) ||
        vk_shader_modules.contains(item.key.fragment_shader.vk_fragment_module))
    {
      pipelines_to_destroy.append(item.value);
      return true;
    }
    return false;
  });

  VKDevice &device = VKBackend::get().device;
  VK_ALLOCATION_CALLBACKS;
  for (VkPipeline vk_pipeline : pipelines_to_destroy) {
    vkDestroyPipeline(device.vk_handle(), vk_pipeline, vk_allocation_callbacks);
  }
}

void VKPipelinePool::free_data()
{
  std::scoped_lock lock(mutex_);
  VKDevice &device = VKBackend::get().device;
  VK_ALLOCATION_CALLBACKS;
  for (VkPipeline &vk_pipeline : graphic_pipelines_.values()) {
    vkDestroyPipeline(device.vk_handle(), vk_pipeline, vk_allocation_callbacks);
  }
  graphic_pipelines_.clear();
  for (VkPipeline &vk_pipeline : compute_pipelines_.values()) {
    vkDestroyPipeline(device.vk_handle(), vk_pipeline, vk_allocation_callbacks);
  }
  compute_pipelines_.clear();
}

}  // namespace blender::gpu
