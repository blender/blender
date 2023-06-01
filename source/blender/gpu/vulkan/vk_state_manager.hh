/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_state_private.hh"

#include "BLI_array.hh"

#include "vk_sampler.hh"

namespace blender::gpu {
class VKTexture;
class VKUniformBuffer;
class VKVertexBuffer;

class VKStateManager : public StateManager {
  /* Dummy sampler for now.*/
  VKSampler sampler_;

  uint texture_unpack_row_length_ = 0;

  struct TextureBinding {
    VKTexture *texture = nullptr;
    /* bufferTextures and samplers share the same namespace. */
    VKVertexBuffer *vertex_buffer = nullptr;
  };
  struct ImageBinding {
    VKTexture *texture = nullptr;
  };
  struct UniformBufferBinding {
    VKUniformBuffer *buffer = nullptr;
  };
  Array<ImageBinding> image_bindings_;
  Array<TextureBinding> texture_bindings_;
  Array<UniformBufferBinding> uniform_buffer_bindings_;

 public:
  VKStateManager();

  void apply_state() override;
  void force_state() override;

  void issue_barrier(eGPUBarrier barrier_bits) override;

  /** Apply resources to the bindings of the active shader.*/
  void apply_bindings();

  void texture_bind(Texture *tex, GPUSamplerState sampler, int unit) override;
  void texture_unbind(Texture *tex) override;
  void texture_unbind_all() override;

  void image_bind(Texture *tex, int unit) override;
  void image_unbind(Texture *tex) override;
  void image_unbind_all() override;

  void uniform_buffer_bind(VKUniformBuffer *uniform_buffer, int slot);
  void uniform_buffer_unbind(VKUniformBuffer *uniform_buffer);

  void texel_buffer_bind(VKVertexBuffer *vertex_buffer, int slot);
  void texel_buffer_unbind(VKVertexBuffer *vertex_buffer);

  void texture_unpack_row_length_set(uint len) override;

  /**
   * Row length for unpacking host data when uploading texture data.
   *
   * When set to zero (0) host data can be assumed to be stored sequential.
   */
  uint texture_unpack_row_length_get() const;
};
}  // namespace blender::gpu
