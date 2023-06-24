/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_pipeline_state.hh"
#include "vk_framebuffer.hh"
#include "vk_texture.hh"

namespace blender::gpu {
VKPipelineStateManager::VKPipelineStateManager()
{
  rasterization_state = {};
  rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state.lineWidth = 1.0f;
  rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;

  pipeline_color_blend_state = {};
  pipeline_color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

  depth_stencil_state = {};
  depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

  color_blend_attachment_template = {};
  color_blend_attachment_template.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                   VK_COLOR_COMPONENT_G_BIT |
                                                   VK_COLOR_COMPONENT_B_BIT |
                                                   VK_COLOR_COMPONENT_A_BIT;
}

void VKPipelineStateManager::set_state(const GPUState &state, const GPUStateMutable &mutable_state)
{
  GPUState changed = state ^ current_;
  if (changed.blend) {
    set_blend(static_cast<eGPUBlend>(state.blend));
  }
  if (changed.write_mask != 0) {
    set_write_mask((eGPUWriteMask)state.write_mask);
  }
  if (changed.depth_test != 0) {
    set_depth_test((eGPUDepthTest)state.depth_test);
  }
  if (changed.stencil_test != 0 || changed.stencil_op != 0) {
    set_stencil_test((eGPUStencilTest)state.stencil_test, (eGPUStencilOp)state.stencil_op);
    set_stencil_mask((eGPUStencilTest)state.stencil_test, mutable_state);
  }
  if (changed.clip_distances != 0) {
    set_clip_distances(state.clip_distances, current_.clip_distances);
  }
  if (changed.culling_test != 0) {
    set_backface_culling((eGPUFaceCullTest)state.culling_test);
  }
  if (changed.logic_op_xor != 0) {
    set_logic_op(state.logic_op_xor);
  }
  if (changed.invert_facing != 0) {
    set_facing(state.invert_facing);
  }
  if (changed.provoking_vert != 0) {
    set_provoking_vert((eGPUProvokingVertex)state.provoking_vert);
  }
  if (changed.shadow_bias != 0) {
    set_shadow_bias(state.shadow_bias);
  }
  current_ = state;
}

void VKPipelineStateManager::force_state(const GPUState &state,
                                         const GPUStateMutable &mutable_state)
{
  current_ = ~state;
  set_state(state, mutable_state);
}

void VKPipelineStateManager::finalize_color_blend_state(const VKFrameBuffer &framebuffer)
{
  color_blend_attachments.clear();
  if (framebuffer.is_immutable()) {
    /* Immutable frame-buffers are owned by GHOST and don't have any attachments assigned. In this
     * case we assume that there is a single color texture assigned. */
    color_blend_attachments.append(color_blend_attachment_template);
  }
  else {

    bool is_sequential = true;
    for (int color_slot = 0; color_slot < GPU_FB_MAX_COLOR_ATTACHMENT; color_slot++) {
      VKTexture *texture = unwrap(unwrap(framebuffer.color_tex(color_slot)));
      if (texture) {
        BLI_assert(is_sequential);
        color_blend_attachments.append(color_blend_attachment_template);
      }
      else {
        /* Test to detect if all color textures are sequential attached from the first slot. We
         * assume at this moment that this is the case. Otherwise we need to rewire how attachments
         * and bindings work. */
        is_sequential = false;
      }
    }
    UNUSED_VARS_NDEBUG(is_sequential);
  }

  pipeline_color_blend_state.attachmentCount = color_blend_attachments.size();
  pipeline_color_blend_state.pAttachments = color_blend_attachments.data();
}

void VKPipelineStateManager::set_blend(const eGPUBlend blend)
{
  VkPipelineColorBlendStateCreateInfo &cb = pipeline_color_blend_state;
  VkPipelineColorBlendAttachmentState &att_state = color_blend_attachment_template;

  att_state.blendEnable = VK_TRUE;
  att_state.alphaBlendOp = VK_BLEND_OP_ADD;
  att_state.colorBlendOp = VK_BLEND_OP_ADD;
  att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
  att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;

  switch (blend) {
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

  if (blend == GPU_BLEND_SUBTRACT) {
    att_state.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    att_state.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
  }
  else {
    att_state.alphaBlendOp = VK_BLEND_OP_ADD;
    att_state.colorBlendOp = VK_BLEND_OP_ADD;
  }

  if (blend != GPU_BLEND_NONE) {
    att_state.blendEnable = VK_TRUE;
  }
  else {
    att_state.blendEnable = VK_FALSE;
  }
}

void VKPipelineStateManager::set_write_mask(const eGPUWriteMask write_mask)
{
  depth_stencil_state.depthWriteEnable = (write_mask & GPU_WRITE_DEPTH) ? VK_TRUE : VK_FALSE;

  color_blend_attachment_template.colorWriteMask = 0;

  if ((write_mask & GPU_WRITE_RED) != 0) {
    color_blend_attachment_template.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
  }
  if ((write_mask & GPU_WRITE_GREEN) != 0) {
    color_blend_attachment_template.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
  }
  if ((write_mask & GPU_WRITE_BLUE) != 0) {
    color_blend_attachment_template.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
  }
  if ((write_mask & GPU_WRITE_ALPHA) != 0) {
    color_blend_attachment_template.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
  }
}

void VKPipelineStateManager::set_depth_test(const eGPUDepthTest value)
{
  switch (value) {
    case GPU_DEPTH_LESS:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
      break;
    case GPU_DEPTH_LESS_EQUAL:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
      break;
    case GPU_DEPTH_EQUAL:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_EQUAL;
      break;
    case GPU_DEPTH_GREATER:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER;
      break;
    case GPU_DEPTH_GREATER_EQUAL:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
      break;
    case GPU_DEPTH_ALWAYS:
    default:
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
  }

  if (value != GPU_DEPTH_NONE) {
    depth_stencil_state.depthTestEnable = VK_TRUE;
  }
  else {
    depth_stencil_state.depthTestEnable = VK_FALSE;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
  }
}

void VKPipelineStateManager::set_stencil_test(const eGPUStencilTest test,
                                              const eGPUStencilOp operation)
{
  depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
  depth_stencil_state.front.compareMask = 0;
  depth_stencil_state.front.reference = 0;

  switch (operation) {
    case GPU_STENCIL_OP_REPLACE:
      depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_REPLACE;
      depth_stencil_state.back = depth_stencil_state.front;
      break;

    case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
      depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
      depth_stencil_state.back = depth_stencil_state.front;
      depth_stencil_state.back.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      break;

    case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
      depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.back = depth_stencil_state.front;
      depth_stencil_state.back.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
      break;

    case GPU_STENCIL_OP_NONE:
    default:
      depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
      depth_stencil_state.back = depth_stencil_state.front;
      break;
  }

  if (test != GPU_STENCIL_NONE) {
    depth_stencil_state.stencilTestEnable = VK_TRUE;
  }
  else {
    depth_stencil_state.stencilTestEnable = VK_FALSE;
  }
}

void VKPipelineStateManager::set_stencil_mask(const eGPUStencilTest test,
                                              const GPUStateMutable &mutable_state)
{
  depth_stencil_state.front.writeMask = static_cast<uint32_t>(mutable_state.stencil_write_mask);
  depth_stencil_state.front.reference = static_cast<uint32_t>(mutable_state.stencil_reference);

  depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
  depth_stencil_state.front.compareMask = static_cast<uint32_t>(
      mutable_state.stencil_compare_mask);

  switch (test) {
    case GPU_STENCIL_NEQUAL:
      depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
      break;
    case GPU_STENCIL_EQUAL:
      depth_stencil_state.front.compareOp = VK_COMPARE_OP_EQUAL;
      break;
    case GPU_STENCIL_ALWAYS:
      depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
      break;
    case GPU_STENCIL_NONE:
    default:
      depth_stencil_state.front.compareMask = 0x00;
      depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
      return;
  }

  depth_stencil_state.back = depth_stencil_state.front;
}

void VKPipelineStateManager::set_clip_distances(const int /*new_dist_len*/,
                                                const int /*old_dist_len*/)
{
  /* TODO: needs to be implemented. */
}

void VKPipelineStateManager::set_logic_op(const bool enable)
{
  if (enable) {
    pipeline_color_blend_state.logicOpEnable = VK_TRUE;
    pipeline_color_blend_state.logicOp = VK_LOGIC_OP_XOR;
  }
  else {
    pipeline_color_blend_state.logicOpEnable = VK_FALSE;
  }
}

void VKPipelineStateManager::set_facing(const bool invert)
{
  rasterization_state.frontFace = invert ? VK_FRONT_FACE_COUNTER_CLOCKWISE :
                                           VK_FRONT_FACE_CLOCKWISE;
}

void VKPipelineStateManager::set_backface_culling(const eGPUFaceCullTest cull_test)
{
  rasterization_state.cullMode = to_vk_cull_mode_flags(cull_test);
}

void VKPipelineStateManager::set_provoking_vert(const eGPUProvokingVertex /*vert*/)
{
  /* TODO: Requires VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, See:
   * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineRasterizationProvokingVertexStateCreateInfoEXT.html
   */
}

void VKPipelineStateManager::set_shadow_bias(const bool enable)
{
  if (enable) {
    rasterization_state.depthBiasEnable = VK_TRUE;
    rasterization_state.depthBiasSlopeFactor = 2.f;
    rasterization_state.depthBiasConstantFactor = 1.f;
    rasterization_state.depthBiasClamp = 0.f;
  }
  else {
    rasterization_state.depthBiasEnable = VK_FALSE;
  }
}

}  // namespace blender::gpu
