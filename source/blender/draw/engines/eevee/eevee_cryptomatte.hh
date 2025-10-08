/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Cryptomatte.
 *
 * During rasterization, cryptomatte hashes are stored into a single array texture.
 * The film pass then resamples this texture using pixel filter weighting.
 * Each cryptomatte layer can hold N samples. These are stored in sequential layers
 * of the array texture. The samples are sorted and merged only for final rendering.
 */

#pragma once

#include "DRW_gpu_wrapper.hh"

#include "BKE_cryptomatte.hh"

#include "draw_handle.hh"

#include "eevee_defines.hh"

extern "C" {
struct Material;
}

namespace blender::eevee {

using namespace draw;

class Instance;

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

class Cryptomatte {
 private:
  class Instance &inst_;

  using CryptomatteObjectBuf = draw::StorageArrayBuffer<float2, 16>;

  bke::cryptomatte::CryptomatteSessionPtr session_;

  /* Cached pointer to the cryptomatte layer instances. */
  bke::cryptomatte::CryptomatteLayer *object_layer_ = nullptr;
  bke::cryptomatte::CryptomatteLayer *asset_layer_ = nullptr;
  bke::cryptomatte::CryptomatteLayer *material_layer_ = nullptr;

  /** Contains per object hashes (object and asset hash). Indexed by resource ID. */
  CryptomatteObjectBuf cryptomatte_object_buf;

 public:
  Cryptomatte(Instance &inst) : inst_(inst) {};

  void begin_sync();
  void sync_object(Object *ob, ResourceHandleRange res_handle);
  void sync_material(const ::Material *material);
  void end_sync();

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_ssbo(CRYPTOMATTE_BUF_SLOT, &cryptomatte_object_buf);
  }

  /* Register ID to use inside cryptomatte layer and returns associated hash as float. */
  float register_id(const eViewLayerEEVEEPassType layer, const ID &id) const;
  void store_metadata(RenderResult *render_result);
};

/** \} */

}  // namespace blender::eevee
