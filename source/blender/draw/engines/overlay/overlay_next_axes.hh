/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 *
 * Draws object axes extra display.
 */

#pragma once

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

/**
 * Displays extra object axes.
 * It is toggled by Object Panel > Viewport Display > Axes.
 */
class Axes : Overlay {
  using EmptyInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple ps_ = {"Axes"};

  EmptyInstanceBuf axes_buf = {selection_type_, "object_axes"};

 public:
  Axes(const SelectionType selection_type) : selection_type_{selection_type} {};

  void begin_sync(Resources & /*res*/, const State &state) final
  {
    enabled_ = state.is_space_v3d();

    ps_.init();
    axes_buf.clear();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    Object *ob = ob_ref.object;
    if (is_from_dupli_or_set(ob)) {
      return;
    }

    if ((ob->dtx & OB_AXIS) == 0) {
      return;
    }

    ExtraInstanceData data(ob->object_to_world(), res.object_wire_color(ob_ref, state), 1.0f);
    axes_buf.append(data, res.select_id(ob_ref));
  }

  void end_sync(Resources &res, const ShapeCache &shapes, const State &state) final
  {
    if (!enabled_) {
      return;
    }
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                  state.clipping_plane_count);
    ps_.shader_set(res.shaders.extra_shape.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    res.select_bind(ps_);
    axes_buf.end_sync(ps_, shapes.arrows.get());
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
