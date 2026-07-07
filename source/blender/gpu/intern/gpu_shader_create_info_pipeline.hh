/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_primitive.hh"

#include "gpu_state_private.hh"
#include "gpu_texture_private.hh"

namespace blender::gpu::shader {

/**
 * \brief Description of a graphical pipeline to pre-compile during shader creation.
 */
struct PipelineState {
  Vector<SpecializationConstant::Value> specialization_constants_;
  /* Vertex input */
  GPUPrimType primitive_;
  /* Pre-fragment and Fragment stage*/
  GPUState state_ = {{GPU_WRITE_COLOR}};
  uint32_t viewport_count_;
  /* Attachment formats. */
  TextureTargetFormat depth_format_;
  TextureTargetFormat stencil_format_;
  Vector<TextureTargetFormat> color_formats_;

  using Self = PipelineState;

  Self &state(GPUWriteMask write_mask,
              GPUBlend blend,
              GPUFaceCullTest culling_test,
              GPUDepthTest depth_test,
              GPUStencilTest stencil_test,
              GPUStencilOp stencil_op,
              GPUProvokingVertex provoking_vert)
  {
    state_.write_mask = write_mask;
    state_.blend = blend;
    state_.culling_test = culling_test;
    state_.depth_test = depth_test;
    state_.stencil_test = stencil_test;
    state_.stencil_op = stencil_op;
    state_.provoking_vert = provoking_vert;
    return *this;
  }

  Self &logic_op_xor()
  {
    state_.logic_op_xor = 1;
    return *this;
  }

  Self &primitive(GPUPrimType primitive_type)
  {
    primitive_ = primitive_type;
    return *this;
  }

  Self &viewports(uint32_t viewport_count)
  {
    viewport_count_ = viewport_count;
    return *this;
  }

  Self &add_specialization_constant(SpecializationConstant::Value specialization_constant)
  {
    specialization_constants_.append(specialization_constant);
    return *this;
  }

  Self &depth_format(TextureTargetFormat depth_format)
  {
    BLI_assert(bool(to_format_flag(to_texture_format(depth_format)) & GPU_FORMAT_DEPTH));
    depth_format_ = depth_format;
    return *this;
  }

  Self &stencil_format(TextureTargetFormat stencil_format)
  {
    BLI_assert(bool(to_format_flag(to_texture_format(stencil_format)) & GPU_FORMAT_STENCIL));
    stencil_format_ = stencil_format;
    return *this;
  }

  Self &color_format(TextureTargetFormat color_format)
  {
    BLI_assert(bool(to_format_flag(to_texture_format(color_format)) &
                    (GPU_FORMAT_STENCIL | GPU_FORMAT_DEPTH)) == false);
    color_formats_.append(color_format);
    return *this;
  }
};

}  // namespace blender::gpu::shader
