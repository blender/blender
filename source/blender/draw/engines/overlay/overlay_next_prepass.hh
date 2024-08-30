/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 *
 * A depth pass that write surface depth when it is needed.
 * It is also used for selecting non overlay-only objects.
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Prepass {
 private:
  const SelectionType selection_type_;

  PassMain ps_ = {"prepass"};

  bool enabled = false;

 public:
  Prepass(const SelectionType selection_type) : selection_type_(selection_type){};

  void begin_sync(Resources &res, const State &state)
  {
    enabled = !state.xray_enabled || (selection_type_ != SelectionType::DISABLED);
    if (!enabled) {
      /* Not used. But release the data. */
      ps_.init();
      return;
    }

    const View3DShading &shading = state.v3d->shading;
    bool use_cull = ((shading.type == OB_SOLID) && (shading.flag & V3D_SHADING_BACKFACE_CULLING));
    DRWState backface_cull_state = use_cull ? DRW_STATE_CULL_BACK : DRWState(0);

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state |
                  backface_cull_state);
    ps_.shader_set(res.shaders.depth_mesh.get());
    res.select_bind(ps_);
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res)
  {
    if (!enabled) {
      return;
    }

    if (ob_ref.object->dt < OB_SOLID) {
      return;
    }

    /* TODO(fclem) This function should contain what `basic_cache_populate` contained. */

    gpu::Batch *geom = nullptr;
    switch (ob_ref.object->type) {
      case OB_MESH:
        geom = DRW_cache_mesh_surface_get(ob_ref.object);
        break;
      case OB_VOLUME:
        if (selection_type_ == SelectionType::DISABLED) {
          /* Disable during display, only enable for selection. */
          /* TODO(fclem): Would be nice to have even when not selecting to occlude overlays. */
          return;
        }
        geom = DRW_cache_volume_selection_surface_get(ob_ref.object);
        break;
      default:
        break;
    }

    if (geom) {
      ResourceHandle res_handle = manager.resource_handle(ob_ref);
      ps_.draw(geom, res_handle, res.select_id(ob_ref).get());
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled) {
      return;
    }
    /* Should be fine to use the line buffer since the prepass only writes to the depth buffer. */
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
