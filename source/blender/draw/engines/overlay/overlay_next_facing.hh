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
    enabled = state.v3d && (state.overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) &&
              !state.xray_enabled && (selection_type_ == SelectionType::DISABLED);
    if (!enabled) {
      /* Not used. But release the data. */
      ps_.init();
      return;
    }

    const View3DShading &shading = state.v3d->shading;
    const bool is_solid_viewport = shading.type == OB_SOLID;
    bool use_cull = (is_solid_viewport && (shading.flag & V3D_SHADING_BACKFACE_CULLING));
    DRWState backface_cull_state = use_cull ? DRW_STATE_CULL_BACK : DRWState(0);

    /* Use the Depth Equal test in solid mode to ensure transparent textures display correctly.
     * (See #128113). And the Depth-Less test in other modes (E.g. EEVEE) to ensure the overlay
     * displays correctly (See # 114000). */
    DRWState depth_compare_state = is_solid_viewport ? DRW_STATE_DEPTH_EQUAL :
                                                       DRW_STATE_DEPTH_LESS_EQUAL;

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | depth_compare_state |
                      backface_cull_state,
                  state.clipping_plane_count);
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
      ResourceHandle handle = manager.resource_handle_for_sculpt(ob_ref);

      for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
        ps_.draw(batch.batch, handle);
      }
    }
    else {
      blender::gpu::Batch *geom = DRW_cache_object_surface_get(ob_ref.object);
      if (geom) {
        ps_.draw(geom, manager.unique_handle(ob_ref));
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
