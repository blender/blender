/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "draw_common_c.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Draw Point Cloud objects in edit mode.
 */
class PointClouds : Overlay {
 private:
  PassMain ps_ = {"PointCloud"};

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();
    if (!enabled_) {
      return;
    }

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                  state.clipping_plane_count);
    ps_.shader_set(res.shaders->pointcloud_points.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State & /*state*/) final
  {
    if (!enabled_) {
      return;
    }

    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);
    {
      gpu::Batch *geom = DRW_cache_pointcloud_vert_overlay_get(ob_ref.object);
      ps_.draw(geom, res_handle, res.select_id(ob_ref).get());
    }
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }
};
}  // namespace blender::draw::overlay
