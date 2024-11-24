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
#include "overlay_next_base.hh"

namespace blender::draw::overlay {

/**
 * Draw 2D or 3D grid as well at global X, Y and Z axes.
 */
class Grid : Overlay {
 private:
  UniformBuffer<OVERLAY_GridData> data_;
  StorageVectorBuffer<float4> tile_pos_buf_;

  PassSimple grid_ps_ = {"grid_ps_"};

  bool show_axis_z_ = false;
  bool is_xr_ = false;
  bool is_space_image_ = false;
  /* Copy of v3d->dist. */
  float v3d_clip_end_ = 0.0f;

  float3 grid_axes_ = float3(0.0f);
  float3 zplane_axes_ = float3(0.0f);
  int grid_flag_ = int(0);
  int zneg_flag_ = int(0);
  int zpos_flag_ = int(0);

  const ShapeCache &shapes_;

 public:
  Grid(const ShapeCache &shapes) : shapes_(shapes){};

  void begin_sync(Resources &res, const State &state) final
  {
    is_space_image_ = state.is_space_image();

    enabled_ = init(state);
    if (!enabled_) {
      grid_ps_.init();
      return;
    }

    GPUTexture **depth_tx = state.xray_enabled ? &res.xray_depth_tx : &res.depth_tx;
    GPUTexture **depth_infront_tx = &res.depth_target_in_front_tx;

    grid_ps_.init();
    grid_ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    grid_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
    if (state.is_space_image()) {
      /* Add quad background. */
      auto &sub = grid_ps_.sub("grid_background");
      sub.shader_set(res.shaders.grid_background.get());
      const float4 color_back = math::interpolate(
          res.theme_settings.color_background, res.theme_settings.color_grid, 0.5);
      sub.push_constant("ucolor", color_back);
      sub.push_constant("tile_scale", float3(data_.size));
      sub.bind_texture("depthBuffer", depth_tx);
      sub.draw(shapes_.quad_solid.get());
    }
    {
      auto &sub = grid_ps_.sub("grid");
      sub.shader_set(res.shaders.grid.get());
      sub.bind_ubo("grid_buf", &data_);
      sub.bind_texture("depth_tx", depth_tx, GPUSamplerState::default_sampler());
      sub.bind_texture("depth_infront_tx", depth_infront_tx, GPUSamplerState::default_sampler());
      if (zneg_flag_ & SHOW_AXIS_Z) {
        sub.push_constant("grid_flag", &zneg_flag_);
        sub.push_constant("plane_axes", &zplane_axes_);
        sub.draw(shapes_.grid.get());
      }
      if (grid_flag_) {
        sub.push_constant("grid_flag", &grid_flag_);
        sub.push_constant("plane_axes", &grid_axes_);
        sub.draw(shapes_.grid.get());
      }
      if (zpos_flag_ & SHOW_AXIS_Z) {
        sub.push_constant("grid_flag", &zpos_flag_);
        sub.push_constant("plane_axes", &zplane_axes_);
        sub.draw(shapes_.grid.get());
      }
    }
    if (state.is_space_image()) {
      float4 theme_color;
      UI_GetThemeColorShade4fv(TH_BACK, 60, theme_color);
      srgb_to_linearrgb_v4(theme_color, theme_color);

      /* Add wire border. */
      auto &sub = grid_ps_.sub("wire_border");
      sub.shader_set(res.shaders.grid_image.get());
      sub.push_constant("ucolor", theme_color);
      tile_pos_buf_.clear();
      for (const int x : IndexRange(data_.size[0])) {
        for (const int y : IndexRange(data_.size[1])) {
          tile_pos_buf_.append(float4(x, y, 0.0f, 0.0f));
        }
      }
      tile_pos_buf_.push_update();
      sub.bind_ssbo("tile_pos_buf", &tile_pos_buf_);
      sub.draw(shapes_.quad_wire.get(), tile_pos_buf_.size());
    }
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    sync_view(view);
    data_.push_update();

    GPU_framebuffer_bind(framebuffer);
    manager.submit(grid_ps_, view);
  }

 private:
  bool init(const State &state)
  {
    data_.line_size = max_ff(0.0f, U.pixelsize - 1.0f) * 0.5f;
    /* Default, nothing is drawn. */
    grid_flag_ = zneg_flag_ = zpos_flag_ = 0;
    show_axis_z_ = false;

    return (state.is_space_image()) ? init_2d(state) : init_3d(state);
  }

  void copy_steps_to_data(Span<float> grid_steps_x, Span<float> grid_steps_y)
  {
    /* Convert to UBO alignment. */
    for (const int i : IndexRange(SI_GRID_STEPS_LEN)) {
      data_.steps[i][0] = grid_steps_x[i];
      data_.steps[i][1] = grid_steps_y[i];
    }
  }

  bool init_2d(const State &state)
  {
    if (state.hide_overlays) {
      return false;
    }
    SpaceImage *sima = (SpaceImage *)state.space_data;
    const View2D *v2d = &state.region->v2d;
    std::array<float, SI_GRID_STEPS_LEN> grid_steps_x = {
        0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};
    std::array<float, SI_GRID_STEPS_LEN> grid_steps_y = {0.0f};

    /* Only UV Edit mode has the various Overlay options for now. */
    const bool is_uv_edit = sima->mode == SI_MODE_UV;

    const bool background_enabled = is_uv_edit ? (!state.hide_overlays &&
                                                  (sima->overlay.flag &
                                                   SI_OVERLAY_SHOW_GRID_BACKGROUND) != 0) :
                                                 true;
    if (background_enabled) {
      grid_flag_ = GRID_BACK | PLANE_IMAGE;
      if (sima->flag & SI_GRID_OVER_IMAGE) {
        grid_flag_ = PLANE_IMAGE;
      }
    }

    const bool draw_grid = is_uv_edit || !ED_space_image_has_buffer(sima);
    if (background_enabled && draw_grid) {
      grid_flag_ |= SHOW_GRID;
      if (is_uv_edit) {
        if (sima->grid_shape_source != SI_GRID_SHAPE_DYNAMIC) {
          grid_flag_ |= CUSTOM_GRID;
        }
      }
    }

    data_.distance = 1.0f;
    data_.size = float4(1.0f);
    if (is_uv_edit) {
      data_.size[0] = float(sima->tile_grid_shape[0]);
      data_.size[1] = float(sima->tile_grid_shape[1]);
    }

    data_.zoom_factor = ED_space_image_zoom_level(v2d, SI_GRID_STEPS_LEN);
    ED_space_image_grid_steps(sima, grid_steps_x.data(), grid_steps_y.data(), SI_GRID_STEPS_LEN);
    copy_steps_to_data(grid_steps_x, grid_steps_y);
    return true;
  }

  bool init_3d(const State &state)
  {
    const View3D *v3d = state.v3d;
    const RegionView3D *rv3d = state.rv3d;

    const bool show_axis_x = (state.v3d_gridflag & V3D_SHOW_X) != 0;
    const bool show_axis_y = (state.v3d_gridflag & V3D_SHOW_Y) != 0;
    const bool show_axis_z = (state.v3d_gridflag & V3D_SHOW_Z) != 0;
    const bool show_floor = (state.v3d_gridflag & V3D_SHOW_FLOOR) != 0;
    const bool show_ortho_grid = (state.v3d_gridflag & V3D_SHOW_ORTHO_GRID) != 0;
    const bool show_any = show_axis_x || show_axis_y || show_axis_z || show_floor ||
                          show_ortho_grid;

    if (state.hide_overlays || !show_any) {
      return false;
    }

    std::array<float, SI_GRID_STEPS_LEN> grid_steps = {
        0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};

    /* If perspective view or non-axis aligned view. */
    if (rv3d->is_persp || rv3d->view == RV3D_VIEW_USER) {
      if (show_axis_x) {
        grid_flag_ |= PLANE_XY | SHOW_AXIS_X;
      }
      if (show_axis_y) {
        grid_flag_ |= PLANE_XY | SHOW_AXIS_Y;
      }
      if (show_floor) {
        grid_flag_ |= PLANE_XY | SHOW_GRID;
      }
    }
    else {
      if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
        grid_flag_ = PLANE_YZ | SHOW_AXIS_Y | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
      else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        grid_flag_ = PLANE_XY | SHOW_AXIS_X | SHOW_AXIS_Y | SHOW_GRID | GRID_BACK;
      }
      else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
        grid_flag_ = PLANE_XZ | SHOW_AXIS_X | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
    }

    grid_axes_[0] = float((grid_flag_ & (PLANE_XZ | PLANE_XY)) != 0);
    grid_axes_[1] = float((grid_flag_ & (PLANE_YZ | PLANE_XY)) != 0);
    grid_axes_[2] = float((grid_flag_ & (PLANE_YZ | PLANE_XZ)) != 0);

    /* Z axis if needed */
    if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
      zpos_flag_ = SHOW_AXIS_Z;
    }
    else {
      zneg_flag_ = zpos_flag_ = CLIP_ZNEG | CLIP_ZPOS;
    }

    if (rv3d->persp == RV3D_CAMOB && v3d->camera && v3d->camera->type == OB_CAMERA) {
      Object *camera_object = DEG_get_evaluated_object(state.depsgraph, v3d->camera);
      v3d_clip_end_ = ((Camera *)(camera_object->data))->clip_end;
      grid_flag_ |= GRID_CAMERA;
      zneg_flag_ |= GRID_CAMERA;
      zpos_flag_ |= GRID_CAMERA;
    }
    else {
      v3d_clip_end_ = v3d->clip_end;
    }

    ED_view3d_grid_steps(state.scene, v3d, rv3d, grid_steps.data());

    is_xr_ = (v3d->flag & (V3D_XR_SESSION_SURFACE | V3D_XR_SESSION_MIRROR)) != 0;

    copy_steps_to_data(grid_steps, grid_steps);
    return true;
  }

  /* Update data that depends on the view. */
  void sync_view(const View &view)
  {
    if (is_space_image_) {
      return;
    }

    if (zpos_flag_ & SHOW_AXIS_Z) {
      float3 backward = -view.forward();
      float3 position = view.location();

      /* z axis : chose the most facing plane */
      if (fabsf(backward.x) < fabsf(backward.y)) {
        zpos_flag_ |= PLANE_XZ;
      }
      else {
        zpos_flag_ |= PLANE_YZ;
      }
      zneg_flag_ = zpos_flag_;

      /* Perspective: If camera is below floor plane, we switch clipping.
       * Orthographic: If eye vector is looking up, we switch clipping. */
      if ((view.is_persp() && (position.z > 0.0f)) || (!view.is_persp() && (backward.z < 0.0f))) {
        zpos_flag_ |= CLIP_ZPOS;
        zneg_flag_ |= CLIP_ZNEG;
      }
      else {
        zpos_flag_ |= CLIP_ZNEG;
        zneg_flag_ |= CLIP_ZPOS;
      }

      zplane_axes_.x = float((zpos_flag_ & (PLANE_XZ | PLANE_XY)) != 0);
      zplane_axes_.y = float((zpos_flag_ & (PLANE_YZ | PLANE_XY)) != 0);
      zplane_axes_.z = float((zpos_flag_ & (PLANE_YZ | PLANE_XZ)) != 0);
    }

    data_.size = float4(v3d_clip_end_);
    if (!view.is_persp()) {
      data_.size /= min_ff(fabsf(view.winmat()[0][0]), fabsf(view.winmat()[1][1]));
    }

    data_.distance = v3d_clip_end_ / 2.0f;

    if (is_xr_) {
      /* The calculations for the grid parameters assume that the view matrix has no scale
       * component, which may not be correct if the user is "shrunk" or "enlarged" by zooming in or
       * out. Therefore, we need to compensate the values here. */
      /* Assumption is uniform scaling (all column vectors are of same length). */
      float viewinvscale = len_v3(view.viewinv()[0]);
      data_.distance *= viewinvscale;
    }
  }
};

}  // namespace blender::draw::overlay
