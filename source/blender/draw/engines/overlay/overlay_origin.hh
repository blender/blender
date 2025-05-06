/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_layer.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Display object origins as dots.
 * The option can be found under (Viewport Overlays > Objects > Origins).
 */
class Origins : Overlay {
 private:
  StorageVectorBuffer<VertexData> point_buf_;
  select::SelectBuf select_buf_;

  PassSimple ps_ = {"Origins"};

 public:
  Origins(SelectionType selection_type) : select_buf_(selection_type) {}

  void begin_sync(Resources & /*res*/, const State &state) final
  {
    const bool is_paint_mode = (state.object_mode &
                                (OB_MODE_ALL_PAINT | OB_MODE_ALL_PAINT_GPENCIL |
                                 OB_MODE_SCULPT_CURVES)) != 0;
    enabled_ = state.is_space_v3d() && !is_paint_mode && state.show_object_origins();
    point_buf_.clear();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    /* Don't show object extras in set's. */
    if (is_from_dupli_or_set(ob_ref)) {
      return;
    }

    const Object *ob = ob_ref.object;
    const bool is_library = ID_REAL_USERS(&ob->id) > 1 || ID_IS_LINKED(ob);
    BKE_view_layer_synced_ensure(state.scene, (ViewLayer *)state.view_layer);
    const float4 location = float4(ob->object_to_world().location(), 0.0f);

    if (ob == BKE_view_layer_active_object_get(state.view_layer)) {
      select_buf_.select_append(res.select_id(ob_ref));
      point_buf_.append(VertexData{location, res.theme.colors.active_object});
    }
    else if (ob->base_flag & BASE_SELECTED) {
      select_buf_.select_append(res.select_id(ob_ref));
      point_buf_.append(VertexData{location,
                                   is_library ? res.theme.colors.library_select :
                                                res.theme.colors.object_select});
    }
    else if (state.v3d_flag & V3D_DRAW_CENTERS) {
      select_buf_.select_append(res.select_id(ob_ref));
      point_buf_.append(
          VertexData{location, is_library ? res.theme.colors.library : res.theme.colors.deselect});
    }
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!enabled_) {
      return;
    }
    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
    res.select_bind(ps_);
    ps_.shader_set(res.shaders->extra_point.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    select_buf_.select_bind(ps_);
    point_buf_.push_update();
    ps_.bind_ssbo("data_buf", &point_buf_);
    ps_.draw_procedural(GPU_PRIM_POINTS, 1, point_buf_.size());
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};
}  // namespace blender::draw::overlay
