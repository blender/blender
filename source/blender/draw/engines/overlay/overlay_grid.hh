/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DEG_depsgraph_query.hh"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_image.hh"
#include "ED_view3d.hh"
#include "GPU_texture.hh"

#include "draw_shader_shared.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Draw 2D or 3D grid as well at global X, Y and Z axes.
 */
class Grid : Overlay {
 private:
  /* Shader data */
  PassSimple grid_ps_ = {"grid_ps_"};
  UniformBuffer<OVERLAY_GridData> grid_ubo_;
  StorageVectorBuffer<float4> tile_pos_buf_;

  /* Config data */
  float2 grid_offs_ = float2(0.0f);
  int grid_flag_ = 0;
  int axis_flag_ = 0;
  uint num_iters_ = 0;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = init(state);
    if (!enabled_) {
      grid_ps_.init();
      return;
    }

    DRWState ps_draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;

    grid_ps_.init();
    grid_ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    grid_ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    grid_ps_.state_set(ps_draw_state);

    /* Background quad draw in UV/Image editor. */
    if (state.is_space_image()) {
      float3 tile_scale(grid_ubo_.clip_rect.x, grid_ubo_.clip_rect.y, 0.0f);
      const float4 color_back = math::interpolate(
          res.theme.colors.background, res.theme.colors.grid, 0.33);

      auto &sub = grid_ps_.sub("grid_background");
      sub.shader_set(res.shaders->grid_background.get());
      sub.state_set(ps_draw_state | DRW_STATE_DEPTH_LESS_EQUAL);
      sub.push_constant("ucolor", color_back);
      sub.push_constant("tile_scale", tile_scale);
      sub.draw(res.shapes.quad_solid.get());
    }

    /* Grid and axis line draws. */
    {
      const uint axis_vertex_count = 6;
      const uint grid_vertex_count = 4 * OVERLAY_GRID_STEPS_DRAW * grid_ubo_.num_lines;

      auto &sub = grid_ps_.sub("grid");
      sub.shader_set(res.shaders->grid.get());
      sub.state_set(ps_draw_state | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH |
                    DRW_STATE_BLEND_ADD);
      sub.bind_ubo("grid_buf", &grid_ubo_);

      for (int grid_iter = 0; grid_iter < num_iters_; grid_iter++) {
        sub.push_constant("grid_iter", grid_iter);
        if (axis_flag_) {
          sub.push_constant("grid_flag", &axis_flag_);
          sub.draw_procedural(GPUPrimType::GPU_PRIM_LINES, -1, axis_vertex_count, 0);
        }
        if (grid_flag_) {
          sub.push_constant("grid_flag", &grid_flag_);
          sub.draw_procedural(GPUPrimType::GPU_PRIM_LINES, -1, grid_vertex_count, 0);
        }
      }
    }

    /* Outline draw in UV/Image editors. */
    if (state.is_space_image()) {
      float4 theme_color;
      ui::theme::get_color_shade_4fv(TH_BACK, 60, theme_color);
      srgb_to_linearrgb_v4(theme_color, theme_color);

      auto &sub = grid_ps_.sub("wire_border");
      sub.shader_set(res.shaders->grid_image.get());
      sub.push_constant("ucolor", theme_color);
      sub.bind_ssbo("tile_pos_buf", &tile_pos_buf_);
      sub.draw(res.shapes.quad_wire.get(), tile_pos_buf_.size());
    }
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    grid_ubo_.push_update();
    GPU_framebuffer_bind(framebuffer);
    manager.submit(grid_ps_, view);
  }

 private:
  bool init(const State &state)
  {
    /* Set config flags to default values. */
    grid_flag_ = axis_flag_ = 0;

    if (state.hide_overlays) {
      return false;
    }

    if (state.is_space_v3d()) {
      return init_v3d(state);
    }
    if (state.is_space_image()) {
      return init_space_image(state);
    }
    /* Grid is currently unsupported in SPACE_NODE and such. */
    return false;
  }

  bool init_space_image(const State &state)
  {
    const View2D *v2d = &state.region->v2d;
    SpaceImage *sima = (SpaceImage *)state.space_data;

    /* Grid is currently visible in UV edit, if enabled. */
    const bool show_grid = sima->mode == SI_MODE_UV &&
                           (sima->overlay.flag & SI_OVERLAY_SHOW_GRID_BACKGROUND);
    if (!show_grid) {
      return false;
    }

    /* Configure grid flags s.t. GRID_OVER_IMAGE is taken into account. */
    grid_flag_ = SHOW_GRID | GRID_SIMA;
    if (sima->flag & SI_GRID_OVER_IMAGE) {
      grid_flag_ |= GRID_OVER_IMAGE;
    }

    /* Query grid step/level scaling; these can differ per axis. */
    std::array<float, SI_GRID_STEPS_LEN> steps_x, steps_y;
    ED_space_image_grid_steps(sima, steps_x.data(), steps_y.data(), SI_GRID_STEPS_LEN);
    for (int i : IndexRange(SI_GRID_STEPS_LEN)) {
      /* If the current level is not specified, or the same size as the previous level,
       * we apply a 10x scale to the previous level and use that. */
      float step_mult = (i > 0 && (steps_x[i] == 0.0f || steps_x[i] == steps_x[i - 1])) ? 20.0f :
                                                                                          2.0f;
      grid_ubo_.steps[i].x = steps_x[i] * step_mult;
      grid_ubo_.steps[i].z = grid_ubo_.steps[i].x;
      grid_ubo_.steps[i].y = steps_y[i] * step_mult;
    }

    /* Determine camera offset to center of v2d. */
    grid_ubo_.offset = float2(v2d->cur.xmax + v2d->cur.xmin, v2d->cur.ymax + v2d->cur.ymin) - 1.0f;

    /* Query grid image zoom level. Then find the lowest relevant grid level + fractional. */
    float dist = ED_space_image_zoom_level(v2d, SI_GRID_STEPS_LEN) * 4.0f;
    for (int i : IndexRange(SI_GRID_STEPS_LEN + 1)) {
      float prev = (i > 0) ? std::min(grid_ubo_.steps[i - 1].x, grid_ubo_.steps[i - 1].y) : 0.0f;
      float curr = (i < OVERLAY_GRID_STEPS_LEN) ?
                       std::min(grid_ubo_.steps[i].x, grid_ubo_.steps[i].y) :
                       std::numeric_limits<float>::infinity();
      if (curr >= dist || i == OVERLAY_GRID_STEPS_LEN) {
        grid_ubo_.level = static_cast<float>(i) + safe_divide(dist - prev, curr - prev);
        break;
      }
    }

    /* Clip rectangle can be specified in UV editor view. */
    grid_ubo_.clip_rect.x = float(sima->tile_grid_shape[0]);
    grid_ubo_.clip_rect.y = float(sima->tile_grid_shape[1]);

    /* Outline draws lines around tile grid */
    tile_pos_buf_.clear();
    for (const int x : IndexRange(sima->tile_grid_shape[0])) {
      for (const int y : IndexRange(sima->tile_grid_shape[1])) {
        tile_pos_buf_.append(float4(x, y, 0.0f, 0.0f));
      }
    }
    tile_pos_buf_.push_update();

    /* This suffices for most cases, and in others we fade to hide it. */
    grid_ubo_.num_lines = 301u;
    num_iters_ = 1u;

    return true;
  }

  bool init_v3d(const State &state)
  {
    /* Query different options from overlay state */
    const bool show_axis_x = (state.v3d_gridflag & V3D_SHOW_X) != 0;
    const bool show_axis_y = (state.v3d_gridflag & V3D_SHOW_Y) != 0;
    const bool show_axis_z = (state.v3d_gridflag & V3D_SHOW_Z) != 0;
    const bool show_persp = (state.v3d_gridflag & V3D_SHOW_FLOOR) != 0;
    const bool show_ortho = (state.v3d_gridflag & V3D_SHOW_ORTHO_GRID) != 0;
    const bool show_any = show_axis_x || show_axis_y || show_axis_z || show_persp || show_ortho;

    if (!show_any) {
      return false;
    }

    const View3D *v3d = state.v3d;
    const RegionView3D *rv3d = state.rv3d;

    /* Set `grid_flag_` dependent on view configuration. */
    if (rv3d->is_persp || rv3d->view == RV3D_VIEW_USER) {
      /* Perspective; set selected axes and floor bits. */
      axis_flag_ |= (show_axis_x ? (AXIS_X | SHOW_AXES) : OVERLAY_GridBits(0));
      axis_flag_ |= (show_axis_y ? (AXIS_Y | SHOW_AXES) : OVERLAY_GridBits(0));
      axis_flag_ |= (show_axis_z ? (AXIS_Z | SHOW_AXES) : OVERLAY_GridBits(0));
      grid_flag_ |= (show_persp ? (PLANE_XY | SHOW_GRID) : OVERLAY_GridBits(0));

      /* Axes are passed to the grid flag for correct occlusion. */
      if (grid_flag_) {
        grid_flag_ |= (show_axis_x ? AXIS_X : OVERLAY_GridBits(0));
        grid_flag_ |= (show_axis_y ? AXIS_Y : OVERLAY_GridBits(0));
        grid_flag_ |= (show_axis_z ? AXIS_Z : OVERLAY_GridBits(0));
      }
    }
    else {
      /* Orthographic; set selected axes and plane bits dependent on the specific view
       * (top, right, left, etc.) that is selected. */
      if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
        axis_flag_ = (show_axis_y ? AXIS_Y : OVERLAY_GridBits(0)) |
                     (show_axis_z ? AXIS_Z : OVERLAY_GridBits(0));
        grid_flag_ = axis_flag_ | PLANE_YZ;
      }
      else if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        axis_flag_ = (show_axis_x ? AXIS_X : OVERLAY_GridBits(0)) |
                     (show_axis_y ? AXIS_Y : OVERLAY_GridBits(0));
        grid_flag_ = axis_flag_ | PLANE_XY;
      }
      else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
        axis_flag_ = (show_axis_x ? AXIS_X : OVERLAY_GridBits(0)) |
                     (show_axis_z ? AXIS_Z : OVERLAY_GridBits(0));
        grid_flag_ = axis_flag_ | PLANE_XZ;
      }
      grid_flag_ |= (show_ortho ? SHOW_GRID : OVERLAY_GridBits(0));
      axis_flag_ |= (show_ortho ? SHOW_AXES : OVERLAY_GridBits(0));
    }

    /* Query grid scales from unit/scaling; this range suffices for user-visible levels. */
    Array<float, SI_GRID_STEPS_LEN> steps(SI_GRID_STEPS_LEN);
    ED_view3d_grid_steps(state.scene, v3d, rv3d, steps.data());
    for (int i : IndexRange(SI_GRID_STEPS_LEN)) {
      /* If the current level is not specified, or the same size as the previous level,
       * we apply a 10x scale to the previous level and use that. */
      if (i > 0 && (steps[i] == 0.0f || steps[i] == steps[i - 1])) {
        grid_ubo_.steps[i] = grid_ubo_.steps[i - 1] * 10.0f;
      }
      else {
        grid_ubo_.steps[i] = float4(steps[i]);
      }
    }

    /* Camera parameters. */
    float3 drw_view_position = rv3d->viewinv[3], drw_view_forward = rv3d->viewinv[2];

    /* Compute distance to a relevant floor point-of-interest from the camera. The grid translates
     * with this point and is only drawn around it. */
    float dist;
    if (rv3d->is_persp) {
      /* Scale depends on distance to a point on the floor plane; we interpolate between the
       * point viewed by the camera and the point directly below it, dependent on azimuth. */
      dist = interpolate(abs(drw_view_position.z / drw_view_forward.z),
                         abs(drw_view_position.z),
                         1.0f - abs(drw_view_forward.z));
    }
    else {
      /* Scale is simply specified by orthographic view. */
      dist = rv3d->dist;
    }

    /* Extract 2D grid offset for moving grid "with the camera" on the floor plane. */
    if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
      grid_ubo_.offset = drw_view_position.yz();
    }
    else if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
      grid_ubo_.offset = drw_view_position.xy();
    }
    else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
      grid_ubo_.offset = float2(drw_view_position.x, drw_view_position.z);
    }
    else if (rv3d->is_persp) {
      float3 camera_offs = drw_view_position - dist * drw_view_forward;
      grid_ubo_.offset = camera_offs.xy();
    }
    else { /* Orthographic, Image/UV view. */
      grid_ubo_.offset = drw_view_position.xy();
    }

    /* Find the lowest relevant grid level + fractional. */
    for (int i : IndexRange(SI_GRID_STEPS_LEN)) {
      float curr = std::min(grid_ubo_.steps[i].x, grid_ubo_.steps[i].y);
      float next = (i < SI_GRID_STEPS_LEN - 1) ?
                       std::min(grid_ubo_.steps[i + 1].x, grid_ubo_.steps[i + 1].y) :
                       curr * 10.0f;
      if (next >= dist || i == OVERLAY_GRID_STEPS_LEN - 1) {
        grid_ubo_.level = static_cast<float>(i) + safe_divide(dist - curr, next - curr);
        break;
      }
    }

    /* Set clipping rectangle for lines, dependent on camera/viewport. */
    /* TODO(not_mark): use for finite grid clipping. */
    if (rv3d->persp == RV3D_CAMOB && v3d->camera && v3d->camera->type == OB_CAMERA) {
      Object *camera_object = DEG_get_evaluated(state.depsgraph, v3d->camera);
      grid_flag_ |= GRID_CAMERA;
      axis_flag_ |= GRID_CAMERA;

      float clip_dist = ((Camera *)(camera_object->data))->clip_end;
      grid_ubo_.clip_rect = float2(clip_dist);
    }
    else {
      /* WATCH(not_mark): This appears to function in ortho/VR, but I'm not convinced. */
      bool use_clip_end = rv3d->is_persp ||
                          ((v3d->flag & (V3D_XR_SESSION_SURFACE | V3D_XR_SESSION_MIRROR)) != 0);
      float clip_dist = use_clip_end ? v3d->clip_end :
                                       (4.0f / max(rv3d->winmat[0][0], rv3d->winmat[1][1]));
      grid_ubo_.clip_rect = float2(clip_dist);
    }

    /* This suffices for most cases, and in others we fade to hide it. */
    /* TODO (not_mark): make this view-dependent in orthographic to have full coverage */
    grid_ubo_.num_lines = rv3d->is_persp ? 151u : 301u;
    num_iters_ = rv3d->is_persp ? OVERLAY_GRID_ITER_LEN : 1u;

    return true;
  }
};
}  // namespace blender::draw::overlay
