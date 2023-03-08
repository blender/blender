/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_state_private.hh"

namespace blender::gpu {
class VKStateManager : public StateManager {
 public:
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
};
}  // namespace blender::gpu