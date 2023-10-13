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

#include "DRW_render.h"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Hierarchical-Z buffer
 * \{ */

class HiZBuffer {
 private:
  Instance &inst_;

  /** The texture containing the hiz mip chain. */
  Texture hiz_tx_ = {"hiz_tx_"};
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
  GPUTexture *src_tx_;
  GPUTexture **src_tx_ptr_;

  HiZData &data_;

 public:
  HiZBuffer(Instance &inst, HiZData &data) : inst_(inst), data_(data)
  {
    atomic_tile_counter_.clear_to_zero();
  };

  void sync();

  /**
   * Set source texture for the hiz downsampling.
   */
  void set_source(GPUTexture **texture, int layer = -1)
  {
    src_tx_ptr_ = texture;
    layer_id_ = layer;
  }

  /**
   * Tag the buffer for update if needed.
   */
  void set_dirty()
  {
    is_dirty_ = true;
  }

  /**
   * Update the content of the HiZ buffer with the depth render target.
   * Noop if the buffer has not been tagged as dirty.
   * Should be called before each passes that needs to read the hiz buffer.
   */
  void update();

  void debug_draw(View &view, GPUFrameBuffer *view_fb);

  void bind_resources(DRWShadingGroup *grp)
  {
    DRW_shgroup_uniform_texture_ref(grp, "hiz_tx", &hiz_tx_);
  }

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(HIZ_TEX_SLOT, &hiz_tx_);
  }
};

/** \} */

}  // namespace blender::eevee
