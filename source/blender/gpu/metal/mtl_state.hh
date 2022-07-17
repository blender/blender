/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "GPU_state.h"
#include "gpu_state_private.hh"

namespace blender::gpu {

/* Forward Declarations. */
class MTLContext;

/**
 * State manager keeping track of the draw state and applying it before drawing.
 * Metal Implementation.
 **/
class MTLStateManager : public StateManager {
 public:
 private:
  /* Current state of the associated MTLContext.
   * Avoids resetting the whole state for every change. */
  GPUState current_;
  GPUStateMutable current_mutable_;
  MTLContext *context_;

 public:
  MTLStateManager(MTLContext *ctx);

  void apply_state() override;
  void force_state() override;

  void issue_barrier(eGPUBarrier barrier_bits) override;

  void texture_bind(Texture *tex, eGPUSamplerState sampler, int unit) override;
  void texture_unbind(Texture *tex) override;
  void texture_unbind_all() override;

  void image_bind(Texture *tex, int unit) override;
  void image_unbind(Texture *tex) override;
  void image_unbind_all() override;

  void texture_unpack_row_length_set(uint len) override;

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

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLStateManager")
};

}  // namespace blender::gpu
