/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_paint.hh"

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

class Facing : Overlay {

 private:
  PassMain ps_ = {"Facing"};

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.v3d && state.show_face_orientation() && !state.xray_enabled &&
               !res.is_selection();
    if (!enabled_) {
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

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    if (!enabled_) {
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
                                 !state.is_image_render;

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

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }
};

}  // namespace blender::draw::overlay
