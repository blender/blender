/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
#include "BLI_sort_utils.h"

#include "BKE_context.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "view3d_intern.h"

#define USE_AXIS_FONT
#define USE_FADE_BACKGROUND

#ifdef USE_AXIS_FONT
#  include "BLF_api.h"
#endif

#define DIAL_RESOLUTION 32

/* Sizes of axis spheres containing XYZ characters. */
#define AXIS_HANDLE_SIZE_FG 0.19f
/* When pointing away from the view. */
#define AXIS_HANDLE_SIZE_BG 0.15f
/* How far axis handles are away from the center. */
#define AXIS_HANDLE_OFFSET (1.0f - AXIS_HANDLE_SIZE_FG)

struct AxisDrawInfo {
  /* Matrix is needed for screen-aligned font drawing. */
#ifdef USE_AXIS_FONT
  float matrix_final[4][4];
#endif
#ifdef USE_FADE_BACKGROUND
  float color_bg[3];
#endif
};

#ifndef USE_AXIS_FONT
/**
 * \param viewmat_local_unit: is typically the 'rv3d->viewmatob'
 * copied into a 3x3 matrix and normalized.
 */
static void draw_xyz_wire(
    uint pos_id, const float viewmat_local_unit[3][3], const float c[3], float size, int axis)
{
  int line_type;
  float buffer[4][3];
  int n = 0;

  float v1[3] = {0.0f, 0.0f, 0.0f}, v2[3] = {0.0f, 0.0f, 0.0f};
  float dim = size * 0.1f;
  float dx[3], dy[3];

  dx[0] = dim;
  dx[1] = 0.0f;
  dx[2] = 0.0f;
  dy[0] = 0.0f;
  dy[1] = dim;
  dy[2] = 0.0f;

  switch (axis) {
    case 0: /* x axis */
      line_type = GPU_PRIM_LINES;

      /* bottom left to top right */
      negate_v3_v3(v1, dx);
      sub_v3_v3(v1, dy);
      copy_v3_v3(v2, dx);
      add_v3_v3(v2, dy);

      copy_v3_v3(buffer[n++], v1);
      copy_v3_v3(buffer[n++], v2);

      /* top left to bottom right */
      mul_v3_fl(dy, 2.0f);
      add_v3_v3(v1, dy);
      sub_v3_v3(v2, dy);

      copy_v3_v3(buffer[n++], v1);
      copy_v3_v3(buffer[n++], v2);

      break;
    case 1: /* y axis */
      line_type = GPU_PRIM_LINES;

      /* bottom left to top right */
      mul_v3_fl(dx, 0.75f);
      negate_v3_v3(v1, dx);
      sub_v3_v3(v1, dy);
      copy_v3_v3(v2, dx);
      add_v3_v3(v2, dy);

      copy_v3_v3(buffer[n++], v1);
      copy_v3_v3(buffer[n++], v2);

      /* top left to center */
      mul_v3_fl(dy, 2.0f);
      add_v3_v3(v1, dy);
      zero_v3(v2);

      copy_v3_v3(buffer[n++], v1);
      copy_v3_v3(buffer[n++], v2);

      break;
    case 2: /* z axis */
      line_type = GPU_PRIM_LINE_STRIP;

      /* start at top left */
      negate_v3_v3(v1, dx);
      add_v3_v3(v1, dy);

      copy_v3_v3(buffer[n++], v1);

      mul_v3_fl(dx, 2.0f);
      add_v3_v3(v1, dx);

      copy_v3_v3(buffer[n++], v1);

      mul_v3_fl(dy, 2.0f);
      sub_v3_v3(v1, dx);
      sub_v3_v3(v1, dy);

      copy_v3_v3(buffer[n++], v1);

      add_v3_v3(v1, dx);

      copy_v3_v3(buffer[n++], v1);

      break;
    default:
      BLI_assert(0);
      return;
  }

  for (int i = 0; i < n; i++) {
    mul_transposed_m3_v3((float(*)[3])viewmat_local_unit, buffer[i]);
    add_v3_v3(buffer[i], c);
  }

  immBegin(line_type, n);
  for (int i = 0; i < n; i++) {
    immVertex3fv(pos_id, buffer[i]);
  }
  immEnd();
}
#endif /* !USE_AXIS_FONT */

/**
 * \param draw_info: Extra data needed for drawing.
 */
static void axis_geom_draw(const wmGizmo *gz,
                           const float color[4],
                           const bool select,
                           const struct AxisDrawInfo *draw_info)
{

  GPU_line_width(gz->line_width);

  GPUVertFormat *format = immVertexFormat();
  const uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  struct {
    float depth;
    char index;
    char axis;
    bool is_pos;
  } axis_order[6] = {
      {-gz->matrix_offset[0][2], 0, 0, false},
      {+gz->matrix_offset[0][2], 1, 0, true},
      {-gz->matrix_offset[1][2], 2, 1, false},
      {+gz->matrix_offset[1][2], 3, 1, true},
      {-gz->matrix_offset[2][2], 4, 2, false},
      {+gz->matrix_offset[2][2], 5, 2, true},
  };

  int axis_align = -1;
  for (int axis = 0; axis < 3; axis++) {
    if (len_squared_v2(gz->matrix_offset[axis]) < 1e-6f) {
      axis_align = axis;
      break;
    }
  }

  /* Show backwards pointing highlight on-top (else we can't see it at all). */
  if ((select == false) && (gz->highlight_part > 0) && (axis_align != -1)) {
    if (axis_order[gz->highlight_part - 1].is_pos == false) {
      axis_order[gz->highlight_part - 1].depth = FLT_MAX;
    }
  }

  qsort(&axis_order, ARRAY_SIZE(axis_order), sizeof(axis_order[0]), BLI_sortutil_cmp_float);

  static const float axis_highlight[4] = {1, 1, 1, 1};
  static const float axis_black[4] = {0, 0, 0, 1};
  static float axis_color[3][4];

  const float axis_depth_bias = 0.01f;
  const float sphere_scale = 1.15f;

#ifdef USE_AXIS_FONT
  struct {
    float matrix[4][4];
    int id;
  } font;

  if (select == false) {
    font.id = blf_mono_font;
    BLF_disable(font.id, BLF_ROTATION | BLF_SHADOW | BLF_MATRIX | BLF_ASPECT | BLF_WORD_WRAP);
    BLF_color4fv(font.id, axis_black);
    BLF_size(font.id, 11 * U.dpi_fac, 72);

    /* Calculate the inverse of the (matrix_final * matrix_offset).
     * This allows us to use the final location, while reversing the rotation so fonts
     * show without any rotation. */
    float m3[3][3];
    float m3_offset[3][3];
    copy_m3_m4(m3, draw_info->matrix_final);
    copy_m3_m4(m3_offset, gz->matrix_offset);
    mul_m3_m3m3(m3, m3, m3_offset);
    invert_m3(m3);
    copy_m4_m3(font.matrix, m3);
  }
#endif

  /* When the cursor is over any of the gizmos (show circle backdrop). */
  const bool is_active = (color[3] != 0.0f);

  const float clip_range = gz->scale_final * sphere_scale;
  bool use_project_matrix = (clip_range >= -GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT);
  if (use_project_matrix) {
    GPU_matrix_push_projection();
    GPU_matrix_ortho_set_z(-clip_range, clip_range);
  }

  /* Circle defining active area. */
  if (is_active) {
    immUniformColor4fv(color);
    imm_draw_circle_fill_3d(pos_id, 0, 0, 1.0f, DIAL_RESOLUTION);
  }

  GPU_matrix_push();
  GPU_matrix_mul(gz->matrix_offset);

  for (int axis_index = 0; axis_index < ARRAY_SIZE(axis_order); axis_index++) {
    const int index = axis_order[axis_index].index;
    const int axis = axis_order[axis_index].axis;
    const bool is_pos = axis_order[axis_index].is_pos;
    const bool is_highlight = index + 1 == gz->highlight_part;

    UI_GetThemeColor3fv(TH_AXIS_X + axis, axis_color[axis]);
    axis_color[axis][3] = 1.0f;

    const int index_z = axis;
    const int index_y = (axis + 1) % 3;
    const int index_x = (axis + 2) % 3;

    bool ok = true;

    /* Skip view align axis when selecting (allows to switch to opposite side). */
    if (select && ((axis_align == axis) && (gz->matrix_offset[axis][2] > 0.0f) == is_pos)) {
      ok = false;
    }
    if (ok) {
      /* Check aligned, since the front axis won't display in this case,
       * and we want to make sure all 3 axes have a character at all times. */
      const bool show_axis_char = (is_pos || (axis == axis_align));
      const float v[3] = {0, 0, AXIS_HANDLE_OFFSET * (is_pos ? 1 : -1)};
      const float v_final[3] = {v[index_x], v[index_y], v[index_z]};
      const float *color_current = is_highlight ? axis_highlight : axis_color[axis];
      float color_current_fade[4];

      /* Flip the faded state when axis aligned, since we're hiding the front-mode axis
       * otherwise we see the color for the back-most axis, which is useful for
       * click-to-rotate 180d but not useful to visualize.
       *
       * Use depth bias so axis-aligned views show the positive axis as being in-front.
       * This is a detail so primary axes show as dominant.
       */
      const bool is_pos_color = (axis_order[axis_index].depth >
                                 (axis_depth_bias * (is_pos ? -1 : 1)));

      if (select == false) {
#ifdef USE_FADE_BACKGROUND
        interp_v3_v3v3(
            color_current_fade, draw_info->color_bg, color_current, is_highlight ? 1.0 : 0.5f);
        color_current_fade[3] = color_current[3];
#else
        copy_v4_v4(color_current_fade, color_current);
        color_current_fade[3] *= 0.2;
#endif
      }
      else {
        copy_v4_fl(color_current_fade, 1.0f);
      }

      /* Axis Line. */
      if (is_pos) {
        float v_start[3];
        GPU_line_width(2.0f);
        immUniformColor4fv(is_pos_color ? color_current : color_current_fade);
        immBegin(GPU_PRIM_LINES, 2);
        if (axis_align == -1) {
          zero_v3(v_start);
        }
        else {
          /* When axis aligned we don't draw the front most axis
           * (allowing us to switch to the opposite side).
           * In this case don't draw lines over axis pointing away from us
           * because it obscures character and looks noisy.
           */
          mul_v3_v3fl(v_start, v_final, 0.3f);
        }
        immVertex3fv(pos_id, v_start);
        immVertex3fv(pos_id, v_final);
        immEnd();
      }

      /* Axis Ball. */
      {
        GPU_matrix_push();
        GPU_matrix_translate_3fv(v_final);
        GPU_matrix_scale_1f(is_pos ? AXIS_HANDLE_SIZE_FG : AXIS_HANDLE_SIZE_BG);

        GPUBatch *sphere = GPU_batch_preset_sphere(0);
        GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);

        /* Black outlines for negative axis balls, otherwise they can be hard to see since
         * they use a faded color which can be similar to the circle backdrop in tone. */
        if (is_active && !is_highlight && !is_pos && !select && !(axis_align == axis)) {
          static const float axis_black_faded[4] = {0, 0, 0, 0.2f};
          GPU_matrix_scale_1f(sphere_scale);
          GPU_batch_uniform_4fv(sphere, "color", axis_black_faded);
          GPU_batch_draw(sphere);
          GPU_matrix_scale_1f(1.0 / sphere_scale);
        }

        GPU_batch_uniform_4fv(sphere, "color", is_pos_color ? color_current : color_current_fade);
        GPU_batch_draw(sphere);
        GPU_matrix_pop();
      }

      /* Axis XYZ Character. */
      if (show_axis_char && (select == false)) {
#ifdef USE_AXIS_FONT
        immUnbindProgram();

        GPU_matrix_push();
        GPU_matrix_translate_3fv(v_final);
        GPU_matrix_mul(font.matrix);

        const char axis_str[2] = {'X' + axis, 0};
        float offset[2] = {0};
        BLF_width_and_height(font.id, axis_str, 2, &offset[0], &offset[1]);
        BLF_position(font.id, roundf(offset[0] * -0.5f), roundf(offset[1] * -0.5f), 0);
        BLF_draw_ascii(font.id, axis_str, 2);
        GPU_blend(true); /* XXX, blf disables */
        GPU_matrix_pop();

        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
#else
        GPU_line_width(1.0f);
        float m3[3][3];
        copy_m3_m4(m3, gz->matrix_offset);
        immUniformColor4fv(axis_black);
        draw_xyz_wire(pos_id, m3, v_final, 1.0, axis);
#endif
      }
    }
  }

  GPU_matrix_pop();
  immUnbindProgram();

  if (use_project_matrix) {
    GPU_matrix_pop_projection();
  }
}

static void axis3d_draw_intern(const bContext *C,
                               wmGizmo *gz,
                               const bool select,
                               const bool highlight)
{
  const float *color = highlight ? gz->color_hi : gz->color;
  float matrix_final[4][4];
  float matrix_unit[4][4];

  unit_m4(matrix_unit);

  WM_gizmo_calc_matrix_final_params(gz,
                                    &((struct WM_GizmoMatrixParams){
                                        .matrix_offset = matrix_unit,
                                    }),
                                    matrix_final);

  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);

  struct AxisDrawInfo draw_info;
#ifdef USE_AXIS_FONT
  if (select == false) {
    copy_m4_m4(draw_info.matrix_final, matrix_final);
  }
#endif

#ifdef USE_FADE_BACKGROUND
  if (select == false) {
    ED_view3d_background_color_get(CTX_data_scene(C), CTX_wm_view3d(C), draw_info.color_bg);
  }
#else
  UNUSED_VARS(C);
#endif

  GPU_blend(true);
  axis_geom_draw(gz, color, select, &draw_info);
  GPU_blend(false);
  GPU_matrix_pop();
}

static void gizmo_axis_draw(const bContext *C, wmGizmo *gz)
{
  const bool is_modal = gz->state & WM_GIZMO_STATE_MODAL;
  const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;

  (void)is_modal;

  GPU_blend(true);
  axis3d_draw_intern(C, gz, false, is_highlight);
  GPU_blend(false);
}

static int gizmo_axis_test_select(bContext *UNUSED(C), wmGizmo *gz, const int mval[2])
{
  float point_local[2] = {UNPACK2(mval)};
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

static int gizmo_axis_cursor_get(wmGizmo *UNUSED(gz))
{
  return WM_CURSOR_DEFAULT;
}

void VIEW3D_GT_navigate_rotate(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "VIEW3D_GT_navigate_rotate";

  /* api callbacks */
  gzt->draw = gizmo_axis_draw;
  gzt->test_select = gizmo_axis_test_select;
  gzt->cursor_get = gizmo_axis_cursor_get;

  gzt->struct_size = sizeof(wmGizmo);
}
