/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_time.h"

#include "BKE_paint.hh"

#include "draw_cache.hh"
#include "draw_sculpt.hh"

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

/**
 * Make newly active mesh flash for a brief period of time.
 * This can be triggered using the "Transfer Mode" operator when in any edit mode.
 */
class ModeTransfer : Overlay {
 private:
  PassSimple ps_ = {"ModeTransfer"};

  float4 flash_color_;

  double current_time_ = 0.0;

  /* True if any object used was synced using this overlay. */
  bool any_animated_ = false;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d() && !res.is_selection();

    if (!enabled_) {
      /* Not used. But release the data. */
      ps_.init();
      return;
    }

    UI_GetThemeColor3fv(TH_VERTEX_SELECT, flash_color_);
    srgb_to_linearrgb_v4(flash_color_, flash_color_);

    current_time_ = BLI_time_now_seconds();

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                  state.clipping_plane_count);
    ps_.shader_set(res.shaders.uniform_color.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);

    any_animated_ = false;
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
    if (!draw_surface) {
      return;
    }

    const float time = current_time_ - ob_ref.object->runtime->overlay_mode_transfer_start_time;
    const float alpha = alpha_from_time_get(time);
    if (alpha == 0.0f) {
      return;
    }

    ps_.push_constant("ucolor", float4(flash_color_.xyz() * alpha, alpha));

    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d) &&
                                 !state.is_image_render;
    if (use_sculpt_pbvh) {
      ResourceHandle handle = manager.resource_handle_for_sculpt(ob_ref);

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

    any_animated_ = true;
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);

    if (any_animated_) {
      /* Request redraws until the object fades out. */
      DRW_viewport_request_redraw();
    }
  }

 private:
  static constexpr float flash_length = 0.55f;
  static constexpr float flash_alpha = 0.25f;

  static float alpha_from_time_get(const float anim_time)
  {
    if (anim_time > flash_length) {
      return 0.0f;
    }
    if (anim_time < 0.0f) {
      return 0.0f;
    }
    return (1.0f - (anim_time / flash_length)) * flash_alpha;
  }
};

}  // namespace blender::draw::overlay
