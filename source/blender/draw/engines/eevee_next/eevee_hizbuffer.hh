/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *  */

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
  /** Single pass recursive downsample. */
  PassSimple hiz_update_ps_ = {"HizUpdate"};
  /** Debug pass. */
  PassSimple debug_draw_ps_ = {"HizUpdate.Debug"};
  /** Dirty flag to check if the update is necessary. */
  bool is_dirty_ = true;

  HiZDataBuf data_;

 public:
  HiZBuffer(Instance &inst) : inst_(inst)
  {
    atomic_tile_counter_.clear_to_zero();
  };

  void sync();

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
    DRW_shgroup_uniform_block_ref(grp, "hiz_buf", &data_);
  }

  /* TODO(fclem): Hardcoded bind slots. */
  template<typename T> void bind_resources(draw::detail::PassBase<T> *pass)
  {
    pass->bind_texture("hiz_tx", &hiz_tx_);
    pass->bind_ubo("hiz_buf", &data_);
  }
};

/** \} */

}  // namespace blender::eevee
