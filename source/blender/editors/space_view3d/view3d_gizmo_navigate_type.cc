/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Custom Orientation/Navigation Gizmo for the 3D View
 *
 * \brief Simple gizmo to axis and translate.
 *
 * - scale_basis: used for the size.
 * - matrix_basis: used for the location.
 * - matrix_offset: used to store the orientation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_sort_utils.h"

#include "BKE_context.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "view3d_intern.h"

/* Radius of the entire background. */
#define WIDGET_RADIUS ((U.gizmo_size_navigate_v3d / 2.0f) * UI_SCALE_FAC)

/* Sizes of axis spheres containing XYZ characters in relation to above. */
#define AXIS_HANDLE_SIZE 0.20f

#define AXIS_LINE_WIDTH ((U.gizmo_size_navigate_v3d / 40.0f) * U.pixelsize)
#define AXIS_RING_WIDTH ((U.gizmo_size_navigate_v3d / 60.0f) * U.pixelsize)
#define AXIS_TEXT_SIZE (WIDGET_RADIUS * AXIS_HANDLE_SIZE * 1.25f)

/* distance within this from center is considered positive. */
#define AXIS_DEPTH_BIAS 0.01f

static void gizmo_axis_draw(const bContext *C, wmGizmo *gz)
{
  struct {
    float depth;
    char index;
    char axis;
    char axis_opposite;
    bool is_pos;
  } axis_order[6] = {
      {-gz->matrix_offset[0][2], 0, 0, 1, false},
      {+gz->matrix_offset[0][2], 1, 0, 0, true},
      {-gz->matrix_offset[1][2], 2, 1, 3, false},
      {+gz->matrix_offset[1][2], 3, 1, 2, true},
      {-gz->matrix_offset[2][2], 4, 2, 5, false},
      {+gz->matrix_offset[2][2], 5, 2, 4, true},
  };

  int axis_align = -1;
  for (int axis = 0; axis < 3; axis++) {
    if (len_squared_v2(gz->matrix_offset[axis]) < 1e-6f) {
      axis_align = axis;
      break;
    }
  }

  qsort(&axis_order, ARRAY_SIZE(axis_order), sizeof(axis_order[0]), BLI_sortutil_cmp_float);

  /* When the cursor is over any of the gizmos (show circle backdrop). */
  const bool is_active = ((gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0);

  /* Background color of the View3D, used to mix colors. */
  float view_color[4];
  ED_view3d_background_color_get(CTX_data_scene(C), CTX_wm_view3d(C), view_color);
  view_color[3] = 1.0f;

  float matrix_screen[4][4];
  float matrix_unit[4][4];
  unit_m4(matrix_unit);

  WM_GizmoMatrixParams params{};
  params.matrix_offset = matrix_unit;
  WM_gizmo_calc_matrix_final_params(gz, &params, matrix_screen);
  GPU_matrix_push();
  GPU_matrix_mul(matrix_screen);

  GPUVertFormat *format = immVertexFormat();
  const uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const uint color_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);

  static float axis_color[3][4];

  struct {
    float matrix[4][4];
    float matrix_m3[3][3];
    float matrix_m3_invert[3][3];
    int id;
  } font;

  font.id = BLF_default();
  BLF_disable(font.id, BLF_ROTATION | BLF_SHADOW | BLF_MATRIX | BLF_ASPECT | BLF_WORD_WRAP);
  BLF_enable(font.id, BLF_BOLD);
  BLF_size(font.id, AXIS_TEXT_SIZE);
  BLF_position(font.id, 0, 0, 0);

  /* Calculate the inverse of the (matrix_final * matrix_offset).
   * This allows us to use the final location, while reversing the rotation so fonts
   * show without any rotation. */
  float m3[3][3];
  float m3_offset[3][3];
  copy_m3_m4(m3, matrix_screen);
  copy_m3_m4(m3_offset, gz->matrix_offset);
  mul_m3_m3m3(m3, m3, m3_offset);
  copy_m3_m3(font.matrix_m3_invert, m3);
  invert_m3(m3);
  copy_m3_m3(font.matrix_m3, m3);
  copy_m4_m3(font.matrix, m3);

  bool use_project_matrix = (gz->scale_final >= -GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT);
  if (use_project_matrix) {
    GPU_matrix_push_projection();
    GPU_matrix_ortho_set_z(-gz->scale_final, gz->scale_final);
  }

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  GPU_polygon_smooth(false);

  /* Circle defining active area. */
  if (is_active) {
    const float rad = WIDGET_RADIUS;
    GPU_matrix_push();
    GPU_matrix_scale_1f(1.0f / rad);

    rctf rect{};
    rect.xmin = -rad;
    rect.xmax = rad;
    rect.ymin = -rad;
    rect.ymax = rad;
    UI_draw_roundbox_4fv(&rect, true, rad, gz->color_hi);
    GPU_matrix_pop();
  }

  GPU_matrix_mul(gz->matrix_offset);

  for (int axis_index = 0; axis_index < ARRAY_SIZE(axis_order); axis_index++) {
    const int index = axis_order[axis_index].index;
    const int axis = axis_order[axis_index].axis;
    const bool is_pos = axis_order[axis_index].is_pos;
    const float depth = axis_order[axis_index].depth;
    const bool is_behind = (depth <= (AXIS_DEPTH_BIAS * (is_pos ? -1 : 1)));
    bool is_aligned_front = (axis_align != -1 && axis_align == axis && !is_behind);
    bool is_aligned_back = (axis_align != -1 && axis_align == axis && is_behind);

    const float v[3] = {0, 0, (1.0f - AXIS_HANDLE_SIZE) * (is_pos ? 1 : -1)};
    const float v_final[3] = {v[(axis + 2) % 3], v[(axis + 1) % 3], v[axis]};

    bool is_highlight = index + 1 == gz->highlight_part;
    /* Check if highlight part is the other side when axis aligned. */
    if (is_aligned_front && (axis_order[axis_index].axis_opposite + 1 == gz->highlight_part)) {
      is_highlight = true;
    }

    UI_GetThemeColor3fv(TH_AXIS_X + axis, axis_color[axis]);
    axis_color[axis][3] = 1.0f;

    /* Color that is full at front, but 50% view background when in back. */
    float fading_color[4];
    interp_v4_v4v4(fading_color, view_color, axis_color[axis], ((depth + 1) * 0.25) + 0.5);

    /* Color that is midway between front and back. */
    float middle_color[4];
    interp_v4_v4v4(middle_color, view_color, axis_color[axis], 0.75f);

    GPU_blend(GPU_BLEND_ALPHA);

    /* Axis Line. */
    if (is_pos || axis_align != -1) {

      /* Extend slightly to meet better at the center. */
      float v_start[3] = {0.0f, 0.0f, 0.0f};
      mul_v3_v3fl(v_start, v_final, -(AXIS_LINE_WIDTH / WIDGET_RADIUS * 0.66f));

      /* Decrease length of line by ball radius. */
      float v_end[3] = {0.0f, 0.0f, 0.0f};
      mul_v3_v3fl(v_end, v_final, 1.0f - AXIS_HANDLE_SIZE);

      immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR);
      immUniform2fv("viewportSize", &viewport_size[2]);
      immUniform1f("lineWidth", AXIS_LINE_WIDTH);
      immBegin(GPU_PRIM_LINES, 2);
      immAttr4fv(color_id, middle_color);
      immVertex3fv(pos_id, v_start);
      immAttr4fv(color_id, fading_color);
      immVertex3fv(pos_id, v_end);
      immEnd();
      immUnbindProgram();
    }

    /* Axis Ball. */
    if (!is_aligned_back) {
      float *inner_color = fading_color;
      float *outline_color = fading_color;
      float negative_color[4];
      if (!is_pos) {
        if (is_aligned_front) {
          interp_v4_v4v4(
              negative_color, blender::float4{1.0f, 1.0f, 1.0f, 1.0f}, axis_color[axis], 0.5f);
          negative_color[3] = MIN2(depth + 1, 1.0f);
          outline_color = negative_color;
        }
        else {
          interp_v4_v4v4(negative_color, view_color, axis_color[axis], 0.25f);
          negative_color[3] = MIN2(depth + 1, 1.0f);
          inner_color = negative_color;
        }
      }

      GPU_matrix_push();
      GPU_matrix_translate_3fv(v_final);
      GPU_matrix_mul(font.matrix);
      /* Size change from back to front: 0.92f - 1.08f. */
      float scale = ((depth + 1) * 0.08f) + 0.92f;
      const float rad = WIDGET_RADIUS * AXIS_HANDLE_SIZE * scale;
      rctf rect{};
      rect.xmin = -rad;
      rect.xmax = rad;
      rect.ymin = -rad;
      rect.ymax = rad;
      UI_draw_roundbox_4fv_ex(
          &rect, inner_color, nullptr, 0.0f, outline_color, AXIS_RING_WIDTH, rad);
      GPU_matrix_pop();
    }

    /* Axis XYZ Character. */
    if ((is_pos || is_highlight || (axis == axis_align)) && !is_aligned_back) {
      float axis_str_width, axis_string_height;
      char axis_str[3] = {char('X' + axis), 0, 0};
      if (!is_pos) {
        axis_str[0] = '-';
        axis_str[1] = 'X' + axis;
      }
      BLF_width_and_height(font.id, axis_str, 3, &axis_str_width, &axis_string_height);

      /* Calculate pixel-aligned location, without this text draws fuzzy. */
      float v_final_px[3];
      mul_v3_m3v3(v_final_px, font.matrix_m3_invert, v_final);
      /* Center the text and pixel align, it's important to round once
       * otherwise the characters are noticeably not-centered.
       * If this wasn't an issue we could use #BLF_position to place the text. */
      v_final_px[0] = roundf(v_final_px[0] - (axis_str_width * (is_pos ? 0.5f : 0.55f)));
      v_final_px[1] = roundf(v_final_px[1] - (axis_string_height / 2.0f));
      mul_m3_v3(font.matrix_m3, v_final_px);
      GPU_matrix_push();
      GPU_matrix_translate_3fv(v_final_px);
      GPU_matrix_mul(font.matrix);
      float text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      if (!is_highlight) {
        zero_v4(text_color);
        text_color[3] = is_active ? 1.0f : 0.9f;
      }
      BLF_color4fv(font.id, text_color);
      BLF_draw(font.id, axis_str, 2);
      GPU_matrix_pop();
    }
  }

  if (use_project_matrix) {
    GPU_matrix_pop_projection();
  }

  GPU_blend(GPU_BLEND_NONE);
  BLF_disable(font.id, BLF_BOLD);
  GPU_matrix_pop();
}

static int gizmo_axis_test_select(bContext * /*C*/, wmGizmo *gz, const int mval[2])
{
  float point_local[2] = {float(mval[0]), float(mval[1])};
  sub_v2_v2(point_local, gz->matrix_basis[3]);
  mul_v2_fl(point_local, 1.0f / gz->scale_final);

  const float len_sq = len_squared_v2(point_local);
  if (len_sq > 1.0) {
    return -1;
  }

  int part_best = -1;
  int part_index = 1;
  /* Use 'SQUARE(HANDLE_SIZE)' if we want to be able to _not_ focus on one of the axis. */
  float i_best_len_sq = FLT_MAX;
  for (int i = 0; i < 3; i++) {
    for (int is_pos = 0; is_pos < 2; is_pos++) {
      const float co[2] = {
          gz->matrix_offset[i][0] * (is_pos ? 1 : -1),
          gz->matrix_offset[i][1] * (is_pos ? 1 : -1),
      };

      bool ok = true;

      /* Check if we're viewing on an axis,
       * there is no point to clicking on the current axis so show the reverse. */
      if (len_squared_v2(co) < 1e-6f && (gz->matrix_offset[i][2] > 0.0f) == is_pos) {
        ok = false;
      }

      if (ok) {
        const float len_axis_sq = len_squared_v2v2(co, point_local);
        if (len_axis_sq < i_best_len_sq) {
          part_best = part_index;
          i_best_len_sq = len_axis_sq;
        }
      }
      part_index += 1;
    }
  }

  if (part_best != -1) {
    return part_best;
  }

  /* The 'gz->scale_final' is already applied when projecting. */
  if (len_sq < 1.0f) {
    return 0;
  }

  return -1;
}

static int gizmo_axis_cursor_get(wmGizmo * /*gz*/)
{
  return WM_CURSOR_DEFAULT;
}

static bool gizmo_axis_screen_bounds_get(bContext *C, wmGizmo *gz, rcti *r_bounding_box)
{
  ScrArea *area = CTX_wm_area(C);
  const float rad = WIDGET_RADIUS;
  r_bounding_box->xmin = gz->matrix_basis[3][0] + area->totrct.xmin - rad;
  r_bounding_box->ymin = gz->matrix_basis[3][1] + area->totrct.ymin - rad;
  r_bounding_box->xmax = r_bounding_box->xmin + rad;
  r_bounding_box->ymax = r_bounding_box->ymin + rad;
  return true;
}

void VIEW3D_GT_navigate_rotate(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "VIEW3D_GT_navigate_rotate";

  /* api callbacks */
  gzt->draw = gizmo_axis_draw;
  gzt->test_select = gizmo_axis_test_select;
  gzt->cursor_get = gizmo_axis_cursor_get;
  gzt->screen_bounds_get = gizmo_axis_screen_bounds_get;

  gzt->struct_size = sizeof(wmGizmo);
}
