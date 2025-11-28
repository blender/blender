/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * To create graphics pipelines multiple structs are needed. With graphics pipeline libraries only
 * parts of these structs doesn't need to be filled. This header file ensures that all code to
 * create `VkGraphicsPipelineCreateInfo` and related structs are grouped and the different
 * configurations can be created.
 */
// TODO: separate in the different configuration and add a main configuration that includes all.
// Unure yet if how to organize this in structs to keep stack allocation small.

#pragma once

#include "vk_common.hh"
#include "vk_pipeline_pool.hh"

namespace blender::gpu {

struct VKGraphicsPipelineCreateInfoBuilder {
  VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info;
  std::array<VkPipelineShaderStageCreateInfo, 3> vk_pipeline_shader_stage_create_info;
  VkSpecializationInfo vk_specialization_info;
  Array<VkSpecializationMapEntry> vk_specialization_map_entries;
  VkPipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info;
  VkPipelineVertexInputStateCreateInfo vk_pipeline_vertex_input_state_create_info;
  VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state_create_info;
  VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
      vk_pipeline_rasterization_provoking_vertex_state_info;
  VkPipelineRasterizationLineStateCreateInfoEXT vk_pipeline_rasterization_line_state_info;
  Vector<VkDynamicState, 7> vk_dynamic_states;
  VkPipelineDynamicStateCreateInfo vk_pipeline_dynamic_state_create_info;
  VkPipelineViewportStateCreateInfo vk_pipeline_viewport_state_create_info;
  VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_stencil_state_create_info;
  VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;
  Vector<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states;
  VkPipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info;
  VkGraphicsPipelineCreateInfo vk_graphics_pipeline_create_info;
  VkGraphicsPipelineLibraryCreateInfoEXT vk_graphics_pipeline_library_create_info;

  /**
   * Initialize graphics pipeline create info and related structs for a full pipeline build.
   */
  void build_full(const VKGraphicsInfo &graphics_info,
                  const VKExtensions &extensions,
                  VkPipeline vk_pipeline_base)
  {
    build_graphics_pipeline(graphics_info, vk_pipeline_base);

    build_input_assembly_state(graphics_info.vertex_in);
    build_vertex_input_state(graphics_info.vertex_in);

    build_shader_stages(graphics_info.shaders);
    const bool do_specialization_constants =
        !graphics_info.shaders.specialization_constants.is_empty();
    if (do_specialization_constants) {
      build_specialization_constants(graphics_info.shaders);
    }
    build_dynamic_state(graphics_info.shaders, extensions);
    build_multisample_state();
    build_viewport_state(graphics_info.shaders);
    build_rasterization_state(graphics_info.shaders, extensions);
    build_depth_stencil_state(graphics_info.shaders);

    build_color_blend_attachment_states(graphics_info.fragment_out);
    build_color_blend_state(graphics_info.fragment_out, extensions);
    build_dynamic_rendering(graphics_info.fragment_out);
  }

  /**
   * Initialize graphics pipeline create info and related structs for a vertex input library build.
   */
  void build_vertex_input_lib(const VKGraphicsInfo::VertexIn &vertex_input_info,
                              VkPipeline vk_pipeline_base)
  {
    build_graphics_pipeline_library(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
    build_graphics_pipeline_vertex_input_lib(vk_pipeline_base);
    build_input_assembly_state(vertex_input_info);
    build_vertex_input_state(vertex_input_info);
  }

  /**
   * Initialize graphics pipeline create info and related structs for a shaders library build.
   */
  void build_shaders_lib(const VKGraphicsInfo::Shaders &shaders_info,
                         const VKExtensions &extensions,
                         VkPipeline vk_pipeline_base)
  {
    build_graphics_pipeline_library(
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
    build_graphics_pipeline_shaders_lib(shaders_info, vk_pipeline_base);

    build_shader_stages(shaders_info);
    const bool do_specialization_constants = !shaders_info.specialization_constants.is_empty();
    if (do_specialization_constants) {
      build_specialization_constants(shaders_info);
    }
    build_dynamic_state(shaders_info, extensions);
    build_multisample_state();
    build_viewport_state(shaders_info);
    build_rasterization_state(shaders_info, extensions);
    build_depth_stencil_state(shaders_info);
    build_dynamic_rendering_shaders_lib();
  }

  /**
   * Initialize graphics pipeline create info and related structs for a fragment output library
   * build.
   */
  void build_fragment_output_lib(const VKGraphicsInfo::FragmentOut &fragment_output_info,
                                 const VKExtensions &extensions,
                                 VkPipeline vk_pipeline_base)
  {
    build_graphics_pipeline_library(
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
    build_graphics_pipeline_fragment_output_lib(vk_pipeline_base);
    build_multisample_state();
    build_color_blend_attachment_states(fragment_output_info);
    build_color_blend_state(fragment_output_info, extensions);
    build_dynamic_rendering(fragment_output_info);
  }

 private:
  void build_graphics_pipeline(const VKGraphicsInfo &graphics_info, VkPipeline vk_pipeline_base)
  {
    vk_graphics_pipeline_create_info = {};
    vk_graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vk_graphics_pipeline_create_info.pNext = &vk_pipeline_rendering_create_info;
    vk_graphics_pipeline_create_info.stageCount = graphics_info.shaders.vk_geometry_module ==
                                                          VK_NULL_HANDLE ?
                                                      2 :
                                                      3;
    vk_graphics_pipeline_create_info.pStages = vk_pipeline_shader_stage_create_info.data();
    vk_graphics_pipeline_create_info.pInputAssemblyState =
        &vk_pipeline_input_assembly_state_create_info;
    vk_graphics_pipeline_create_info.pVertexInputState =
        &vk_pipeline_vertex_input_state_create_info;
    vk_graphics_pipeline_create_info.pRasterizationState =
        &vk_pipeline_rasterization_state_create_info;
    vk_graphics_pipeline_create_info.pDepthStencilState =
        &vk_pipeline_depth_stencil_state_create_info;
    vk_graphics_pipeline_create_info.pDynamicState = &vk_pipeline_dynamic_state_create_info;
    vk_graphics_pipeline_create_info.pViewportState = &vk_pipeline_viewport_state_create_info;
    vk_graphics_pipeline_create_info.pMultisampleState =
        &vk_pipeline_multisample_state_create_info;
    vk_graphics_pipeline_create_info.pColorBlendState = &vk_pipeline_color_blend_state_create_info;
    vk_graphics_pipeline_create_info.layout = graphics_info.shaders.vk_pipeline_layout;
    vk_graphics_pipeline_create_info.basePipelineHandle = vk_pipeline_base;
  }

  void build_graphics_pipeline_vertex_input_lib(VkPipeline vk_pipeline_base)
  {
    vk_graphics_pipeline_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        &vk_graphics_pipeline_library_create_info,
        VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
            VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
        0,
        nullptr,
        &vk_pipeline_vertex_input_state_create_info,
        &vk_pipeline_input_assembly_state_create_info,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        0,
        vk_pipeline_base,
        0};
  }

  void build_graphics_pipeline_shaders_lib(const VKGraphicsInfo::Shaders &shaders_info,
                                           VkPipeline vk_pipeline_base)
  {
    vk_graphics_pipeline_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        &vk_graphics_pipeline_library_create_info,
        VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
            VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
        shaders_info.vk_geometry_module == VK_NULL_HANDLE ? 2u : 3u,
        vk_pipeline_shader_stage_create_info.data(),
        nullptr,
        nullptr,
        nullptr,
        &vk_pipeline_viewport_state_create_info,
        &vk_pipeline_rasterization_state_create_info,
        &vk_pipeline_multisample_state_create_info,
        &vk_pipeline_depth_stencil_state_create_info,
        nullptr,
        &vk_pipeline_dynamic_state_create_info,
        shaders_info.vk_pipeline_layout,
        VK_NULL_HANDLE,
        0,
        vk_pipeline_base,
        0};
    vk_graphics_pipeline_library_create_info.pNext = &vk_pipeline_rendering_create_info;
  }

  void build_graphics_pipeline_fragment_output_lib(VkPipeline vk_pipeline_base)
  {
    vk_graphics_pipeline_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        &vk_graphics_pipeline_library_create_info,
        VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
            VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &vk_pipeline_multisample_state_create_info,
        nullptr,
        &vk_pipeline_color_blend_state_create_info,
        nullptr,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        0,
        vk_pipeline_base,
        0};
    vk_graphics_pipeline_library_create_info.pNext = &vk_pipeline_rendering_create_info;
  }

  void build_graphics_pipeline_library(VkGraphicsPipelineLibraryFlagsEXT flags)
  {
    vk_graphics_pipeline_library_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, nullptr, flags};
  }

  void build_shader_stages(const VKGraphicsInfo::Shaders &shaders_info)
  {
    vk_pipeline_shader_stage_create_info[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                               nullptr,
                                               0,
                                               VK_SHADER_STAGE_VERTEX_BIT,
                                               shaders_info.vk_vertex_module,
                                               "main",
                                               nullptr};
    vk_pipeline_shader_stage_create_info[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                               nullptr,
                                               0,
                                               VK_SHADER_STAGE_FRAGMENT_BIT,
                                               shaders_info.vk_fragment_module,
                                               "main",
                                               nullptr};
    vk_pipeline_shader_stage_create_info[2] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                               nullptr,
                                               0,
                                               VK_SHADER_STAGE_GEOMETRY_BIT,
                                               shaders_info.vk_geometry_module,
                                               "main",
                                               nullptr};
  }

  void build_specialization_constants(const VKGraphicsInfo::Shaders &shaders_info)
  {
    vk_specialization_map_entries.reinitialize(shaders_info.specialization_constants.size());
    for (int index : IndexRange(3)) {
      vk_pipeline_shader_stage_create_info[index].pSpecializationInfo = &vk_specialization_info;
    }
    for (uint32_t index : IndexRange(shaders_info.specialization_constants.size())) {
      vk_specialization_map_entries[index] = {
          index, uint32_t(index * sizeof(uint32_t)), sizeof(uint32_t)};
    }
    vk_specialization_info = {uint32_t(vk_specialization_map_entries.size()),
                              vk_specialization_map_entries.data(),
                              shaders_info.specialization_constants.size() * sizeof(uint32_t),
                              shaders_info.specialization_constants.data()};
  }

  void build_multisample_state()
  {
    vk_pipeline_multisample_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE};
  }

  void build_viewport_state(const VKGraphicsInfo::Shaders &shaders_info)
  {
    vk_pipeline_viewport_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr,
        0,
        shaders_info.viewport_count,
        nullptr,
        shaders_info.viewport_count,
        nullptr};
  }

  void build_input_assembly_state(const VKGraphicsInfo::VertexIn &vertex_input_info)
  {
    vk_pipeline_input_assembly_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    vk_pipeline_input_assembly_state_create_info.topology = vertex_input_info.vk_topology;
    vk_pipeline_input_assembly_state_create_info.primitiveRestartEnable =
        ELEM(vertex_input_info.vk_topology,
             VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
             VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
             VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY) ?
            VK_FALSE :
            VK_TRUE;
  }

  void build_vertex_input_state(const VKGraphicsInfo::VertexIn &vertex_input_info)
  {
    vk_pipeline_vertex_input_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vk_pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions =
        vertex_input_info.attributes.data();
    vk_pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount =
        vertex_input_info.attributes.size();
    vk_pipeline_vertex_input_state_create_info.pVertexBindingDescriptions =
        vertex_input_info.bindings.data();
    vk_pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount =
        vertex_input_info.bindings.size();
  }

  void build_rasterization_state(const VKGraphicsInfo::Shaders &shaders_info,
                                 const VKExtensions &extensions)
  {
    vk_pipeline_rasterization_provoking_vertex_state_info = {};
    vk_pipeline_rasterization_provoking_vertex_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
    vk_pipeline_rasterization_provoking_vertex_state_info.provokingVertexMode =
        VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
    vk_pipeline_rasterization_provoking_vertex_state_info.provokingVertexMode =
        shaders_info.state.provoking_vert == GPU_VERTEX_LAST ?
            VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT :
            VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

    vk_pipeline_rasterization_state_create_info = {};
    vk_pipeline_rasterization_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vk_pipeline_rasterization_state_create_info.lineWidth = 1.0f;
    vk_pipeline_rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    vk_pipeline_rasterization_state_create_info.pNext =
        &vk_pipeline_rasterization_provoking_vertex_state_info;

    vk_pipeline_rasterization_state_create_info.cullMode = to_vk_cull_mode_flags(
        static_cast<GPUFaceCullTest>(shaders_info.state.culling_test));
    if (!extensions.extended_dynamic_state && !shaders_info.state.invert_facing) {
      vk_pipeline_rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    }

    if (extensions.line_rasterization) {
      /* Request use of Bresenham algorithm if supported. */
      vk_pipeline_rasterization_line_state_info = {};
      vk_pipeline_rasterization_line_state_info.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;
      vk_pipeline_rasterization_line_state_info.lineRasterizationMode =
          VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
      vk_pipeline_rasterization_line_state_info.stippledLineEnable = VK_FALSE;
      vk_pipeline_rasterization_line_state_info.lineStippleFactor = 0u;
      vk_pipeline_rasterization_line_state_info.lineStipplePattern = 0u;
      vk_pipeline_rasterization_line_state_info.pNext =
          vk_pipeline_rasterization_state_create_info.pNext;
      vk_pipeline_rasterization_state_create_info.pNext =
          &vk_pipeline_rasterization_line_state_info;
    }
  }

  void build_dynamic_state(const VKGraphicsInfo::Shaders &shaders_info,
                           const VKExtensions &extensions)
  {
    vk_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const bool is_line_topology = ELEM(shaders_info.vk_topology,
                                       VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                       VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
                                       VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    if (is_line_topology) {
      vk_dynamic_states.append(VK_DYNAMIC_STATE_LINE_WIDTH);
    }
    if (shaders_info.has_stencil && shaders_info.state.stencil_test != GPU_STENCIL_NONE) {
      vk_dynamic_states.append(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
      vk_dynamic_states.append(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
      vk_dynamic_states.append(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
    }
    if (extensions.extended_dynamic_state) {
      vk_dynamic_states.append(VK_DYNAMIC_STATE_FRONT_FACE);
    }
    vk_pipeline_dynamic_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                             nullptr,
                                             0,
                                             uint32_t(vk_dynamic_states.size()),
                                             vk_dynamic_states.data()};
  }

  void build_depth_stencil_state(const VKGraphicsInfo::Shaders &shaders_info)
  {
    vk_pipeline_depth_stencil_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    if (shaders_info.has_depth) {
      vk_graphics_pipeline_create_info.pDepthStencilState =
          &vk_pipeline_depth_stencil_state_create_info;
      vk_pipeline_depth_stencil_state_create_info.depthWriteEnable =
          (shaders_info.state.write_mask & GPU_WRITE_DEPTH) ? VK_TRUE : VK_FALSE;

      vk_pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
      switch (shaders_info.state.depth_test) {
        case GPU_DEPTH_LESS:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
          break;
        case GPU_DEPTH_LESS_EQUAL:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
          break;
        case GPU_DEPTH_EQUAL:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_EQUAL;
          break;
        case GPU_DEPTH_GREATER:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_GREATER;
          break;
        case GPU_DEPTH_GREATER_EQUAL:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp =
              VK_COMPARE_OP_GREATER_OR_EQUAL;
          break;
        case GPU_DEPTH_ALWAYS:
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
          break;
        case GPU_DEPTH_NONE:
          vk_pipeline_depth_stencil_state_create_info.depthTestEnable = VK_FALSE;
          vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_NEVER;
          break;
      }
    }

    if (shaders_info.has_stencil) {
      vk_graphics_pipeline_create_info.pDepthStencilState =
          &vk_pipeline_depth_stencil_state_create_info;

      switch (shaders_info.state.stencil_test) {
        case GPU_STENCIL_NEQUAL:
          vk_pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_TRUE;
          vk_pipeline_depth_stencil_state_create_info.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
          break;
        case GPU_STENCIL_EQUAL:
          vk_pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_TRUE;
          vk_pipeline_depth_stencil_state_create_info.front.compareOp = VK_COMPARE_OP_EQUAL;
          break;
        case GPU_STENCIL_ALWAYS:
          vk_pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_TRUE;
          vk_pipeline_depth_stencil_state_create_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
          break;
        case GPU_STENCIL_NONE:
          vk_pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
          vk_pipeline_depth_stencil_state_create_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
          break;
      }

      switch (shaders_info.state.stencil_op) {
        case GPU_STENCIL_OP_REPLACE:
          vk_pipeline_depth_stencil_state_create_info.front.failOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.passOp = VK_STENCIL_OP_REPLACE;
          vk_pipeline_depth_stencil_state_create_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.back =
              vk_pipeline_depth_stencil_state_create_info.front;
          break;

        case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
          vk_pipeline_depth_stencil_state_create_info.front.failOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.passOp =
              VK_STENCIL_OP_DECREMENT_AND_WRAP;
          vk_pipeline_depth_stencil_state_create_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.back =
              vk_pipeline_depth_stencil_state_create_info.front;
          vk_pipeline_depth_stencil_state_create_info.back.passOp =
              VK_STENCIL_OP_INCREMENT_AND_WRAP;
          break;

        case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
          vk_pipeline_depth_stencil_state_create_info.front.failOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.passOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.depthFailOp =
              VK_STENCIL_OP_INCREMENT_AND_WRAP;
          vk_pipeline_depth_stencil_state_create_info.back =
              vk_pipeline_depth_stencil_state_create_info.front;
          vk_pipeline_depth_stencil_state_create_info.back.depthFailOp =
              VK_STENCIL_OP_DECREMENT_AND_WRAP;
          break;

        case GPU_STENCIL_OP_NONE:
        default:
          vk_pipeline_depth_stencil_state_create_info.front.failOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.passOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
          vk_pipeline_depth_stencil_state_create_info.back =
              vk_pipeline_depth_stencil_state_create_info.front;
          break;
      }
    }
  }

  void build_dynamic_rendering(const VKGraphicsInfo::FragmentOut &fragment_output_info)
  {
    vk_pipeline_rendering_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        0,
        uint32_t(fragment_output_info.color_attachment_formats.size()),
        fragment_output_info.color_attachment_formats.data(),
        fragment_output_info.depth_attachment_format,
        fragment_output_info.stencil_attachment_format};
  }

  /* Shaders lib only requires the viewmask to be set. */
  void build_dynamic_rendering_shaders_lib()
  {
    vk_pipeline_rendering_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  }

  void build_color_blend_attachment_states(const VKGraphicsInfo::FragmentOut &fragment_output_info)
  {
    VkPipelineColorBlendAttachmentState attachment_state = {VK_TRUE,
                                                            VK_BLEND_FACTOR_DST_ALPHA,
                                                            VK_BLEND_FACTOR_ONE,
                                                            VK_BLEND_OP_ADD,
                                                            VK_BLEND_FACTOR_ZERO,
                                                            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                            VK_BLEND_OP_ADD,
                                                            0};

    switch (fragment_output_info.state.blend) {
      default:
      case GPU_BLEND_ALPHA:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_ALPHA_PREMULT:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_ADDITIVE:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

        /* Factors are not use in min or max mode, but avoid uninitialized values. */;
      case GPU_BLEND_MIN:
      case GPU_BLEND_MAX:
      case GPU_BLEND_SUBTRACT:
      case GPU_BLEND_ADDITIVE_PREMULT:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_MULTIPLY:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        break;

      case GPU_BLEND_INVERT:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_OIT:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_BACKGROUND:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        break;

      case GPU_BLEND_ALPHA_UNDER_PREMUL:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;

      case GPU_BLEND_CUSTOM:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
        break;

      case GPU_BLEND_OVERLAY_MASK_FROM_ALPHA:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

      case GPU_BLEND_TRANSPARENCY:
        attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        break;
    }

    if (fragment_output_info.state.blend == GPU_BLEND_MIN) {
      attachment_state.alphaBlendOp = VK_BLEND_OP_MIN;
      attachment_state.colorBlendOp = VK_BLEND_OP_MIN;
    }
    else if (fragment_output_info.state.blend == GPU_BLEND_MAX) {
      attachment_state.alphaBlendOp = VK_BLEND_OP_MAX;
      attachment_state.colorBlendOp = VK_BLEND_OP_MAX;
    }
    else if (fragment_output_info.state.blend == GPU_BLEND_SUBTRACT) {
      attachment_state.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
      attachment_state.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    }
    else {
      attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
      attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    }

    if (fragment_output_info.state.blend != GPU_BLEND_NONE) {
      attachment_state.blendEnable = VK_TRUE;
    }
    else {
      attachment_state.blendEnable = VK_FALSE;
    }

    /* Adjust the template with the color components in the write mask. */
    if ((fragment_output_info.state.write_mask & GPU_WRITE_RED) != 0) {
      attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    }
    if ((fragment_output_info.state.write_mask & GPU_WRITE_GREEN) != 0) {
      attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    }
    if ((fragment_output_info.state.write_mask & GPU_WRITE_BLUE) != 0) {
      attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    }
    if ((fragment_output_info.state.write_mask & GPU_WRITE_ALPHA) != 0) {
      attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    }

    vk_pipeline_color_blend_attachment_states.append_n_times(
        attachment_state, fragment_output_info.color_attachment_formats.size());
  }

  void build_color_blend_state(const VKGraphicsInfo::FragmentOut &fragment_output_info,
                               const VKExtensions &extensions)
  {
    vk_pipeline_color_blend_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        uint32_t(vk_pipeline_color_blend_attachment_states.size()),
        vk_pipeline_color_blend_attachment_states.data(),
        {1.0f, 1.0f, 1.0f, 1.0f}};
    /* Logic ops. */
    if (fragment_output_info.state.logic_op_xor && extensions.logic_ops) {
      vk_pipeline_color_blend_state_create_info.logicOpEnable = VK_TRUE;
      vk_pipeline_color_blend_state_create_info.logicOp = VK_LOGIC_OP_XOR;
    }
  }
};
}  // namespace blender::gpu
