/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_state_private.hh"

#include "vk_common.hh"

#include "BLI_vector.hh"

namespace blender::gpu {
class VKFrameBuffer;

class VKPipelineStateManager {
 private:
  GPUState current_;
  GPUStateMutable current_mutable_;

 public:
  VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state;
  VkPipelineColorBlendAttachmentState color_blend_attachment_template;
  Vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
  VkPipelineRasterizationStateCreateInfo rasterization_state;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state;

  VKPipelineStateManager();

  void set_state(const GPUState &state, const GPUStateMutable &mutable_state);
  void force_state(const GPUState &state, const GPUStateMutable &mutable_state);

  void finalize_color_blend_state(const VKFrameBuffer &framebuffer);

 private:
  void set_blend(eGPUBlend blend);
  void set_write_mask(eGPUWriteMask write_mask);
  void set_depth_test(eGPUDepthTest value);
  void set_stencil_test(eGPUStencilTest test, eGPUStencilOp operation);
  void set_stencil_mask(eGPUStencilTest test, const GPUStateMutable &mutable_state);
  void set_clip_distances(int new_dist_len, int old_dist_len);
  void set_logic_op(bool enable);
  void set_facing(bool invert);
  void set_backface_culling(eGPUFaceCullTest test);
  void set_provoking_vert(eGPUProvokingVertex vert);
  void set_shadow_bias(bool enable);
};

}  // namespace blender::gpu
