/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BKE_grease_pencil.hh"
#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

namespace blender::draw::greasepencil {

using namespace draw;

class LayerModule {
 private:
  /** Contains all Objects in the scene. Indexed by gpObject.layer_offset + layer_id. */
  StorageVectorBuffer<gpLayer> layers_buf_ = "gp_layers_buf";

 public:
  void begin_sync()
  {
    layers_buf_.clear();
  }

  void sync(const Object * /*object*/,
            const bke::greasepencil::Layer & /*layer*/,
            bool &do_layer_blending)
  {
    /* TODO(fclem): All of this is placeholder. */
    gpLayer gp_layer;
    gp_layer.vertex_color_opacity = 0.0f;
    gp_layer.opacity = 1.0f;
    gp_layer.thickness_offset = 0.0f;
    gp_layer.tint = float4(1.0f, 1.0f, 1.0f, 0.0f);
    gp_layer.stroke_index_offset = 0.0f;

    layers_buf_.append(gp_layer);

    do_layer_blending = false;
  }

  void end_sync()
  {
    layers_buf_.push_update();
  }

  void bind_resources(PassMain::Sub &sub)
  {
    sub.bind_ssbo(GPENCIL_LAYER_SLOT, &layers_buf_);
  }

  uint object_offset_get() const
  {
    return layers_buf_.size();
  }
};

}  // namespace blender::draw::greasepencil
