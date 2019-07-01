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
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edgizmolib
 *
 * \name Dial Gizmo
 *
 * 3D Gizmo
 *
 * \brief Circle shaped gizmo for circular interaction.
 * Currently no own handling, use with operator only.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' when DialGizmo.use_start_y_axis is set.
 * - `matrix[2]` is the axis the dial rotates around (all dials).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_transform.h"
#include "ED_gizmo_library.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

/* To use custom dials exported to geom_dial_gizmo.c */
// #define USE_GIZMO_CUSTOM_DIAL

typedef struct DialInteraction {
  struct {
    float mval[2];
    /* Only for when using properties. */
    float prop_angle;
  } init;
  struct {
    /* Cache the last angle to detect rotations bigger than -/+ PI. */
    eWM_GizmoFlagTweak tweak_flag;
    float angle;
  } prev;

  /* Number of full rotations. */
  int rotations;
  bool has_drag;
  float angle_increment;

  /* Final output values, used for drawing. */
  struct {
    float angle_ofs;
    float angle_delta;
  } output;
} DialInteraction;

#define DIAL_WIDTH 1.0f
#define DIAL_RESOLUTION 48

/* Could make option, negative to clip more (don't show when view aligned). */
#define DIAL_CLIP_BIAS 0.02

/* -------------------------------------------------------------------- */

static void dial_geom_draw(const float color[4],
                           const float line_width,
                           const bool select,
                           const float axis_modal_mat[4][4],
                           const float clip_plane[4],
                           const float arc_partial_angle,
                           const float arc_inner_factor,
                           const int draw_options)
{
#ifdef USE_GIZMO_CUSTOM_DIAL
  UNUSED_VARS(gz, axis_modal_mat, clip_plane);
  wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_dial, select, color);
#else
  const bool filled = ((draw_options & (select ? (ED_GIZMO_DIAL_DRAW_FLAG_FILL |
                                                  ED_GIZMO_DIAL_DRAW_FLAG_FILL_SELECT) :
                                                 ED_GIZMO_DIAL_DRAW_FLAG_FILL)));

  GPU_line_width(line_width);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (clip_plane) {
    immBindBuiltinProgram(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR);
    immUniform4fv("ClipPlane", clip_plane);
    immUniformMatrix4fv("ModelMatrix", axis_modal_mat);
    glEnable(GL_CLIP_DISTANCE0);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  }

  immUniformColor4fv(color);

  if (filled) {
    if (arc_partial_angle == 0.0f) {
      if (arc_inner_factor == 0.0f) {
        imm_draw_circle_fill_2d(pos, 0, 0, 1.0, DIAL_RESOLUTION);
      }
      else {
        imm_draw_disk_partial_fill_2d(
            pos, 0, 0, arc_inner_factor, 1.0f, DIAL_RESOLUTION, 0, RAD2DEGF(M_PI * 2));
      }
    }
    else {
      float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
      imm_draw_disk_partial_fill_2d(pos,
                                    0,
                                    0,
                                    arc_inner_factor,
                                    1.0f,
                                    DIAL_RESOLUTION,
                                    -arc_partial_deg / 2,
                                    arc_partial_deg);
    }
  }
  else {
    if (arc_partial_angle == 0.0f) {
      imm_draw_circle_wire_2d(pos, 0, 0, 1.0, DIAL_RESOLUTION);
      if (arc_inner_factor != 0.0f) {
        imm_draw_circle_wire_2d(pos, 0, 0, arc_inner_factor, DIAL_RESOLUTION);
      }
    }
    else {
      float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
      imm_draw_circle_partial_wire_2d(
          pos, 0, 0, 1.0, DIAL_RESOLUTION, -arc_partial_deg / 2, arc_partial_deg);
#  if 0
      if (arc_inner_factor != 0.0f) {
        BLI_assert(0);
      }
#  endif
    }
  }

  immUnbindProgram();

  if (clip_plane) {
    glDisable(GL_CLIP_DISTANCE0);
  }

  UNUSED_VARS(select);
#endif
}

/**
 * Draws a line from (0, 0, 0) to \a co_outer, at \a angle.
 */
static void dial_ghostarc_draw_helpline(const float angle,
                                        const float co_outer[3],
                                        const float color[4])
{
  GPU_matrix_push();
  GPU_matrix_rotate_3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4fv(color);

  immBegin(GPU_PRIM_LINE_STRIP, 2);
  immVertex3f(pos, 0.0f, 0, 0.0f);
  immVertex3fv(pos, co_outer);
  immEnd();

  immUnbindProgram();

  GPU_matrix_pop();
}

/**
 * Draws segments to indicate the position of each increment.
 */
static void dial_ghostarc_draw_incremental_angle(const float incremental_angle, const float offset)
{
  const int tot_incr = (2 * M_PI) / incremental_angle;
  GPU_line_width(1.0f);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3f(1.0f, 1.0f, 1.0f);
  immBegin(GPU_PRIM_LINES, tot_incr * 2);

  float v[3] = {0};
  for (int i = 0; i < tot_incr; i++) {
    v[0] = sinf(offset + incremental_angle * i);
    v[1] = cosf(offset + incremental_angle * i);

    mul_v2_fl(v, DIAL_WIDTH * 1.1f);
    immVertex3fv(pos, v);

    mul_v2_fl(v, 1.1f);
    immVertex3fv(pos, v);
  }

  immEnd();
  immUnbindProgram();
}

static void dial_ghostarc_draw(const float angle_ofs,
                               const float angle_delta,
                               const float arc_inner_factor,
                               const float color[4])
{
  const float width_inner = DIAL_WIDTH;
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (arc_inner_factor != 0.0) {
    float color_dark[4] = {0};
    color_dark[3] = color[3] / 2;
    immUniformColor4fv(color_dark);
    imm_draw_disk_partial_fill_2d(pos,
                                  0,
                                  0,
                                  arc_inner_factor,
                                  width_inner,
                                  DIAL_RESOLUTION,
                                  RAD2DEGF(angle_ofs),
                                  RAD2DEGF(M_PI * 2));
  }

  immUniformColor4fv(color);
  imm_draw_disk_partial_fill_2d(pos,
                                0,
                                0,
                                arc_inner_factor,
                                width_inner,
                                DIAL_RESOLUTION,
                                RAD2DEGF(angle_ofs),
                                RAD2DEGF(angle_delta));
  immUnbindProgram();
}

static void dial_ghostarc_get_angles(const wmGizmo *gz,
                                     const wmEvent *event,
                                     const ARegion *ar,
                                     float mat[4][4],
                                     const float co_outer[3],
                                     float *r_start,
                                     float *r_delta)
{
  DialInteraction *inter = gz->interaction_data;
  const RegionView3D *rv3d = ar->regiondata;
  const float mval[2] = {event->x - ar->winrct.xmin, event->y - ar->winrct.ymin};

  /* We might need to invert the direction of the angles. */
  float view_vec[3], axis_vec[3];
  ED_view3d_global_to_vector(rv3d, gz->matrix_basis[3], view_vec);
  normalize_v3_v3(axis_vec, gz->matrix_basis[2]);

  float proj_outer_rel[3];
  mul_v3_project_m4_v3(proj_outer_rel, mat, co_outer);
  sub_v3_v3(proj_outer_rel, gz->matrix_basis[3]);

  float proj_mval_new_rel[3];
  float proj_mval_init_rel[3];
  float dial_plane[4];

  plane_from_point_normal_v3(dial_plane, gz->matrix_basis[3], axis_vec);

  if (!ED_view3d_win_to_3d_on_plane(ar, dial_plane, inter->init.mval, false, proj_mval_init_rel)) {
    goto fail;
  }
  sub_v3_v3(proj_mval_init_rel, gz->matrix_basis[3]);

  if (!ED_view3d_win_to_3d_on_plane(ar, dial_plane, mval, false, proj_mval_new_rel)) {
    goto fail;
  }
  sub_v3_v3(proj_mval_new_rel, gz->matrix_basis[3]);

  const int draw_options = RNA_enum_get(gz->ptr, "draw_options");

  /* Start direction from mouse or set by user. */
  const float *proj_init_rel = (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y) ?
                                   gz->matrix_basis[1] :
                                   proj_mval_init_rel;

  /* Return angles. */
  const float start = angle_wrap_rad(
      angle_signed_on_axis_v3v3_v3(proj_outer_rel, proj_init_rel, axis_vec));
  const float delta = angle_wrap_rad(
      angle_signed_on_axis_v3v3_v3(proj_mval_init_rel, proj_mval_new_rel, axis_vec));

  /* Change of sign, we passed the 180 degree threshold. This means we need to add a turn
   * to distinguish between transition from 0 to -1 and -PI to +PI, use comparison with PI/2.
   * Logic taken from #BLI_dial_angle */
  if ((delta * inter->prev.angle < 0.0f) && (fabsf(inter->prev.angle) > (float)M_PI_2)) {
    if (inter->prev.angle < 0.0f) {
      inter->rotations--;
    }
    else {
      inter->rotations++;
    }
  }
  inter->prev.angle = delta;

  const bool wrap_angle = RNA_boolean_get(gz->ptr, "wrap_angle");
  const double delta_final = (double)delta + ((2 * M_PI) * (double)inter->rotations);
  *r_start = start;
  *r_delta = (float)(wrap_angle ? fmod(delta_final, 2 * M_PI) : delta_final);
  return;

  /* If we can't project (unlikely). */
fail:
  *r_start = 0.0;
  *r_delta = 0.0;
}

static void dial_ghostarc_draw_with_helplines(const float angle_ofs,
                                              const float angle_delta,
                                              const float arc_inner_factor,
                                              const float color_helpline[4],
                                              const int draw_options)
{
  /* Coordinate at which the arc drawing will be started. */
  const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f};
  dial_ghostarc_draw(
      angle_ofs, angle_delta, arc_inner_factor, (const float[4]){0.8f, 0.8f, 0.8f, 0.4f});
  GPU_line_width(1.0f);
  dial_ghostarc_draw_helpline(angle_ofs, co_outer, color_helpline);
  if (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE) {
    GPU_line_width(3.0f);
  }
  dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, color_helpline);
}

static void dial_draw_intern(
    const bContext *C, wmGizmo *gz, const bool select, const bool highlight, float clip_plane[4])
{
  float matrix_final[4][4];
  float color[4];

  (void)C;
  BLI_assert(CTX_wm_area(C)->spacetype == SPACE_VIEW3D);

  gizmo_color_get(gz, highlight, color);

  WM_gizmo_calc_matrix_final(gz, matrix_final);

  const float arc_partial_angle = RNA_float_get(gz->ptr, "arc_partial_angle");
  const float arc_inner_factor = RNA_float_get(gz->ptr, "arc_inner_factor");
  int draw_options = RNA_enum_get(gz->ptr, "draw_options");
  float angle_ofs = 0.0f;
  float angle_delta = 0.0f;
  float angle_increment = 0.0f;

  if (select) {
    draw_options &= ~ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE;
  }

  if (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE && (gz->flag & WM_GIZMO_DRAW_VALUE)) {
    DialInteraction *inter = gz->interaction_data;
    if (inter) {
      angle_ofs = inter->output.angle_ofs;
      angle_delta = inter->output.angle_delta;
      angle_increment = inter->angle_increment;
    }
    else {
      wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
      if (WM_gizmo_target_property_is_valid(gz_prop)) {
        angle_delta = WM_gizmo_target_property_float_get(gz, gz_prop);
      }
    }
  }

  ED_gizmotypes_dial_3d_draw_util(gz->matrix_basis,
                                  matrix_final,
                                  gz->line_width,
                                  color,
                                  select,
                                  &(struct Dial3dParams){
                                      .draw_options = draw_options,
                                      .angle_ofs = angle_ofs,
                                      .angle_delta = angle_delta,
                                      .angle_increment = angle_increment,
                                      .arc_partial_angle = arc_partial_angle,
                                      .arc_inner_factor = arc_inner_factor,
                                      .clip_plane = clip_plane,
                                  });
}

static void gizmo_dial_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
  float clip_plane_buf[4];
  const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
  float *clip_plane = (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_CLIP) ? clip_plane_buf : NULL;

  if (clip_plane) {
    ARegion *ar = CTX_wm_region(C);
    RegionView3D *rv3d = ar->regiondata;

    copy_v3_v3(clip_plane, rv3d->viewinv[2]);
    clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], gz->matrix_basis[3]);
    clip_plane[3] += DIAL_CLIP_BIAS;
  }

  GPU_select_load_id(select_id);
  dial_draw_intern(C, gz, true, false, clip_plane);

  if (clip_plane) {
    glDisable(GL_CLIP_DISTANCE0);
  }
}

static void gizmo_dial_draw(const bContext *C, wmGizmo *gz)
{
  const bool is_modal = gz->state & WM_GIZMO_STATE_MODAL;
  const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;
  float clip_plane_buf[4];
  const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
  float *clip_plane = (!is_modal && (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_CLIP)) ?
                          clip_plane_buf :
                          NULL;

  if (clip_plane) {
    ARegion *ar = CTX_wm_region(C);
    RegionView3D *rv3d = ar->regiondata;

    copy_v3_v3(clip_plane, rv3d->viewinv[2]);
    clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], gz->matrix_basis[3]);
    clip_plane[3] += DIAL_CLIP_BIAS;
  }

  GPU_blend(true);
  dial_draw_intern(C, gz, false, is_highlight, clip_plane);
  GPU_blend(false);
}

static int gizmo_dial_modal(bContext *C,
                            wmGizmo *gz,
                            const wmEvent *event,
                            eWM_GizmoFlagTweak tweak_flag)
{
  DialInteraction *inter = gz->interaction_data;
  if ((event->type != MOUSEMOVE) && (inter->prev.tweak_flag == tweak_flag)) {
    return OPERATOR_RUNNING_MODAL;
  }
  /* Coordinate at which the arc drawing will be started. */
  const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f};
  float angle_ofs, angle_delta, angle_increment = 0.0f;

  dial_ghostarc_get_angles(
      gz, event, CTX_wm_region(C), gz->matrix_basis, co_outer, &angle_ofs, &angle_delta);

  if (tweak_flag & WM_GIZMO_TWEAK_SNAP) {
    angle_increment = RNA_float_get(gz->ptr, "incremental_angle");
    angle_delta = (float)roundf((double)angle_delta / angle_increment) * angle_increment;
  }
  if (tweak_flag & WM_GIZMO_TWEAK_PRECISE) {
    angle_increment *= 0.2f;
    angle_delta *= 0.2f;
  }
  if (angle_delta != 0.0f) {
    inter->has_drag = true;
  }

  inter->angle_increment = angle_increment;
  inter->output.angle_delta = angle_delta;
  inter->output.angle_ofs = angle_ofs;

  /* Set the property for the operator and call its modal function. */
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
  if (WM_gizmo_target_property_is_valid(gz_prop)) {
    WM_gizmo_target_property_float_set(C, gz, gz_prop, inter->init.prop_angle + angle_delta);
  }

  inter->prev.tweak_flag = tweak_flag;

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_dial_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
  DialInteraction *inter = gz->interaction_data;
  bool use_reset_value = false;
  float reset_value = 0.0f;
  if (cancel) {
    /* Set the property for the operator and call its modal function. */
    wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
    if (WM_gizmo_target_property_is_valid(gz_prop)) {
      use_reset_value = true;
      reset_value = inter->init.prop_angle;
    }
  }
  else {
    if (inter->has_drag == false) {
      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "click_value");
      if (RNA_property_is_set(gz->ptr, prop)) {
        use_reset_value = true;
        reset_value = RNA_property_float_get(gz->ptr, prop);
      }
    }
  }

  if (use_reset_value) {
    wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
    if (WM_gizmo_target_property_is_valid(gz_prop)) {
      WM_gizmo_target_property_float_set(C, gz, gz_prop, reset_value);
    }
  }
}

static void gizmo_dial_setup(wmGizmo *gz)
{
  const float dir_default[3] = {0.0f, 0.0f, 1.0f};

  /* defaults */
  copy_v3_v3(gz->matrix_basis[2], dir_default);
}

static int gizmo_dial_invoke(bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
  DialInteraction *inter = MEM_callocN(sizeof(DialInteraction), __func__);

  inter->init.mval[0] = event->mval[0];
  inter->init.mval[1] = event->mval[1];

  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
  if (WM_gizmo_target_property_is_valid(gz_prop)) {
    inter->init.prop_angle = WM_gizmo_target_property_float_get(gz, gz_prop);
  }

  gz->interaction_data = inter;

  return OPERATOR_RUNNING_MODAL;
}

/* -------------------------------------------------------------------- */
/** \name Dial Gizmo API
 *
 * \{ */

void ED_gizmotypes_dial_3d_draw_util(const float matrix_basis[4][4],
                                     const float matrix_final[4][4],
                                     const float line_width,
                                     const float color[4],
                                     const bool select,
                                     struct Dial3dParams *params)
{
  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);

  GPU_polygon_smooth(false);

  if ((params->draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE) != 0) {
    /* Draw rotation indicator arc first. */
    dial_ghostarc_draw_with_helplines(params->angle_ofs,
                                      params->angle_delta,
                                      params->arc_inner_factor,
                                      color,
                                      params->draw_options);

    if ((params->draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR) != 0) {
      dial_ghostarc_draw_with_helplines(params->angle_ofs + M_PI,
                                        params->angle_delta,
                                        params->arc_inner_factor,
                                        color,
                                        params->draw_options);
    }
  }

  if (params->angle_increment) {
    dial_ghostarc_draw_incremental_angle(params->angle_increment, params->angle_ofs);
  }

  /* Draw actual dial gizmo. */
  dial_geom_draw(color,
                 line_width,
                 select,
                 matrix_basis,
                 params->clip_plane,
                 params->arc_partial_angle,
                 params->arc_inner_factor,
                 params->draw_options);

  GPU_matrix_pop();
}

static void GIZMO_GT_dial_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_dial_3d";

  /* api callbacks */
  gzt->draw = gizmo_dial_draw;
  gzt->draw_select = gizmo_dial_draw_select;
  gzt->setup = gizmo_dial_setup;
  gzt->invoke = gizmo_dial_invoke;
  gzt->modal = gizmo_dial_modal;
  gzt->exit = gizmo_dial_exit;

  gzt->struct_size = sizeof(wmGizmo);

  /* rna */
  static EnumPropertyItem rna_enum_draw_options[] = {
      {ED_GIZMO_DIAL_DRAW_FLAG_CLIP, "CLIP", 0, "Clipped", ""},
      {ED_GIZMO_DIAL_DRAW_FLAG_FILL, "FILL", 0, "Filled", ""},
      {ED_GIZMO_DIAL_DRAW_FLAG_FILL_SELECT, "FILL_SELECT", 0, "Use fill for selection test", ""},
      {ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR, "ANGLE_MIRROR", 0, "Angle Mirror", ""},
      {ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y, "ANGLE_START_Y", 0, "Angle Start Y", ""},
      {ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE, "ANGLE_VALUE", 0, "Show Angle Value", ""},
      {0, NULL, 0, NULL, NULL},
  };
  RNA_def_enum_flag(gzt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");
  RNA_def_boolean(gzt->srna, "wrap_angle", true, "Wrap Angle", "");
  RNA_def_float_factor(
      gzt->srna, "arc_inner_factor", 0.0f, 0.0f, 1.0f, "Arc Inner Factor", "", 0.0f, 1.0f);
  RNA_def_float_factor(gzt->srna,
                       "arc_partial_angle",
                       0.0f,
                       0.0f,
                       M_PI * 2,
                       "Show Partial Dial",
                       "",
                       0.0f,
                       M_PI * 2);
  RNA_def_float_factor(gzt->srna,
                       "incremental_angle",
                       SNAP_INCREMENTAL_ANGLE,
                       0.0f,
                       M_PI * 2,
                       "Incremental Angle",
                       "Angle to snap in steps",
                       0.0f,
                       M_PI * 2);
  RNA_def_float(gzt->srna,
                "click_value",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Click Value",
                "Value to use for a single click action",
                -FLT_MAX,
                FLT_MAX);

  WM_gizmotype_target_property_def(gzt, "offset", PROP_FLOAT, 1);
}

void ED_gizmotypes_dial_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_dial_3d);
}

/** \} */
