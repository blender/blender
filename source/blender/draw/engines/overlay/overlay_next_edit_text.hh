/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_vfont.hh"
#include "BLI_math_matrix.hh"

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

class EditText : Overlay {

 private:
  PassSimple ps_ = {"Selection&Cursor"};

  View view_edit_text = {"view_edit_text"};
  float view_dist = 0.0f;

  StorageVectorBuffer<ObjectMatrices> text_selection_buf;
  StorageVectorBuffer<ObjectMatrices> text_cursor_buf;
  LinePrimitiveBuf box_line_buf_;

 public:
  EditText(SelectionType selection_type) : box_line_buf_(selection_type, "box_line_buf_") {}

  void begin_sync(Resources & /*res*/, const State &state) final
  {
    enabled_ = state.is_space_v3d();
    text_selection_buf.clear();
    text_cursor_buf.clear();
    box_line_buf_.clear();
  }

  void edit_object_sync(Manager & /*manager*/,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State & /*state*/) final
  {
    if (!enabled_) {
      return;
    }

    const Curve &cu = *static_cast<Curve *>(ob_ref.object->data);
    add_select(cu, ob_ref.object->object_to_world());
    add_cursor(cu, ob_ref.object->object_to_world());
    add_boxes(res, cu, ob_ref.object->object_to_world());
  }

  void end_sync(Resources &res, const ShapeCache &shapes, const State &state) final
  {
    ps_.init();
    res.select_bind(ps_);
    {
      DRWState default_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
      float4 color;

      /* Selection Boxes. */
      {
        auto &sub = ps_.sub("text_selection");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders.uniform_color_batch.get());
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_SELECTION, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);

        auto &buf = text_selection_buf;
        buf.push_update();
        sub.bind_ssbo("matrix_buf", &buf);
        sub.draw(shapes.quad_solid.get(), buf.size());
      }

      /* Highlight text within selection boxes. */
      {
        auto &sub = ps_.sub("highlight_text_selection");
        sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                          DRW_STATE_DEPTH_GREATER_EQUAL,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders.uniform_color_batch.get());
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_HIGHLIGHT, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);

        auto &buf = text_selection_buf;
        buf.push_update();
        sub.bind_ssbo("matrix_buf", &buf);
        sub.draw(shapes.quad_solid.get(), buf.size());
      }

      /* Cursor (text caret). */
      {
        auto &sub = ps_.sub("text_cursor");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders.uniform_color_batch.get());
        sub.state_set(default_state, state.clipping_plane_count);
        UI_GetThemeColor4fv(TH_WIDGET_TEXT_CURSOR, color);
        srgb_to_linearrgb_v4(color, color);
        sub.push_constant("ucolor", color);

        auto &buf = text_cursor_buf;
        buf.push_update();
        sub.bind_ssbo("matrix_buf", &buf);
        sub.draw(shapes.quad_solid.get(), buf.size());
      }

      /* Text boxes. */
      {
        auto &sub_pass = ps_.sub("text_boxes");
        sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                               DRW_STATE_DEPTH_LESS_EQUAL,
                           state.clipping_plane_count);
        sub_pass.shader_set(res.shaders.extra_wire.get());
        sub_pass.bind_ubo("globalsBlock", &res.globals_buf);
        box_line_buf_.end_sync(sub_pass);
      }
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }
    view_edit_text.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, -5.0f));

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

  void add_select(const Curve &cu, const float4x4 &ob_to_world)
  {
    EditFont *ef = cu.editfont;
    float4x4 final_mat;
    float4x2 box;

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
      /* NOTE: v2_quad_corners_to_mat4 don't need the 3rd corner. */
      if (sb->rot == 0.0f) {
        box[0] = float2(sb->x, sb->y);
        box[1] = float2(sb->x + selboxw, sb->y);
        box[3] = float2(sb->x, sb->y + sb->h);
      }
      else {
        float2x2 mat = math::from_rotation<float2x2>(sb->rot);
        box[0] = float2(sb->x, sb->y);
        box[1] = mat[0] * selboxw + float2(&sb->x);
        box[3] = mat[1] * sb->h + float2(&sb->x);
      }
      v2_quad_corners_to_mat4(box, final_mat);
      final_mat = ob_to_world * final_mat;
      ObjectMatrices obj_mat;
      obj_mat.sync(final_mat);
      text_selection_buf.append(obj_mat);
    }
  }

  void add_cursor(const Curve &cu, const float4x4 &ob_to_world)
  {
    EditFont *edit_font = cu.editfont;
    float4x2 cursor = float4x2(&edit_font->textcurs[0][0]);
    float4x4 mat;

    v2_quad_corners_to_mat4(cursor, mat);
    ObjectMatrices ob_mat;
    ob_mat.sync(ob_to_world * mat);
    text_cursor_buf.append(ob_mat);
  }

  void add_boxes(const Resources &res, const Curve &cu, const float4x4 &ob_to_world)
  {
    for (const int i : IndexRange(cu.totbox)) {
      const TextBox &tb = cu.tb[i];
      const bool is_active = (i == (cu.actbox - 1));
      const float4 &color = is_active ? res.theme_settings.color_active :
                                        res.theme_settings.color_wire;

      if ((tb.w != 0.0f) || (tb.h != 0.0f)) {
        const float3 top_left = float3(cu.xof + tb.x, cu.yof + tb.y + cu.fsize_realtime, 0.001);
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
