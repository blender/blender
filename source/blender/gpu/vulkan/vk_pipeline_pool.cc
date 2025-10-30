/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "BKE_appdir.hh"
#include "BKE_blender_version.h"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_time.h"

#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_pipeline_pool.hh"

#ifdef WITH_BUILDINFO
extern "C" char build_hash[];
#endif
static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

void VKPipelinePool::init()
{
  VKDevice &device = VKBackend::get().device;
  VkPipelineCacheCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache_static_);
  debug::object_label(vk_pipeline_cache_static_, "VkPipelineCache.Static");
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache_non_static_);
  debug::object_label(vk_pipeline_cache_non_static_, "VkPipelineCache.Dynamic");
}

/* -------------------------------------------------------------------- */
/** \name Compute pipelines
 * \{ */

VkPipeline VKPipelinePool::get_or_create_compute_pipeline(const VKComputeInfo &compute_info,
                                                          const bool is_static_shader,
                                                          VkPipeline vk_pipeline_base,
                                                          StringRefNull name)
{
  VkPipelineCache vk_pipeline_cache = is_static_shader ? vk_pipeline_cache_static_ :
                                                         vk_pipeline_cache_non_static_;
  return compute_.get_or_create(compute_info, vk_pipeline_cache, vk_pipeline_base, name);
}

template<>
VkPipeline VKPipelineMap<VKComputeInfo>::create(const VKComputeInfo &compute_info,
                                                VkPipelineCache vk_pipeline_cache,
                                                VkPipeline vk_pipeline_base,
                                                StringRefNull name)
{
  /* Building compute pipeline create info */
  const bool do_specialization_constants = !compute_info.specialization_constants.is_empty();
  VkComputePipelineCreateInfo vk_compute_pipeline_create_info = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      nullptr,
      0,
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       nullptr,
       0,
       VK_SHADER_STAGE_COMPUTE_BIT,
       compute_info.vk_shader_module,
       "main",
       nullptr},
      compute_info.vk_pipeline_layout,
      vk_pipeline_base,
      0};

  /* Specialization constants */
  VkSpecializationInfo vk_specialization_info;
  Array<VkSpecializationMapEntry> vk_specialization_map_entries(
      compute_info.specialization_constants.size());
  if (do_specialization_constants) {
    vk_compute_pipeline_create_info.stage.pSpecializationInfo = &vk_specialization_info;
    for (uint32_t index : IndexRange(compute_info.specialization_constants.size())) {
      vk_specialization_map_entries[index] = {
          index, uint32_t(index * sizeof(uint32_t)), sizeof(uint32_t)};
    }
    vk_specialization_info = {uint32_t(vk_specialization_map_entries.size()),
                              vk_specialization_map_entries.data(),
                              compute_info.specialization_constants.size() * sizeof(uint32_t),
                              compute_info.specialization_constants.data()};
  }

  /* Create pipeline. */
  VKBackend &backend = VKBackend::get();
  VKDevice &device = backend.device;

  double start_time = BLI_time_now_seconds();
  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateComputePipelines(device.vk_handle(),
                           vk_pipeline_cache,
                           1,
                           &vk_compute_pipeline_create_info,
                           nullptr,
                           &pipeline);
  double end_time = BLI_time_now_seconds();
  debug::object_label(pipeline, name);
  CLOG_DEBUG(&LOG,
             "Compiled compute pipeline %s in %fms ",
             name.c_str(),
             (end_time - start_time) * 1000.0);

  return pipeline;
}

/* \} */

/* -------------------------------------------------------------------- */
/** \name Graphics pipelines
 * \{ */

static void build_multisample_state(
    VkPipelineMultisampleStateCreateInfo &vk_pipeline_multisample_state_create_info)
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

static void build_viewport_state(
    const VKGraphicsInfo &graphics_info,
    VkPipelineViewportStateCreateInfo &vk_pipeline_viewport_state_create_info)
{
  vk_pipeline_viewport_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                            nullptr,
                                            0,
                                            graphics_info.fragment_shader.viewport_count,
                                            nullptr,
                                            graphics_info.fragment_shader.viewport_count,
                                            nullptr};
}

static void build_input_assembly_state(
    const VKGraphicsInfo &graphics_info,
    VkPipelineInputAssemblyStateCreateInfo &vk_pipeline_input_assembly_state_create_info)
{
  vk_pipeline_input_assembly_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  vk_pipeline_input_assembly_state_create_info.topology = graphics_info.vertex_in.vk_topology;
  vk_pipeline_input_assembly_state_create_info.primitiveRestartEnable =
      ELEM(graphics_info.vertex_in.vk_topology,
           VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
           VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
           VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY) ?
          VK_FALSE :
          VK_TRUE;
}

static void build_vertex_input_state(
    const VKGraphicsInfo &graphics_info,
    VkPipelineVertexInputStateCreateInfo &vk_pipeline_vertex_input_state_create_info)
{
  vk_pipeline_vertex_input_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vk_pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions =
      graphics_info.vertex_in.attributes.data();
  vk_pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount =
      graphics_info.vertex_in.attributes.size();
  vk_pipeline_vertex_input_state_create_info.pVertexBindingDescriptions =
      graphics_info.vertex_in.bindings.data();
  vk_pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount =
      graphics_info.vertex_in.bindings.size();
}

static void build_rasterization_state(
    const VKGraphicsInfo &graphics_info,
    VkPipelineRasterizationStateCreateInfo &vk_pipeline_rasterization_state_create_info,
    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
        &vk_pipeline_rasterization_provoking_vertex_state_info)
{
  vk_pipeline_rasterization_provoking_vertex_state_info = {};
  vk_pipeline_rasterization_provoking_vertex_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
  vk_pipeline_rasterization_provoking_vertex_state_info.provokingVertexMode =
      VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
  vk_pipeline_rasterization_provoking_vertex_state_info.provokingVertexMode =
      graphics_info.state.provoking_vert == GPU_VERTEX_LAST ?
          VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT :
          VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

  vk_pipeline_rasterization_state_create_info = {};
  vk_pipeline_rasterization_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  vk_pipeline_rasterization_state_create_info.lineWidth = 1.0f;
  vk_pipeline_rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  vk_pipeline_rasterization_state_create_info.pNext =
      &vk_pipeline_rasterization_provoking_vertex_state_info;

  vk_pipeline_rasterization_state_create_info.cullMode = to_vk_cull_mode_flags(
      static_cast<GPUFaceCullTest>(graphics_info.state.culling_test));
  if (graphics_info.state.shadow_bias) {
    vk_pipeline_rasterization_state_create_info.depthBiasEnable = VK_TRUE;
    vk_pipeline_rasterization_state_create_info.depthBiasSlopeFactor = 2.0f;
    vk_pipeline_rasterization_state_create_info.depthBiasConstantFactor = 1.0f;
    vk_pipeline_rasterization_state_create_info.depthBiasClamp = 0.0f;
  }
  else {
    vk_pipeline_rasterization_state_create_info.depthBiasEnable = VK_FALSE;
  }
  vk_pipeline_rasterization_state_create_info.frontFace = graphics_info.state.invert_facing ?
                                                              VK_FRONT_FACE_COUNTER_CLOCKWISE :
                                                              VK_FRONT_FACE_CLOCKWISE;
}

static void build_dynamic_state(
    const VKGraphicsInfo &graphics_info,
    VkPipelineDynamicStateCreateInfo &vk_pipeline_dynamic_state_create_info,
    Vector<VkDynamicState, 3> &vk_dynamic_states)
{
  vk_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const bool is_line_topology = ELEM(graphics_info.vertex_in.vk_topology,
                                     VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                     VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
                                     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
  if (is_line_topology) {
    vk_dynamic_states.append(VK_DYNAMIC_STATE_LINE_WIDTH);
  }
  vk_pipeline_dynamic_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                           nullptr,
                                           0,
                                           uint32_t(vk_dynamic_states.size()),
                                           vk_dynamic_states.data()};
}

static void build_depth_stencil_state(
    const VKGraphicsInfo &graphics_info,
    VkGraphicsPipelineCreateInfo &vk_graphics_pipeline_create_info,
    VkPipelineDepthStencilStateCreateInfo &vk_pipeline_depth_stencil_state_create_info)
{
  vk_pipeline_depth_stencil_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  if (graphics_info.fragment_out.depth_attachment_format != VK_FORMAT_UNDEFINED) {
    vk_graphics_pipeline_create_info.pDepthStencilState =
        &vk_pipeline_depth_stencil_state_create_info;
    vk_pipeline_depth_stencil_state_create_info.depthWriteEnable =
        (graphics_info.state.write_mask & GPU_WRITE_DEPTH) ? VK_TRUE : VK_FALSE;

    vk_pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    switch (graphics_info.state.depth_test) {
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

  if (graphics_info.fragment_out.stencil_attachment_format != VK_FORMAT_UNDEFINED) {
    vk_graphics_pipeline_create_info.pDepthStencilState =
        &vk_pipeline_depth_stencil_state_create_info;

    switch (graphics_info.state.stencil_test) {
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

    vk_pipeline_depth_stencil_state_create_info.front.compareMask =
        graphics_info.mutable_state.stencil_compare_mask;
    vk_pipeline_depth_stencil_state_create_info.front.reference =
        graphics_info.mutable_state.stencil_reference;
    vk_pipeline_depth_stencil_state_create_info.front.writeMask =
        graphics_info.mutable_state.stencil_write_mask;

    switch (graphics_info.state.stencil_op) {
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
        vk_pipeline_depth_stencil_state_create_info.back.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
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

static void build_dynamic_rendering(
    const VKGraphicsInfo &graphics_info,
    VkPipelineRenderingCreateInfo &vk_pipeline_rendering_create_info)
{
  vk_pipeline_rendering_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                       nullptr,
                                       0,
                                       graphics_info.fragment_out.color_attachment_size,
                                       graphics_info.fragment_out.color_attachment_formats.data(),
                                       graphics_info.fragment_out.depth_attachment_format,
                                       graphics_info.fragment_out.stencil_attachment_format

  };
}

static void build_color_blend_attachment_states(
    const VKGraphicsInfo &graphics_info,
    Vector<VkPipelineColorBlendAttachmentState> &vk_pipeline_color_blend_attachment_states)
{
  VkPipelineColorBlendAttachmentState attachment_state = {VK_TRUE,
                                                          VK_BLEND_FACTOR_DST_ALPHA,
                                                          VK_BLEND_FACTOR_ONE,
                                                          VK_BLEND_OP_ADD,
                                                          VK_BLEND_FACTOR_ZERO,
                                                          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                          VK_BLEND_OP_ADD,
                                                          0};

  switch (graphics_info.state.blend) {
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
  }

  if (graphics_info.state.blend == GPU_BLEND_MIN) {
    attachment_state.alphaBlendOp = VK_BLEND_OP_MIN;
    attachment_state.colorBlendOp = VK_BLEND_OP_MIN;
  }
  else if (graphics_info.state.blend == GPU_BLEND_MAX) {
    attachment_state.alphaBlendOp = VK_BLEND_OP_MAX;
    attachment_state.colorBlendOp = VK_BLEND_OP_MAX;
  }
  else if (graphics_info.state.blend == GPU_BLEND_SUBTRACT) {
    attachment_state.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    attachment_state.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
  }
  else {
    attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
  }

  if (graphics_info.state.blend != GPU_BLEND_NONE) {
    attachment_state.blendEnable = VK_TRUE;
  }
  else {
    attachment_state.blendEnable = VK_FALSE;
  }

  /* Adjust the template with the color components in the write mask. */
  if ((graphics_info.state.write_mask & GPU_WRITE_RED) != 0) {
    attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
  }
  if ((graphics_info.state.write_mask & GPU_WRITE_GREEN) != 0) {
    attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
  }
  if ((graphics_info.state.write_mask & GPU_WRITE_BLUE) != 0) {
    attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
  }
  if ((graphics_info.state.write_mask & GPU_WRITE_ALPHA) != 0) {
    attachment_state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
  }

  vk_pipeline_color_blend_attachment_states.append_n_times(
      attachment_state, graphics_info.fragment_out.color_attachment_size);
}

static void build_color_blend_state(
    const VKGraphicsInfo &graphics_info,
    const VKExtensions &extensions,
    Span<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states,
    VkPipelineColorBlendStateCreateInfo &vk_pipeline_color_blend_state_create_info)
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
  if (graphics_info.state.logic_op_xor && extensions.logic_ops) {
    vk_pipeline_color_blend_state_create_info.logicOpEnable = VK_TRUE;
    vk_pipeline_color_blend_state_create_info.logicOp = VK_LOGIC_OP_XOR;
  }
}

VkPipeline VKPipelinePool::get_or_create_graphics_pipeline(const VKGraphicsInfo &graphics_info,
                                                           const bool is_static_shader,
                                                           VkPipeline vk_pipeline_base,
                                                           StringRefNull name)
{
  VkPipelineCache vk_pipeline_cache = is_static_shader ? vk_pipeline_cache_static_ :
                                                         vk_pipeline_cache_non_static_;
  return graphics_.get_or_create(graphics_info, vk_pipeline_cache, vk_pipeline_base, name);
}

template<>
VkPipeline VKPipelineMap<VKGraphicsInfo>::create(const VKGraphicsInfo &graphics_info,
                                                 VkPipelineCache vk_pipeline_cache,
                                                 VkPipeline vk_pipeline_base,
                                                 StringRefNull name)
{
  VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info;
  VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info[] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       nullptr,
       0,
       VK_SHADER_STAGE_VERTEX_BIT,
       VK_NULL_HANDLE,
       "main",
       nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       nullptr,
       0,
       VK_SHADER_STAGE_FRAGMENT_BIT,
       VK_NULL_HANDLE,
       "main",
       nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       nullptr,
       0,
       VK_SHADER_STAGE_GEOMETRY_BIT,
       VK_NULL_HANDLE,
       "main",
       nullptr}};
  VkPipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info;
  VkPipelineVertexInputStateCreateInfo vk_pipeline_vertex_input_state_create_info;
  VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state_create_info;
  VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
      vk_pipeline_rasterization_provoking_vertex_state_info;
  VkPipelineDynamicStateCreateInfo vk_pipeline_dynamic_state_create_info;
  VkPipelineViewportStateCreateInfo vk_pipeline_viewport_state_create_info;
  VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_stencil_state_create_info;
  VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;
  Vector<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states;
  VkPipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info;

  /* Initialize VkGraphicsPipelineCreateInfo */
  VkGraphicsPipelineCreateInfo vk_graphics_pipeline_create_info;
  vk_graphics_pipeline_create_info = {};
  vk_graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  vk_graphics_pipeline_create_info.pNext = &vk_pipeline_rendering_create_info;
  vk_graphics_pipeline_create_info.stageCount = 0;
  vk_graphics_pipeline_create_info.pStages = vk_pipeline_shader_stage_create_info;
  vk_graphics_pipeline_create_info.pInputAssemblyState =
      &vk_pipeline_input_assembly_state_create_info;
  vk_graphics_pipeline_create_info.pVertexInputState = &vk_pipeline_vertex_input_state_create_info;
  vk_graphics_pipeline_create_info.pRasterizationState =
      &vk_pipeline_rasterization_state_create_info;
  vk_graphics_pipeline_create_info.pDynamicState = &vk_pipeline_dynamic_state_create_info;
  vk_graphics_pipeline_create_info.pViewportState = &vk_pipeline_viewport_state_create_info;
  vk_graphics_pipeline_create_info.pMultisampleState = &vk_pipeline_multisample_state_create_info;
  vk_graphics_pipeline_create_info.pColorBlendState = &vk_pipeline_color_blend_state_create_info;
  vk_graphics_pipeline_create_info.layout = graphics_info.vk_pipeline_layout;
  vk_graphics_pipeline_create_info.basePipelineHandle = vk_pipeline_base;

  /* Specialization constants */
  const bool do_specialization_constants = !graphics_info.specialization_constants.is_empty();
  VkSpecializationInfo vk_specialization_info;
  Array<VkSpecializationMapEntry> vk_specialization_map_entries(
      graphics_info.specialization_constants.size());
  if (do_specialization_constants) {
    for (int index : IndexRange(3)) {
      vk_pipeline_shader_stage_create_info[index].pSpecializationInfo = &vk_specialization_info;
    }
    for (uint32_t index : IndexRange(graphics_info.specialization_constants.size())) {
      vk_specialization_map_entries[index] = {
          index, uint32_t(index * sizeof(uint32_t)), sizeof(uint32_t)};
    }
    vk_specialization_info = {uint32_t(vk_specialization_map_entries.size()),
                              vk_specialization_map_entries.data(),
                              graphics_info.specialization_constants.size() * sizeof(uint32_t),
                              graphics_info.specialization_constants.data()};
  }

  /* Shader stages */
  vk_graphics_pipeline_create_info.stageCount =
      graphics_info.pre_rasterization.vk_geometry_module == VK_NULL_HANDLE ? 2 : 3;
  vk_pipeline_shader_stage_create_info[0].module =
      graphics_info.pre_rasterization.vk_vertex_module;
  vk_pipeline_shader_stage_create_info[1].module =
      graphics_info.fragment_shader.vk_fragment_module;
  vk_pipeline_shader_stage_create_info[2].module =
      graphics_info.pre_rasterization.vk_geometry_module;

  VKBackend &backend = VKBackend::get();
  VKDevice &device = backend.device;
  const VKExtensions &extensions = device.extensions_get();

  Vector<VkDynamicState, 3> vk_dynamic_states;
  build_dynamic_state(graphics_info, vk_pipeline_dynamic_state_create_info, vk_dynamic_states);
  build_input_assembly_state(graphics_info, vk_pipeline_input_assembly_state_create_info);
  build_vertex_input_state(graphics_info, vk_pipeline_vertex_input_state_create_info);
  build_multisample_state(vk_pipeline_multisample_state_create_info);
  build_viewport_state(graphics_info, vk_pipeline_viewport_state_create_info);
  build_rasterization_state(graphics_info,
                            vk_pipeline_rasterization_state_create_info,
                            vk_pipeline_rasterization_provoking_vertex_state_info);
  build_color_blend_attachment_states(graphics_info, vk_pipeline_color_blend_attachment_states);
  build_color_blend_state(graphics_info,
                          extensions,
                          vk_pipeline_color_blend_attachment_states.as_span(),
                          vk_pipeline_color_blend_state_create_info);
  build_depth_stencil_state(graphics_info,
                            vk_graphics_pipeline_create_info,
                            vk_pipeline_depth_stencil_state_create_info);
  build_dynamic_rendering(graphics_info, vk_pipeline_rendering_create_info);

  /* Build pipeline. */
  VkPipeline pipeline = VK_NULL_HANDLE;
  double start_time = BLI_time_now_seconds();
  vkCreateGraphicsPipelines(device.vk_handle(),
                            vk_pipeline_cache,
                            1,
                            &vk_graphics_pipeline_create_info,
                            nullptr,
                            &pipeline);
  double end_time = BLI_time_now_seconds();
  debug::object_label(pipeline, name);
  CLOG_DEBUG(&LOG,
             "Compiled graphics pipeline %s in %fms ",
             name.c_str(),
             (end_time - start_time) * 1000.0);
  return pipeline;
}

/* \} */

void VKPipelinePool::discard(VKDiscardPool &discard_pool, VkPipelineLayout vk_pipeline_layout)
{
  graphics_.discard(discard_pool, vk_pipeline_layout);
  compute_.discard(discard_pool, vk_pipeline_layout);
}

void VKPipelinePool::free_data()
{
  VKDevice &device = VKBackend::get().device;

  graphics_.free_data(device.vk_handle());
  compute_.free_data(device.vk_handle());

  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache_static_, nullptr);
  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache_non_static_, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Persistent cache
 * \{ */

#ifdef WITH_BUILDINFO
struct VKPipelineCachePrefixHeader {
  /* `BC` stands for "Blender Cache" + 2 bytes for file versioning. */
  uint32_t magic = 0xBC00;
  uint32_t blender_version = BLENDER_VERSION;
  uint32_t blender_version_patch = BLENDER_VERSION_PATCH;
  char commit_hash[8];
  uint32_t data_size;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t driver_version;
  uint8_t pipeline_cache_uuid[VK_UUID_SIZE];

  VKPipelineCachePrefixHeader()
  {
    const VKDevice &device = VKBackend::get().device;
    data_size = 0;
    const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();
    vendor_id = properties.vendorID;
    device_id = properties.deviceID;
    driver_version = properties.driverVersion;
    memcpy(&pipeline_cache_uuid, &properties.pipelineCacheUUID, VK_UUID_SIZE);

    memset(commit_hash, 0, sizeof(commit_hash));
    STRNCPY(commit_hash, build_hash);
  }
};

static std::string pipeline_cache_filepath_get()
{
  static char tmp_dir_buffer[1024];
  BKE_appdir_folder_caches(tmp_dir_buffer, sizeof(tmp_dir_buffer));

  std::string cache_dir = std::string(tmp_dir_buffer) + "vk-pipeline-cache" + SEP_STR;
  BLI_dir_create_recursive(cache_dir.c_str());
  std::string cache_file = cache_dir + "static.bin";
  return cache_file;
}
#endif

void VKPipelinePool::read_from_disk()
{
#ifdef WITH_BUILDINFO
  /* Don't read the shader cache when GPU debugging is enabled. When enabled we use different
   * shaders and compilation settings. Previous generated pipelines will not be used. */
  if (bool(G.debug & G_DEBUG_GPU)) {
    return;
  }

  std::string cache_file = pipeline_cache_filepath_get();
  if (!BLI_exists(cache_file.c_str())) {
    return;
  }

  /* Prevent old cache files from being deleted if they're still being used. */
  BLI_file_touch(cache_file.c_str());
  /* Read cached binary. */
  fstream file(cache_file, std::ios::binary | std::ios::in | std::ios::ate);
  std::streamsize data_size = file.tellg();
  file.seekg(0, std::ios::beg);
  void *buffer = MEM_mallocN(data_size, __func__);
  file.read(reinterpret_cast<char *>(buffer), data_size);
  file.close();

  /* Validate the prefix header. */
  VKPipelineCachePrefixHeader prefix;
  VKPipelineCachePrefixHeader &read_prefix = *static_cast<VKPipelineCachePrefixHeader *>(buffer);
  prefix.data_size = read_prefix.data_size;
  if (memcmp(&read_prefix, &prefix, sizeof(VKPipelineCachePrefixHeader)) != 0) {
    /* Headers are different, most likely the cache will not work and potentially crash the driver.
     * [https://medium.com/@zeuxcg/creating-a-robust-pipeline-cache-with-vulkan-961d09416cda]
     */
    MEM_freeN(buffer);
    CLOG_INFO(&LOG,
              "Pipeline cache on disk [%s] is ignored as it was written by a different driver or "
              "Blender version. Cache will be overwritten when exiting.",
              cache_file.c_str());
    return;
  }

  CLOG_INFO(&LOG, "Initialize static pipeline cache from disk [%s].", cache_file.c_str());
  VKDevice &device = VKBackend::get().device;
  VkPipelineCacheCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  create_info.initialDataSize = read_prefix.data_size;
  create_info.pInitialData = static_cast<uint8_t *>(buffer) + sizeof(VKPipelineCachePrefixHeader);
  VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache);
  MEM_freeN(buffer);

  vkMergePipelineCaches(device.vk_handle(), vk_pipeline_cache_static_, 1, &vk_pipeline_cache);
  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache, nullptr);
#endif
}

void VKPipelinePool::write_to_disk()
{
#ifdef WITH_BUILDINFO
  /* Don't write the pipeline cache when GPU debugging is enabled. When enabled we use different
   * shaders and compilation settings. Writing them to disk will clutter the pipeline cache. */
  if (bool(G.debug & G_DEBUG_GPU)) {
    return;
  }

  VKDevice &device = VKBackend::get().device;
  size_t data_size;
  vkGetPipelineCacheData(device.vk_handle(), vk_pipeline_cache_static_, &data_size, nullptr);
  void *buffer = MEM_mallocN(data_size, __func__);
  vkGetPipelineCacheData(device.vk_handle(), vk_pipeline_cache_static_, &data_size, buffer);

  std::string cache_file = pipeline_cache_filepath_get();
  CLOG_INFO(&LOG, "Writing static pipeline cache to disk [%s].", cache_file.c_str());

  fstream file(cache_file, std::ios::binary | std::ios::out);

  VKPipelineCachePrefixHeader header;
  header.data_size = data_size;
  file.write(reinterpret_cast<char *>(&header), sizeof(VKPipelineCachePrefixHeader));
  file.write(static_cast<char *>(buffer), data_size);

  MEM_freeN(buffer);
#endif
}

/** \} */

}  // namespace blender::gpu
