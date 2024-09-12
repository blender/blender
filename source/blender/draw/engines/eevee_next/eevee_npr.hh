/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "eevee_shader_shared.hh"

struct bNodeTree;
struct GPUMaterial;
struct Material;

namespace blender::eevee {

class Instance;

class NPRModule {
 private:
  Instance &inst_;

  Framebuffer surface_fb_ = {"NPR.Surface"};
  PassSimple surface_ps_ = {"NPR.Surface"};
  Map<bNodeTree *, int> indices_;

  GPUTexture *direct_radiance_txs_[3] = {0};
  GPUTexture *indirect_radiance_txs_[3] = {0};
  bool use_split_radiance_;

 public:
  NPRModule(Instance &inst) : inst_(inst){};

  void init();
  void begin_sync();
  int sync_material(::Material *material);
  void end_sync();
};

}  // namespace blender::eevee
