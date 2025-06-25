/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_map.hh"

#include "BKE_paint.hh"

#include "draw_cache.hh"
#include "draw_sculpt.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Make newly active mesh flash for a brief period of time.
 * This can be triggered using the "Transfer Mode" operator when in any edit mode.
 */
class ModeTransfer : Overlay {
 private:
  PassSimple ps_ = {"ModeTransfer"};

  Map<std::string, float, 1> object_factors_;

  float4 flash_color_;

 public:
  void begin_sync(Resources &res, const State &state) final;

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const std::optional<float> alpha_opt = object_factors_.lookup_try_as(ob_ref.object->id.name);
    if (!alpha_opt) {
      return;
    }

    const bool renderable = DRW_object_is_renderable(ob_ref.object);
    const bool draw_surface = (ob_ref.object->dt >= OB_WIRE) &&
                              (renderable || (ob_ref.object->dt == OB_WIRE));
    if (!draw_surface) {
      return;
    }

    constexpr float flash_alpha = 0.25f;
    const float alpha = *alpha_opt * flash_alpha;

    ps_.push_constant("ucolor", float4(flash_color_.xyz() * alpha, alpha));

    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d) &&
                                 !state.is_image_render;
    if (use_sculpt_pbvh) {
      ResourceHandleRange handle = manager.unique_handle_for_sculpt(ob_ref);

      for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
        ps_.draw(batch.batch, handle);
      }
    }
    else {
      gpu::Batch *geom = DRW_cache_object_surface_get((Object *)ob_ref.object);
      if (geom) {
        ps_.draw(geom, manager.unique_handle(ob_ref));
      }
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);

    /* Request redraws until the object fades out (enabled_ will be reset to false). */
    DRW_viewport_request_redraw();
  }
};

}  // namespace blender::draw::overlay
