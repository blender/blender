/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "GPU_state.hh"
#include "gpu_state_private.hh"

#include "mtl_pso_descriptor_state.hh"

namespace blender::gpu {

/* Forward Declarations. */
class MTLContext;

/**
 * State manager keeping track of the draw state and applying it before drawing.
 * Metal Implementation.
 */
class MTLStateManager : public StateManager {

 private:
  /* Current state of the associated MTLContext.
   * Avoids resetting the whole state for every change. */
  GPUState current_;
  GPUStateMutable current_mutable_;
  MTLContext *context_;

  /* Global pipeline descriptors. */
  MTLRenderPipelineStateDescriptor pipeline_descriptor_;

 public:
  MTLStateManager(MTLContext *ctx);

  void apply_state() override;
  void force_state() override;

  void issue_barrier(eGPUBarrier barrier_bits) override;

  void texture_bind(Texture *tex, GPUSamplerState sampler, int unit) override;
  void texture_unbind(Texture *tex) override;
  void texture_unbind_all() override;

  void image_bind(Texture *tex, int unit) override;
  void image_unbind(Texture *tex) override;
  void image_unbind_all() override;

  void texture_unpack_row_length_set(uint len) override;

  /* Global pipeline descriptors. */
  MTLRenderPipelineStateDescriptor &get_pipeline_descriptor()
  {
    return pipeline_descriptor_;
  }

 private:
  void set_write_mask(const eGPUWriteMask value);
  void set_depth_test(const eGPUDepthTest value);
  void set_stencil_test(const eGPUStencilTest test, const eGPUStencilOp operation);
  void set_stencil_mask(const eGPUStencilTest test, const GPUStateMutable state);
  void set_clip_distances(const int new_dist_len, const int old_dist_len);
  void set_logic_op(const bool enable);
  void set_facing(const bool invert);
  void set_backface_culling(const eGPUFaceCullTest test);
  void set_provoking_vert(const eGPUProvokingVertex vert);
  void set_shadow_bias(const bool enable);
  void set_blend(const eGPUBlend value);

  void set_state(const GPUState &state);
  void set_mutable_state(const GPUStateMutable &state);

  /* METAL State utility functions. */
  void mtl_state_init();
  void mtl_depth_range(float near, float far);
  void mtl_stencil_mask(uint mask);
  void mtl_stencil_set_func(eGPUStencilTest stencil_func, int ref, uint mask);
  void mtl_clip_plane_enable(uint i);
  void mtl_clip_plane_disable(uint i);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLStateManager")
};

/* Fence synchronization primitive. */
class MTLFence : public Fence {
 private:
  /* Using an event in this instance, as this is global for the command stream, rather than being
   * inserted at the encoder level. This has the behavior to match the GL functionality. */
  id<MTLEvent> mtl_event_ = nil;
  /* Events can be re-used multiple times. We can track a counter flagging the latest value
   * signalled. */
  uint64_t last_signalled_value_ = 0;

 public:
  MTLFence() : Fence(){};
  ~MTLFence();

  void signal() override;
  void wait() override;

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLFence")
};

}  // namespace blender::gpu
