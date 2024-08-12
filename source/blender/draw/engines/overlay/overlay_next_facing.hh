/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_paint.hh"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Facing {

 private:
  const SelectionType selection_type_;

  PassMain ps_ = {"Facing"};

  bool enabled = false;

 public:
  Facing(const SelectionType selection_type_) : selection_type_(selection_type_) {}

  void begin_sync(Resources &res, const State &state)
  {
    enabled = state.overlay.flag & V3D_OVERLAY_FACE_ORIENTATION && !state.xray_enabled &&
              selection_type_ == SelectionType::DISABLED;
    if (!enabled) {
      /* Not used. But release the data. */
      ps_.init();
      return;
    }
    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH |
                  state.clipping_state);
    ps_.shader_set(res.shaders.facing.get());
    ps_.bind_ubo("globalsBlock", &res.globals_buf);
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, const State &state)
  {
    if (!enabled) {
      return;
    }
    const bool renderable = DRW_object_is_renderable(ob_ref.object);
    const bool draw_surface = (ob_ref.object->dt >= OB_WIRE) &&
                              (renderable || (ob_ref.object->dt == OB_WIRE));
    const bool draw_facing = draw_surface && (ob_ref.object->dt >= OB_SOLID);
    if (!draw_facing) {
      return;
    }
    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d) &&
                                 !DRW_state_is_image_render();

    if (use_sculpt_pbvh) {
      /* TODO: Add sculpt mode. */
      // DRW_shgroup_call_sculpt(pd->facing_grp[is_xray], ob, false, false, false, false, false);
    }
    else {
      blender::gpu::Batch *geom = DRW_cache_object_surface_get(ob_ref.object);
      if (geom) {
        ResourceHandle res_handle = manager.resource_handle(ob_ref);
        ps_.draw(geom, res_handle);
      }
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled) {
      return;
    }
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
