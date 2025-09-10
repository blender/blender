/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_math_color.h"
#include "BLI_math_matrix.hh"

#include "BKE_vfont.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Text objects related overlays.
 * Currently only display cursor and selection of text edit mode.
 */
class Text : Overlay {

 private:
  PassSimple ps_ = {"TextEdit"};
  PassSimple::Sub *selection_ps_ = nullptr;
  PassSimple::Sub *selection_highlight_ps_ = nullptr;
  PassSimple::Sub *cursor_ps_ = nullptr;

  View view_edit_text = {"view_edit_text"};

  LinePrimitiveBuf box_line_buf_;

  /** A solid quad. */
  gpu::Batch *quad = nullptr;
  /** A wire quad. */
  gpu::Batch *quad_wire = nullptr;

 public:
  Text(SelectionType selection_type) : box_line_buf_(selection_type, "box_line_buf_") {}

  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();
    box_line_buf_.clear();

    if (!enabled_) {
      return;
    }

    quad = res.shapes.quad_solid.get();
    quad_wire = res.shapes.quad_wire.get();

    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(ps_);
    {
      DRWState default_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
      float4 color;

      /* Selection Boxes. */
      {
        auto &sub = ps_.sub("text_selection");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->uniform_color.get());
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_SELECTION, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);
        selection_ps_ = &sub;
      }

      /* Highlight text within selection boxes. */
      {
        auto &sub = ps_.sub("highlight_text_selection");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                          DRW_STATE_DEPTH_GREATER_EQUAL,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders->uniform_color.get());
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_HIGHLIGHT, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);
        selection_highlight_ps_ = &sub;
      }

      /* Cursor (text caret). */
      {
        auto &sub = ps_.sub("text_cursor");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->uniform_color.get());
        sub.state_set(default_state, state.clipping_plane_count);
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_CURSOR, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);
        cursor_ps_ = &sub;
      }
    }
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State & /*state*/) final
  {
    if (!enabled_) {
      return;
    }

    const Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob_ref.object);
    add_select(manager, cu, ob_ref.object->object_to_world());
    add_cursor(manager, cu, ob_ref.object->object_to_world());
    add_boxes(res, cu, ob_ref.object->object_to_world());
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!enabled_) {
      return;
    }

    /* Text boxes. */
    {
      auto &sub_pass = ps_.sub("text_boxes");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                             DRW_STATE_DEPTH_LESS_EQUAL,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_wire.get());
      box_line_buf_.end_sync(sub_pass);
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    View::OffsetData offset;
    view_edit_text.sync(view.viewmat(), offset.winmat_polygon_offset(view.winmat(), -5.0f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view_edit_text);
  }

 private:
  /* Use 2D quad corners to create a matrix that set
   * a [-1..1] quad at the right position. */
  static void v2_quad_corners_to_mat4(const float4x2 &corners, float4x4 &r_mat)
  {
    const float2 &origin = corners[0];
    const float2 half_size_x = (float2(corners[1]) - float2(corners[0])) * 0.5f;
    const float2 half_size_y = (float2(corners[3]) - float2(corners[0])) * 0.5f;

    r_mat = float4x4(float4(half_size_x, 0.0f, 0.0f),
                     float4(half_size_y, 0.0f, 0.0f),
                     float4(0.0f, 0.0f, 1.0f, 0.0f),
                     float4(origin + half_size_x + half_size_y, 0.0f, 1.0f));
  }

  void add_select(Manager &manager, const Curve &cu, const float4x4 &ob_to_world)
  {
    EditFont *ef = cu.editfont;
    for (const int i : IndexRange(ef->selboxes_len)) {
      EditFontSelBox *sb = &ef->selboxes[i];

      float selboxw;
      if (i + 1 != ef->selboxes_len) {
        if (ef->selboxes[i + 1].y == sb->y) {
          selboxw = ef->selboxes[i + 1].x - sb->x;
        }
        else {
          selboxw = sb->w;
        }
      }
      else {
        selboxw = sb->w;
      }
      float4x2 box;
      /* NOTE: v2_quad_corners_to_mat4 don't need the 3rd corner. */
      if (sb->rotate == 0.0f) {
        box[0] = float2(sb->x, sb->y);
        box[1] = float2(sb->x + selboxw, sb->y);
        box[3] = float2(sb->x, sb->y + sb->h);
      }
      else {
        float2x2 mat = math::from_rotation<float2x2>(sb->rotate);
        box[0] = float2(sb->x, sb->y);
        box[1] = mat[0] * selboxw + float2(&sb->x);
        box[3] = mat[1] * sb->h + float2(&sb->x);
      }
      float4x4 mat;
      v2_quad_corners_to_mat4(box, mat);
      ResourceHandleRange res_handle = manager.resource_handle(ob_to_world * mat);
      selection_ps_->draw(quad, res_handle);
      selection_highlight_ps_->draw(quad, res_handle);
    }
  }

  void add_cursor(Manager &manager, const Curve &cu, const float4x4 &ob_to_world)
  {
    EditFont *edit_font = cu.editfont;
    float4x2 cursor = float4x2(&edit_font->textcurs[0][0]);
    float4x4 mat;
    v2_quad_corners_to_mat4(cursor, mat);
    ResourceHandleRange res_handle = manager.resource_handle(ob_to_world * mat);

    cursor_ps_->draw(quad, res_handle);

    /* Draw both wire and solid so the cursor is always at least with width of a line,
     * otherwise it may become invisible, see: #137940. */
    cursor_ps_->draw(quad_wire, res_handle);
  }

  void add_boxes(const Resources &res, const Curve &cu, const float4x4 &ob_to_world)
  {
    const EditFont *edit_font = cu.editfont;
    for (const int i : IndexRange(cu.totbox)) {
      const TextBox &tb = cu.tb[i];
      const bool is_active = (i == (cu.actbox - 1));
      const float4 &color = is_active ? res.theme.colors.active_object : res.theme.colors.wire;

      if ((tb.w != 0.0f) || (tb.h != 0.0f)) {
        const float3 top_left = float3(
            cu.xof + tb.x, cu.yof + tb.y + edit_font->font_size_eval, 0.001);
        const float3 w = float3(tb.w, 0.0f, 0.0f);
        const float3 h = float3(0.0f, tb.h, 0.0f);
        float4x3 vecs = float4x3(top_left, top_left + w, top_left + w - h, top_left - h);

        for (const int j : IndexRange(4)) {
          vecs[j] = math::transform_point(ob_to_world, vecs[j]);
        }
        for (const int j : IndexRange(4)) {
          box_line_buf_.append(vecs[j], vecs[(j + 1) % 4], color);
        }
      }
    }
  }
};
}  // namespace blender::draw::overlay
