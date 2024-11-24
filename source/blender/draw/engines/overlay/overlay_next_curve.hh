/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_attribute.hh"
#include "BKE_curves.h"

#include "DNA_curves_types.h"

#include "GPU_capabilities.hh"

#include "draw_cache_impl.hh"

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

/**
 * Curve object display (including legacy curves) for both object and edit modes.
 */
class Curves : Overlay {
 private:
  PassSimple edit_curves_ps_ = {"Curve Edit"};
  PassSimple::Sub *edit_curves_points_ = nullptr;
  PassSimple::Sub *edit_curves_lines_ = nullptr;
  PassSimple::Sub *edit_curves_handles_ = nullptr;

  PassSimple edit_legacy_curve_ps_ = {"Legacy Curve Edit"};
  PassSimple::Sub *edit_legacy_curve_wires_ = nullptr;
  PassSimple::Sub *edit_legacy_curve_normals_ = nullptr;
  PassSimple::Sub *edit_legacy_curve_points_ = nullptr;
  PassSimple::Sub *edit_legacy_curve_handles_ = nullptr;

  PassSimple edit_legacy_surface_handles_ps = {"Surface Edit"};
  PassSimple::Sub *edit_legacy_surface_handles_ = nullptr;
  /* Handles that are below the geometry and are rendered with lower alpha. */
  PassSimple::Sub *edit_legacy_surface_xray_handles_ = nullptr;

  /* TODO(fclem): This is quite wasteful and expensive, prefer in shader Z modification like the
   * retopology offset. */
  View view_edit_cage = {"view_edit_cage"};
  float view_dist = 0.0f;

 public:
  /* TODO(fclem): Remove dependency on view. */
  void begin_sync(Resources &res, const State &state, const View &view)
  {
    enabled_ = state.is_space_v3d();

    if (!enabled_) {
      return;
    }

    view_dist = state.view_dist_get(view.winmat());

    {
      auto &pass = edit_curves_ps_;
      pass.init();
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      {
        auto &sub = pass.sub("Lines");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                          DRW_STATE_WRITE_DEPTH,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders.curve_edit_line.get());
        sub.bind_texture("weightTex", &res.weight_ramp_tx);
        sub.push_constant("useWeight", false);
        sub.push_constant("useGreasePencil", false);
        edit_curves_lines_ = &sub;
      }
      {
        auto &sub = pass.sub("Handles");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders.curve_edit_handles.get());
        sub.push_constant("curveHandleDisplay", int(state.overlay.handle_display));
        edit_curves_handles_ = &sub;
      }
      {
        auto &sub = pass.sub("Points");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                          DRW_STATE_WRITE_DEPTH,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders.curve_edit_points.get());
        sub.bind_texture("weightTex", &res.weight_ramp_tx);
        sub.push_constant("useWeight", false);
        sub.push_constant("useGreasePencil", false);
        sub.push_constant("doStrokeEndpoints", false);
        sub.push_constant("curveHandleDisplay", int(state.overlay.handle_display));
        edit_curves_points_ = &sub;
      }
    }

    const bool show_normals = (state.overlay.edit_flag & V3D_OVERLAY_EDIT_CU_NORMALS);
    const bool use_hq_normals = (state.scene->r.perf_flag & SCE_PERF_HQ_NORMALS) ||
                                GPU_use_hq_normals_workaround();

    {
      auto &pass = edit_legacy_curve_ps_;
      pass.init();
      {
        auto &sub = pass.sub("Wires");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders.legacy_curve_edit_wires.get());
        sub.push_constant("normalSize", 0.0f);
        edit_legacy_curve_wires_ = &sub;
      }
      if (show_normals) {
        auto &sub = pass.sub("Normals");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders.legacy_curve_edit_normals.get());
        sub.push_constant("normalSize", state.overlay.normals_length);
        sub.push_constant("use_hq_normals", use_hq_normals);
        edit_legacy_curve_normals_ = &sub;
      }
      else {
        edit_legacy_curve_normals_ = nullptr;
      }
      {
        auto &sub = pass.sub("Handles");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders.legacy_curve_edit_handles.get());
        sub.push_constant("showCurveHandles", state.overlay.handle_display != CURVE_HANDLE_NONE);
        sub.push_constant("curveHandleDisplay", int(state.overlay.handle_display));
        sub.push_constant("alpha", 1.0f);
        edit_legacy_curve_handles_ = &sub;
      }
      /* Points need to be rendered after handles. */
      {
        auto &sub = pass.sub("Points");
        sub.state_set(DRW_STATE_WRITE_COLOR, state.clipping_plane_count);
        sub.shader_set(res.shaders.legacy_curve_edit_points.get());
        sub.push_constant("showCurveHandles", state.overlay.handle_display != CURVE_HANDLE_NONE);
        sub.push_constant("curveHandleDisplay", int(state.overlay.handle_display));
        sub.push_constant("useGreasePencil", false);
        sub.push_constant("doStrokeEndpoints", false);
        edit_legacy_curve_points_ = &sub;
      }
    }

    {
      auto &pass = edit_legacy_surface_handles_ps;
      pass.init();

      auto create_sub = [&](const char *name, DRWState drw_state, float alpha) {
        auto &sub = pass.sub(name);
        sub.state_set(drw_state, state.clipping_plane_count);
        sub.shader_set(res.shaders.legacy_curve_edit_handles.get());
        sub.push_constant("showCurveHandles", state.overlay.handle_display != CURVE_HANDLE_NONE);
        sub.push_constant("curveHandleDisplay", int(state.overlay.handle_display));
        sub.push_constant("alpha", alpha);
        return &sub;
      };

      const DRWState state_xray = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_GREATER |
                                  DRW_STATE_BLEND_ALPHA;
      edit_legacy_surface_xray_handles_ = create_sub("SurfaceXrayHandles", state_xray, 0.2f);

      const DRWState state_front = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                   DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA;
      edit_legacy_surface_handles_ = create_sub("SurfaceHandles", state_front, 1.0f);
    }
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources & /*res*/,
                        const State & /*state*/) final
  {
    if (!enabled_) {
      return;
    }

    Object *ob = ob_ref.object;
    ::Curves &curves = *static_cast<::Curves *>(ob->data);
    const bool show_points = bke::AttrDomain(curves.selection_domain) == bke::AttrDomain::Point;

    if (show_points) {
      gpu::Batch *geom = DRW_curves_batch_cache_get_edit_points(&curves);
      edit_curves_points_->draw(geom, manager.unique_handle(ob_ref));
    }
    {
      gpu::Batch *geom = DRW_curves_batch_cache_get_edit_curves_handles(&curves);
      edit_curves_handles_->draw_expand(geom, GPU_PRIM_TRIS, 8, 1, manager.unique_handle(ob_ref));
    }
    {
      gpu::Batch *geom = DRW_curves_batch_cache_get_edit_curves_lines(&curves);
      edit_curves_lines_->draw(geom, manager.unique_handle(ob_ref));
    }
  }

  /* Used for legacy curves. */
  void edit_object_sync_legacy(Manager &manager, const ObjectRef &ob_ref, Resources & /*res*/)
  {
    if (!enabled_) {
      return;
    }

    ResourceHandle res_handle = manager.unique_handle(ob_ref);

    Object *ob = ob_ref.object;
    ::Curve &curve = *static_cast<::Curve *>(ob->data);

    if (ob->type == OB_CURVES_LEGACY) {
      gpu::Batch *geom = DRW_cache_curve_edge_wire_get(ob);
      edit_legacy_curve_wires_->draw(geom, res_handle);
    }
    if (edit_legacy_curve_normals_ && (curve.flag & CU_3D)) {
      gpu::Batch *geom = DRW_cache_curve_edge_normal_get(ob);
      edit_legacy_curve_normals_->draw_expand(geom, GPU_PRIM_LINES, 2, 1, res_handle);
    }
    {
      gpu::Batch *geom = DRW_cache_curve_edge_overlay_get(ob);
      if (ob->type == OB_CURVES_LEGACY) {
        edit_legacy_curve_handles_->draw_expand(geom, GPU_PRIM_TRIS, 8, 1, res_handle);
      }
      else {
        edit_legacy_surface_xray_handles_->draw_expand(geom, GPU_PRIM_TRIS, 8, 1, res_handle);
        edit_legacy_surface_handles_->draw_expand(geom, GPU_PRIM_TRIS, 8, 1, res_handle);
      }
    }
    {
      gpu::Batch *geom = DRW_cache_curve_vert_overlay_get(ob);
      edit_legacy_curve_points_->draw(geom, res_handle);
    }
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    view_edit_cage.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 0.5f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_legacy_surface_handles_ps, view);
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    view_edit_cage.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 0.5f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_curves_ps_, view_edit_cage);
    manager.submit(edit_legacy_curve_ps_, view);
  }
};

}  // namespace blender::draw::overlay
