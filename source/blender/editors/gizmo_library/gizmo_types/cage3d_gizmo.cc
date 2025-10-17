/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Cage Gizmo
 *
 * 3D Gizmo
 *
 * \brief Cuboid gizmo acting as a 'cage' around its content.
 * Interacting scales or translates the gizmo.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "BKE_context.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_select.hh"
#include "GPU_state.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

/* own includes */
#include "../gizmo_library_intern.hh"

#define GIZMO_MARGIN_OFFSET_SCALE 1.5f

static void gizmo_calc_matrix_final_no_offset(const wmGizmo *gz,
                                              float orig_matrix_final_no_offset[4][4],
                                              bool use_space)
{
  float mat_identity[4][4];
  WM_GizmoMatrixParams params = {nullptr};
  unit_m4(mat_identity);
  if (use_space == false) {
    params.matrix_basis = mat_identity;
  }
  params.matrix_offset = mat_identity;
  WM_gizmo_calc_matrix_final_params(gz, &params, orig_matrix_final_no_offset);
}

static void gizmo_calc_rect_view_scale(const wmGizmo *gz, const float dims[3], float scale[3])
{
  UNUSED_VARS(dims);

  /* Unlike cage2d, no need to correct for aspect. */
  float matrix_final_no_offset[4][4];

  float x_axis[3], y_axis[3], z_axis[3];
  gizmo_calc_matrix_final_no_offset(gz, matrix_final_no_offset, false);
  mul_v3_mat3_m4v3(x_axis, matrix_final_no_offset, gz->matrix_offset[0]);
  mul_v3_mat3_m4v3(y_axis, matrix_final_no_offset, gz->matrix_offset[1]);
  mul_v3_mat3_m4v3(z_axis, matrix_final_no_offset, gz->matrix_offset[2]);

  scale[0] = 1.0f / len_v3(x_axis);
  scale[1] = 1.0f / len_v3(y_axis);
  scale[2] = 1.0f / len_v3(z_axis);
}

static void gizmo_calc_rect_view_margin(const wmGizmo *gz, const float dims[3], float margin[3])
{
  const float handle_size = 9.0f;
  /* XXX, the scale isn't taking offset into account, we need to calculate scale per handle! */
  // handle_size *= gz->scale_final;

  float scale_xyz[3];
  gizmo_calc_rect_view_scale(gz, dims, scale_xyz);
  margin[0] = (handle_size * scale_xyz[0]);
  margin[1] = (handle_size * scale_xyz[1]);
  margin[2] = (handle_size * scale_xyz[2]);
}

/* -------------------------------------------------------------------- */

static void gizmo_rect_pivot_from_scale_part(int part,
                                             float r_pt[3],
                                             bool r_constrain_axis[3],
                                             bool has_translation)
{
  if (part >= ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
      part <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
  {
    int index = (part - ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
    int range[3];
    range[2] = index % 3;
    index = index / 3;
    range[1] = index % 3;
    index = index / 3;
    range[0] = index % 3;

    const float sign[3] = {0.5f, 0.0f, -0.5f};
    for (int i = 0; i < 3; i++) {
      r_pt[i] = has_translation ? sign[range[i]] : 0.0f;
      r_constrain_axis[i] = (range[i] == 1);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Box Draw Style
 *
 * Useful for 3D views, see: #ED_GIZMO_CAGE3D_STYLE_BOX
 * \{ */

static void cage3d_draw_box_corners(const float r[3],
                                    const float margin[3],
                                    const float color[3],
                                    const float line_width)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  UNUSED_VARS(margin);

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
  immUniformColor3fv(color);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);
  immUniform1f("lineWidth", line_width * U.pixelsize);

  imm_draw_cube_wire_3d(pos, blender::float3(0.0f), r);

  immUnbindProgram();
}

static void cage3d_draw_box_interaction(const RegionView3D *rv3d,
                                        const float matrix_final[4][4],
                                        const float color[4],
                                        const int highlighted,
                                        const float size[3],
                                        const float margin[3])
{
  if (highlighted >= ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
      highlighted <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
  {
    int index = (highlighted - ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
    int range[3];
    range[2] = index % 3;
    index = index / 3;
    range[1] = index % 3;
    index = index / 3;
    range[0] = index % 3;

    const float sign[3] = {-1.0f, 0.0f, 1.0f};
    float co[3];

    for (int i = 0; i < 3; i++) {
      co[i] = size[i] * sign[range[i]];
    }
    const float rad[3] = {margin[0] / 3, margin[1] / 3, margin[2] / 3};
    float co_test[3];
    mul_v3_m4v3(co_test, matrix_final, co);
    float rad_scale[3];
    mul_v3_v3fl(rad_scale, rad, ED_view3d_pixel_size(rv3d, co_test));

    {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor3fv(color);
      imm_draw_cube_fill_3d(pos, co, rad_scale);
      immUnbindProgram();
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Draw Style
 *
 * Useful for 2D views, see: #ED_GIZMO_CAGE3D_STYLE_CIRCLE
 * \{ */

static void imm_draw_point_aspect_3d(uint pos, const float co[3], const float rad[3], bool solid)
{
  if (solid) {
    imm_draw_cube_fill_3d(pos, co, rad);
  }
  else {
    imm_draw_cube_wire_3d(pos, co, rad);
  }
}

static void cage3d_draw_circle_wire(const float r[3],
                                    const float margin[3],
                                    const float color[3],
                                    const int transform_flag,
                                    const int draw_options,
                                    const float line_width)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
  immUniformColor3fv(color);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);
  immUniform1f("lineWidth", line_width * U.pixelsize);

  imm_draw_cube_wire_3d(pos, blender::float3(0.0f), r);

#if 0
  if (transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE) {
    if (draw_options & ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE) {
      const float rad[2] = {margin[0] / 2, margin[1] / 2};
      const float center[2] = {0.0f, 0.0f};

      immBegin(GPU_PRIM_LINES, 4);
      immVertex2f(pos, center[0] - rad[0], center[1] - rad[1]);
      immVertex2f(pos, center[0] + rad[0], center[1] + rad[1]);
      immVertex2f(pos, center[0] + rad[0], center[1] - rad[1]);
      immVertex2f(pos, center[0] - rad[0], center[1] + rad[1]);
      immEnd();
    }
  }
#else
  UNUSED_VARS(margin, transform_flag, draw_options);
#endif

  immUnbindProgram();
}

static void cage3d_draw_circle_handles(const RegionView3D *rv3d,
                                       const float matrix_final[4][4],
                                       const float r[3],
                                       const float margin[3],
                                       const float color[3],
                                       bool solid,
                                       const float handle_scale)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  const float rad[3] = {margin[0] / 3, margin[1] / 3, margin[2] / 3};

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3fv(color);

  const float sign[3] = {-1.0f, 0.0f, 1.0f};
  for (int x = 0; x < 3; x++) {
    for (int y = 0; y < 3; y++) {
      for (int z = 0; z < 3; z++) {
        if (x == 1 && y == 1 && z == 1) {
          continue;
        }
        const float co[3] = {r[0] * sign[x], r[1] * sign[y], r[2] * sign[z]};
        float co_test[3];
        mul_v3_m4v3(co_test, matrix_final, co);
        float rad_scale[3];
        mul_v3_v3fl(rad_scale, rad, ED_view3d_pixel_size(rv3d, co_test) * handle_scale);
        imm_draw_point_aspect_3d(pos, co, rad_scale, solid);
      }
    }
  }

  immUnbindProgram();
}

/** \} */

static void gizmo_cage3d_draw_intern(
    RegionView3D *rv3d, wmGizmo *gz, const bool select, const bool highlight, const int select_id)
{
  // const bool use_clamp = (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0;
  float dims[3];
  RNA_float_get_array(gz->ptr, "dimensions", dims);
  float matrix_final[4][4];

  const int transform_flag = RNA_enum_get(gz->ptr, "transform");
  const int draw_style = RNA_enum_get(gz->ptr, "draw_style");
  const int draw_options = RNA_enum_get(gz->ptr, "draw_options");

  const float size_real[3] = {dims[0] / 2.0f, dims[1] / 2.0f, dims[2] / 2.0f};

  WM_gizmo_calc_matrix_final(gz, matrix_final);

  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);

  float margin[3];
  gizmo_calc_rect_view_margin(gz, dims, margin);

  /* Handy for quick testing draw (if it's outside bounds). */
  if (false) {
    GPU_blend(GPU_BLEND_ALPHA);
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4f(1, 1, 1, 0.5f);
    float s = 0.5f;
    immRectf(pos, -s, -s, s, s);
    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }

  if (select) {
/* Expand for hot-spot. */
#if 0
    const float size[3] = {
        size_real[0] + margin[0] / 2,
        size_real[1] + margin[1] / 2,
        size_real[2] + margin[2] / 2,
    };
#else
    /* just use same value for now. */
    const float size[3] = {UNPACK3(size_real)};
#endif

    if (transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_SCALE) {
      for (int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
           i <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z;
           i++)
      {
        if (i == ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MID_Y_MID_Z) {
          continue;
        }
        GPU_select_load_id(select_id | i);
        cage3d_draw_box_interaction(rv3d, matrix_final, gz->color, i, size, margin);
      }
    }
    if (transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE) {
      const int transform_part = ED_GIZMO_CAGE3D_PART_TRANSLATE;
      GPU_select_load_id(select_id | transform_part);
      cage3d_draw_box_interaction(rv3d, matrix_final, gz->color, transform_part, size, margin);
    }
  }
  else {
#if 0
    rctf _r {}
    _r.xmin = -size_real[0];
    _r.ymin = -size_real[1];
    _r.xmax = size_real[0];
    _r.ymax = size_real[1];
#endif
    if (draw_style == ED_GIZMO_CAGE3D_STYLE_BOX) {
      float color[4], black[3] = {0, 0, 0};
      gizmo_color_get(gz, highlight, color);

      /* corner gizmos */
      cage3d_draw_box_corners(size_real, margin, black, gz->line_width + 3.0f);

      /* corner gizmos */
      cage3d_draw_box_corners(size_real, margin, color, gz->line_width);

      bool show = false;
      if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_TRANSLATE) {
        /* Only show if we're drawing the center handle
         * otherwise the entire rectangle is the hot-spot. */
        if (draw_options & ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE) {
          show = true;
        }
      }
      else {
        show = true;
      }

      if (show) {
        cage3d_draw_box_interaction(
            rv3d, matrix_final, gz->color, gz->highlight_part, size_real, margin);
      }
    }
    else if (draw_style == ED_GIZMO_CAGE3D_STYLE_CIRCLE) {
      float color[4], black[3] = {0, 0, 0};
      gizmo_color_get(gz, highlight, color);

      GPU_blend(GPU_BLEND_ALPHA);

      cage3d_draw_circle_wire(
          size_real, margin, black, transform_flag, draw_options, gz->line_width + 3.0f);
      cage3d_draw_circle_wire(
          size_real, margin, color, transform_flag, draw_options, gz->line_width);

      /* Corner gizmos (draw the outer & inner so there is a visible outline). */
      GPU_polygon_smooth(true);
      cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, black, true, 1.0f);
      cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, color, true, 1.0f / 1.5f);
      GPU_polygon_smooth(false);

      GPU_blend(GPU_BLEND_NONE);
    }
    else {
      BLI_assert(0);
    }
  }

  GPU_matrix_pop();
}

/**
 * For when we want to draw 3d cage in 3d views.
 */
static void gizmo_cage3d_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  gizmo_cage3d_draw_intern(rv3d, gz, true, false, select_id);
}

static void gizmo_cage3d_draw(const bContext *C, wmGizmo *gz)
{
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;
  gizmo_cage3d_draw_intern(rv3d, gz, false, is_highlight, -1);
}

static int gizmo_cage3d_get_cursor(wmGizmo *gz)
{
  if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
    return WM_CURSOR_NSEW_SCROLL;
  }

  return WM_CURSOR_DEFAULT;
}

namespace {

struct RectTransformInteraction {
  float orig_mouse[3];
  float orig_matrix_offset[4][4];
  float orig_matrix_final_no_offset[4][4];
};

}  // namespace

static void gizmo_cage3d_setup(wmGizmo *gz)
{
  gz->flag |= /* WM_GIZMO_DRAW_MODAL | */ /* TODO */
      WM_GIZMO_DRAW_NO_SCALE;
}

static wmOperatorStatus gizmo_cage3d_invoke(bContext *C, wmGizmo *gz, const wmEvent *event)
{
  RectTransformInteraction *data = MEM_callocN<RectTransformInteraction>("cage_interaction");

  copy_m4_m4(data->orig_matrix_offset, gz->matrix_offset);
  gizmo_calc_matrix_final_no_offset(gz, data->orig_matrix_final_no_offset, true);

  if (gizmo_window_project_3d(
          C, gz, blender::float2(blender::int2(event->mval)), false, data->orig_mouse) == 0)
  {
    zero_v3(data->orig_mouse);
  }

  gz->interaction_data = data;

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus gizmo_cage3d_modal(bContext *C,
                                           wmGizmo *gz,
                                           const wmEvent *event,
                                           eWM_GizmoFlagTweak /*tweak_flag*/)
{
  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }
  /* For transform logic to be manageable we operate in -0.5..0.5 2D space,
   * no matter the size of the rectangle, mouse coords are scaled to unit space.
   * The mouse coords have been projected into the matrix
   * so we don't need to worry about axis alignment.
   *
   * - The cursor offset are multiplied by 'dims'.
   * - Matrix translation is also multiplied by 'dims'.
   */
  RectTransformInteraction *data = static_cast<RectTransformInteraction *>(gz->interaction_data);
  float point_local[3];

  float dims[3];
  RNA_float_get_array(gz->ptr, "dimensions", dims);

  {
    float matrix_back[4][4];
    copy_m4_m4(matrix_back, gz->matrix_offset);
    copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);

    bool ok = gizmo_window_project_3d(
        C, gz, blender::float2(blender::int2(event->mval)), false, point_local);
    copy_m4_m4(gz->matrix_offset, matrix_back);
    if (!ok) {
      return OPERATOR_RUNNING_MODAL;
    }
  }

  const int transform_flag = RNA_enum_get(gz->ptr, "transform");
  wmGizmoProperty *gz_prop;

  gz_prop = WM_gizmo_target_property_find(gz, "matrix");
  if (gz_prop->type != nullptr) {
    WM_gizmo_target_property_float_get_array(gz, gz_prop, &gz->matrix_offset[0][0]);
  }

  if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_TRANSLATE) {
    /* do this to prevent clamping from changing size */
    copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);
    gz->matrix_offset[3][0] = data->orig_matrix_offset[3][0] +
                              (point_local[0] - data->orig_mouse[0]);
    gz->matrix_offset[3][1] = data->orig_matrix_offset[3][1] +
                              (point_local[1] - data->orig_mouse[1]);
    gz->matrix_offset[3][2] = data->orig_matrix_offset[3][2] +
                              (point_local[2] - data->orig_mouse[2]);
  }
  else if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_ROTATE) {
    /* Add this (if we need it). */
  }
  else {
    /* scale */
    copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);

    float pivot[3];
    bool constrain_axis[3] = {false};
    bool has_translation = transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE;
    gizmo_rect_pivot_from_scale_part(gz->highlight_part, pivot, constrain_axis, has_translation);

    float scale[3] = {1.0f, 1.0f, 1.0f};
    for (int i = 0; i < 3; i++) {
      if (constrain_axis[i] == false) {
        /* Original cursor position relative to pivot, remapped to [-1, 1] */
        const float delta_orig = (data->orig_mouse[i] - data->orig_matrix_offset[3][i]) /
                                     (dims[i] * len_v3(data->orig_matrix_offset[i])) -
                                 pivot[i];
        const float delta_curr = (point_local[i] - data->orig_matrix_offset[3][i]) /
                                     (dims[i] * len_v3(data->orig_matrix_offset[i])) -
                                 pivot[i];

        if ((transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_SCALE_SIGNED) == 0) {
          if (signum_i(delta_orig) != signum_i(delta_curr)) {
            scale[i] = 0.0f;
            continue;
          }
        }

        /* Original cursor position does not exactly lie on the cage boundary due to margin. */
        const float delta_boundary = signf(delta_orig) * 0.5f - pivot[i];
        scale[i] = delta_curr / delta_boundary;
      }
    }

    if (transform_flag & ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM) {
      if (constrain_axis[0] == false && constrain_axis[1] == false) {
        scale[1] = scale[0] = (scale[1] + scale[0]) / 2.0f;
      }
      else if (constrain_axis[0] == false) {
        scale[1] = scale[0];
      }
      else if (constrain_axis[1] == false) {
        scale[0] = scale[1];
      }
      else {
        BLI_assert(0);
      }
    }

    /* scale around pivot */
    float matrix_scale[4][4];
    unit_m4(matrix_scale);

    mul_v3_fl(matrix_scale[0], scale[0]);
    mul_v3_fl(matrix_scale[1], scale[1]);
    mul_v3_fl(matrix_scale[2], scale[2]);

    transform_pivot_set_m4(
        matrix_scale, blender::float3(pivot[0] * dims[0], pivot[1] * dims[1], pivot[2] * dims[2]));
    mul_m4_m4m4(gz->matrix_offset, data->orig_matrix_offset, matrix_scale);
  }

  if (gz_prop->type != nullptr) {
    WM_gizmo_target_property_float_set_array(C, gz, gz_prop, &gz->matrix_offset[0][0]);
  }

  /* tag the region for redraw */
  ED_region_tag_redraw_editor_overlays(CTX_wm_region(C));

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_cage3d_property_update(wmGizmo *gz, wmGizmoProperty *gz_prop)
{
  if (STREQ(gz_prop->type->idname, "matrix")) {
    if (WM_gizmo_target_property_array_length(gz, gz_prop) == 16) {
      WM_gizmo_target_property_float_get_array(gz, gz_prop, &gz->matrix_offset[0][0]);
    }
    else {
      BLI_assert(0);
    }
  }
  else {
    BLI_assert(0);
  }
}

static void gizmo_cage3d_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
  RectTransformInteraction *data = static_cast<RectTransformInteraction *>(gz->interaction_data);

  if (!cancel) {
    return;
  }

  wmGizmoProperty *gz_prop;

  /* reset properties */
  gz_prop = WM_gizmo_target_property_find(gz, "matrix");
  if (gz_prop->type != nullptr) {
    WM_gizmo_target_property_float_set_array(C, gz, gz_prop, &data->orig_matrix_offset[0][0]);
  }

  copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);
}

/* -------------------------------------------------------------------- */
/** \name Cage Gizmo API
 * \{ */

static void GIZMO_GT_cage_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_cage_3d";

  /* API callbacks. */
  gzt->draw = gizmo_cage3d_draw;
  gzt->draw_select = gizmo_cage3d_draw_select;
  gzt->setup = gizmo_cage3d_setup;
  gzt->invoke = gizmo_cage3d_invoke;
  gzt->property_update = gizmo_cage3d_property_update;
  gzt->modal = gizmo_cage3d_modal;
  gzt->exit = gizmo_cage3d_exit;
  gzt->cursor_get = gizmo_cage3d_get_cursor;

  gzt->struct_size = sizeof(wmGizmo);

  /* rna */
  static const EnumPropertyItem rna_enum_draw_style[] = {
      {ED_GIZMO_CAGE3D_STYLE_BOX, "BOX", 0, "Box", ""},
      {ED_GIZMO_CAGE3D_STYLE_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem rna_enum_transform[] = {
      {ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE, "TRANSLATE", 0, "Move", ""},
      {ED_GIZMO_CAGE_XFORM_FLAG_SCALE, "SCALE", 0, "Scale", ""},
      {ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM, "SCALE_UNIFORM", 0, "Scale Uniform", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem rna_enum_draw_options[] = {
      {ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE, "XFORM_CENTER_HANDLE", 0, "Center Handle", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const float unit_v3[3] = {1.0f, 1.0f, 1.0f};
  RNA_def_float_vector(
      gzt->srna, "dimensions", 3, unit_v3, 0, FLT_MAX, "Dimensions", "", 0.0f, FLT_MAX);
  RNA_def_enum_flag(gzt->srna, "transform", rna_enum_transform, 0, "Transform Options", "");
  RNA_def_enum(gzt->srna,
               "draw_style",
               rna_enum_draw_style,
               ED_GIZMO_CAGE3D_STYLE_CIRCLE,
               "Draw Style",
               "");
  RNA_def_enum_flag(gzt->srna,
                    "draw_options",
                    rna_enum_draw_options,
                    ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE,
                    "Draw Options",
                    "");

  WM_gizmotype_target_property_def(gzt, "matrix", PROP_FLOAT, 16);
}

void ED_gizmotypes_cage_3d()
{
  WM_gizmotype_append(GIZMO_GT_cage_3d);
}

/** \} */
