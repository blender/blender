/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_paint.hh"

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_view3d.hh"
#include "UI_view2d.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Draw the 2D/3D cursor.
 * Controlled by (Overlay > 3D Cursor)
 */
class Cursor : Overlay {
 private:
  PassSimple ps_ = {"Cursor"};

  bool enabled_ = false;

 public:
  Cursor() {}

  void begin_sync(Resources &res, const State &state) final
  {
    if (state.is_space_v3d()) {
      enabled_ = is_cursor_visible_3d(state);
    }
    else {
      enabled_ = is_cursor_visible_2d(state);
    }

    if (!enabled_) {
      return;
    }

    /* 2D coordinate of the cursor in screen space pixel. */
    int2 pixel_coord;

    float3x3 rotation = float3x3::identity();

    /* TODO(fclem): This is against design. Sync shouldn't depend on view. */
    if (state.is_space_v3d()) {
      const View3DCursor *cursor = &state.scene->cursor;
      rotation = float3x3(float4x4(state.rv3d->viewmat).view<3, 3>()) *
                 math::from_rotation<float3x3>(cursor->rotation());

      eV3DProjStatus status = ED_view3d_project_int_global(
          state.region, cursor->location, pixel_coord, V3D_PROJ_TEST_CLIP_NEAR);
      if (status != V3D_PROJ_RET_OK) {
        /* Clipped. */
        enabled_ = false;
        return;
      }
    }
    else {
      const SpaceImage *sima = (SpaceImage *)state.space_data;
      UI_view2d_view_to_region(
          &state.region->v2d, sima->cursor[0], sima->cursor[1], &pixel_coord[0], &pixel_coord[1]);
    }

    float4x4 cursor_mat = math::from_scale<float4x4>(float2(U.widget_unit));
    cursor_mat.location()[0] = pixel_coord[0] + 0.5f;
    cursor_mat.location()[1] = pixel_coord[1] + 0.5f;

    /* Copy of wmOrtho2_region_pixelspace but without GPU_matrix_ortho_set. */
    const float ofs = -0.01f;
    float4x4 proj_mat = math::projection::orthographic(
        ofs, state.region->winx + ofs, ofs, state.region->winy + ofs, -100.0f, 100.0f);

    float4x4 mvp = proj_mat * cursor_mat;

    PassSimple &pass = ps_;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
    pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_POLYLINE_FLAT_COLOR));
    pass.push_constant("viewportSize", float2(state.region->winx, state.region->winy));
    pass.push_constant("lineWidth", U.pixelsize);
    pass.push_constant("lineSmooth", true);
    /* WORKAROUND: This is normally set by the GPUBatch or IMM API but we don't use them here.
     * So make sure it is set otherwise it can be in undefined state (see #136911). */
    pass.push_constant("gpu_attr_0_fetch_int", false);
    pass.push_constant("gpu_attr_1_fetch_unorm8", false);
    pass.push_constant("gpu_attr_0_len", 3);
    pass.push_constant("gpu_attr_1_len", 3);
    /* See `polyline_draw_workaround`. */
    int3 vert_stride_count_line = {2, 9999 /* Doesn't matter. */, 0};
    int3 vert_stride_count_circle = {1, 9999 /* Doesn't matter. */, 0};

    if (state.is_space_v3d()) {
      const View3DCursor *cursor = &state.scene->cursor;
      const float scale = ED_view3d_pixel_size_no_ui_scale(state.rv3d, cursor->location);
      /* Only draw the axes lines in 3D with the correct perspective. */
      float4x4 cursor_mat = math::from_loc_rot_scale<float4x4>(
          cursor->location, cursor->rotation(), float3(scale * U.widget_unit));

      float4x4 mvp_lines = float4x4(state.rv3d->winmat) * float4x4(state.rv3d->viewmat) *
                           cursor_mat;

      pass.push_constant("ModelViewProjectionMatrix", mvp);
      pass.push_constant("gpu_vert_stride_count_offset", vert_stride_count_circle);
      pass.draw_expand(res.shapes.cursor_circle.get(), GPU_PRIM_TRIS, 2, 1);
      pass.push_constant("ModelViewProjectionMatrix", mvp_lines);
      pass.push_constant("gpu_vert_stride_count_offset", vert_stride_count_line);
      pass.draw_expand(res.shapes.cursor_lines.get(), GPU_PRIM_TRIS, 2, 1);
    }
    else {
      pass.push_constant("ModelViewProjectionMatrix", mvp);
      pass.push_constant("gpu_vert_stride_count_offset", vert_stride_count_circle);
      pass.draw_expand(res.shapes.cursor_circle.get(), GPU_PRIM_TRIS, 2, 1);
      pass.push_constant("gpu_vert_stride_count_offset", vert_stride_count_line);
      pass.draw_expand(res.shapes.cursor_lines.get(), GPU_PRIM_TRIS, 2, 1);
    }
  }

  void draw_output(Framebuffer &framebuffer, Manager &manager, View & /*view*/) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_);
  }

 private:
  bool is_cursor_visible_3d(const State &state)
  {
    if (G.moving & G_TRANSFORM_CURSOR) {
      return true;
    }

    const View3D *v3d = state.v3d;
    if ((v3d->flag2 & V3D_HIDE_OVERLAYS) || (v3d->overlay.flag & V3D_OVERLAY_HIDE_CURSOR)) {
      return false;
    }

    /* don't draw cursor in paint modes, but with a few exceptions */
    if ((state.object_mode & (OB_MODE_ALL_PAINT | OB_MODE_SCULPT_CURVES)) != 0) {
      /* exception: object is in weight paint and has deforming armature in pose mode */
      if (state.object_mode & OB_MODE_WEIGHT_PAINT) {
        if (BKE_object_pose_armature_get(const_cast<Object *>(state.object_active)) != nullptr) {
          return true;
        }
      }
      /* exception: object in texture paint mode, clone brush, use_clone_layer disabled */
      else if (state.object_mode & OB_MODE_TEXTURE_PAINT) {
        const Paint *paint = BKE_paint_get_active(const_cast<Scene *>(state.scene),
                                                  const_cast<ViewLayer *>(state.view_layer));
        const Brush *brush = (paint) ? BKE_paint_brush_for_read(paint) : nullptr;

        if (brush && brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) {
          if ((state.scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE) == 0) {
            return true;
          }
        }
      }

      /* no exception met? then don't draw cursor! */
      return false;
    }

    if (state.object_mode & OB_MODE_WEIGHT_GREASE_PENCIL) {
      /* grease pencil hide always in some modes */
      return false;
    }

    return true;
  }

  static bool is_cursor_visible_2d(const State &state)
  {
    SpaceInfo *space_data = (SpaceInfo *)state.space_data;
    if (space_data == nullptr) {
      return false;
    }
    if (space_data->spacetype != SPACE_IMAGE) {
      return false;
    }
    SpaceImage *sima = (SpaceImage *)space_data;
    switch (sima->mode) {
      case SI_MODE_VIEW:
        return false;
        break;
      case SI_MODE_PAINT:
        return false;
        break;
      case SI_MODE_MASK:
        break;
      case SI_MODE_UV:
        break;
    }
    return (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) != 0;
  }
};

}  // namespace blender::draw::overlay
