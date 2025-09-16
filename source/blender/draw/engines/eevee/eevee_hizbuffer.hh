/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The Hierarchical-Z buffer is texture containing a copy of the depth buffer with mipmaps.
 * Each mip contains the maximum depth of each 4 pixels on the upper level.
 * The size of the texture is padded to avoid messing with the mipmap pixels alignments.
 */

#pragma once

#include "DRW_render.hh"

#include "eevee_hizbuffer_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Hierarchical-Z buffer
 * \{ */

class HiZBuffer {
 private:
  Instance &inst_;

  /** Contains depth pyramid of the current pass and the previous pass. */
  SwapChain<Texture, 2> hiz_tx_;
  /** References to the mip views of the current (front) HiZ texture. */
  std::array<gpu::Texture *, HIZ_MIP_COUNT> hiz_mip_ref_;

  /**
   * Atomic counter counting the number of tile that have finished down-sampling.
   * The last one will process the last few mip level.
   */
  draw::StorageBuffer<uint4, true> atomic_tile_counter_ = {"atomic_tile_counter"};
  /** Single pass recursive down-sample. */
  PassSimple hiz_update_ps_ = {"HizUpdate"};
  /** Single pass recursive down-sample for layered depth buffer. Only downsample 1 layer. */
  PassSimple hiz_update_layer_ps_ = {"HizUpdate.Layer"};
  int layer_id_ = -1;
  /** Debug pass. */
  PassSimple debug_draw_ps_ = {"HizUpdate.Debug"};
  /** Dirty flag to check if the update is necessary. */
  bool is_dirty_ = true;
  /** Reference to the depth texture to downsample. */
  gpu::Texture *src_tx_ = nullptr;
  gpu::Texture **src_tx_ptr_ = nullptr;

  HiZData &data_;

 public:
  HiZBuffer(Instance &inst, HiZData &data) : inst_(inst), data_(data)
  {
    atomic_tile_counter_.clear_to_zero();
  };

  void sync();

  /**
   * Set source texture for the hiz down-sampling.
   * Need to be called once at the start of a pipeline or view.
   * Tag the buffer as dirty.
   */
  void set_source(gpu::Texture **texture, int layer = -1)
  {
    src_tx_ptr_ = texture;
    layer_id_ = layer;
    swap_layer();
  }

  /**
   * Swap front and back layer.
   * Internally set front layer to be dirty.
   * IMPORTANT: Before the second swap (and the second update)
   * the content of the back hi-z buffer is undefined.
   */
  void swap_layer()
  {
    hiz_tx_.swap();
    back.ref_tx_ = hiz_tx_.previous();
    front.ref_tx_ = hiz_tx_.current();
    set_dirty();
  }

  /**
   * Tag the front buffer for update if needed.
   */
  void set_dirty()
  {
    is_dirty_ = true;
  }

  /**
   * Update the content of the HiZ buffer with the source depth set by `set_source()`.
   * Noop if the buffer has not been tagged as dirty.
   * Should be called before each passes that needs to read the hiz buffer.
   */
  void update();

  void debug_draw(View &view, gpu::FrameBuffer *view_fb);

  /* Back is Previous layer depth (ex: For refraction). Front for current layer depth. */
  struct {
    /** References to the textures in the swap-chain. */
    gpu::Texture *ref_tx_ = nullptr;

    template<typename PassType> void bind_resources(PassType &pass)
    {
      pass.bind_texture(HIZ_TEX_SLOT, &ref_tx_);
    }
  } front, back;
};

/** \} */

}  // namespace blender::eevee
