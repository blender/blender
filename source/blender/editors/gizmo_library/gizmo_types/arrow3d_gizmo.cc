/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Arrow Gizmo
 *
 * 2D/3D Gizmo
 *
 * \brief Simple arrow gizmo which is dragged into a certain direction.
 * The arrow head can have varying shapes, e.g. cone, box, etc.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' for gizmo types that have an up.
 * - `matrix[2]` is the arrow direction (for all arrows).
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "DNA_view3d_types.h"

#include "BKE_context.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_select.hh"
#include "GPU_state.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.hh"

// /** To use custom arrows exported to `geom_arrow_gizmo.cc`. */
// #define USE_GIZMO_CUSTOM_ARROWS

/* Margin to add when selecting the arrow. */
#define ARROW_SELECT_THRESHOLD_PX (5)

struct ArrowGizmo3D {
  wmGizmo gizmo;
  GizmoCommonData data;
};

struct ArrowGizmoInteraction {
  GizmoInteraction inter;
  float init_arrow_length;
};

/* -------------------------------------------------------------------- */

static void gizmo_arrow_matrix_basis_get(const wmGizmo *gz, float r_matrix[4][4])
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

  copy_m4_m4(r_matrix, arrow->gizmo.matrix_basis);
  madd_v3_v3fl(r_matrix[3], arrow->gizmo.matrix_basis[2], arrow->data.offset);
}

static void arrow_draw_geom(const ArrowGizmo3D *arrow,
                            const bool select,
                            const float color[4],
                            const float arrow_length)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  bool unbind_shader = true;
  const int draw_style = RNA_enum_get(arrow->gizmo.ptr, "draw_style");
  const int draw_options = RNA_enum_get(arrow->gizmo.ptr, "draw_options");

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);

  if (draw_style == ED_GIZMO_ARROW_STYLE_CROSS) {
    immUniform1f("lineWidth", U.pixelsize + WM_gizmo_select_bias(select));
    immUniformColor4fv(color);

    immBegin(GPU_PRIM_LINES, 4);
    immVertex3f(pos, -1.0f, 0.0f, 0.0f);
    immVertex3f(pos, 1.0f, 0.0f, 0.0f);
    immVertex3f(pos, 0.0f, -1.0f, 0.0f);
    immVertex3f(pos, 0.0f, 1.0f, 0.0f);
    immEnd();
  }
  else if (draw_style == ED_GIZMO_ARROW_STYLE_CONE) {
    float aspect[2];
    RNA_float_get_array(arrow->gizmo.ptr, "aspect", aspect);
    const float unitx = aspect[0];
    const float unity = aspect[1];
    const float vec[4][3] = {
        {-unitx, -unity, 0},
        {unitx, -unity, 0},
        {unitx, unity, 0},
        {-unitx, unity, 0},
    };

    immUniform1f("lineWidth",
                 (arrow->gizmo.line_width * U.pixelsize) + WM_gizmo_select_bias(select));
    wm_gizmo_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GPU_PRIM_LINE_LOOP);
  }
  else if (draw_style == ED_GIZMO_ARROW_STYLE_PLANE) {
    /* Increase the size a bit during selection. These are relatively easy to hit. */
    const float scale = select ? 0.15f : 0.1f;
    const float verts[4][3] = {
        {0, 0, 0},
        {scale, 0, scale},
        {0, 0, 2 * scale},
        {-scale, 0, scale},
    };

    const float color_inner[4] = {UNPACK3(color), color[3] * 0.5f};

    /* Translate to line end. */
    GPU_matrix_push();
    GPU_matrix_translate_3f(0.0f, 0.0f, arrow_length);

    immUniform1f("lineWidth",
                 (arrow->gizmo.line_width * U.pixelsize) + WM_gizmo_select_bias(select));
    wm_gizmo_vec_draw(color, verts, ARRAY_SIZE(verts), pos, GPU_PRIM_LINE_LOOP);

    immUnbindProgram();
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    wm_gizmo_vec_draw(color_inner, verts, ARRAY_SIZE(verts), pos, GPU_PRIM_TRI_FAN);
    GPU_matrix_pop();
  }
  else {
#ifdef USE_GIZMO_CUSTOM_ARROWS
    wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_arrow, select, color);
#else
    const float vec[2][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, arrow_length},
    };

    if (draw_options & ED_GIZMO_ARROW_DRAW_FLAG_STEM) {
      immUniform1f("lineWidth",
                   (arrow->gizmo.line_width * U.pixelsize) + WM_gizmo_select_bias(select));
      wm_gizmo_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GPU_PRIM_LINE_STRIP);
    }
    else {
      immUniformColor4fv(color);
    }

    /* *** draw arrow head *** */

    GPU_matrix_push();

    /* NOTE: ideally #ARROW_SELECT_THRESHOLD_PX would be added here, however adding a
     * margin in pixel space isn't so simple, nor is it as important as for the arrow stem. */
    if (draw_style == ED_GIZMO_ARROW_STYLE_BOX) {
      /* Increase the size during selection so it is wider than other lines. */
      const float size = select ? 0.11f : 0.05f;

      /* translate to line end with some extra offset so box starts exactly where line ends */
      GPU_matrix_translate_3f(0.0f, 0.0f, arrow_length + size);
      /* scale down to box size */
      GPU_matrix_scale_3f(size, size, size);

      /* draw cube */
      immUnbindProgram();
      unbind_shader = false;
      wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, select, color);
    }
    else {
      BLI_assert(draw_style == ED_GIZMO_ARROW_STYLE_NORMAL);

      /* Increase the size during selection, but mostly wider. */
      const float len = select ? 0.35f : 0.25f;
      const float width = select ? 0.12f : 0.06f;

      /* translate to line end */
      GPU_matrix_translate_3f(0.0f, 0.0f, arrow_length);

      immUnbindProgram();
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor4fv(color);

      imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
      imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);
    }

    GPU_matrix_pop();
#endif /* USE_GIZMO_CUSTOM_ARROWS */
  }

  if (unbind_shader) {
    immUnbindProgram();
  }

  if (draw_options & ED_GIZMO_ARROW_DRAW_FLAG_ORIGIN) {
    const float point_size = 10 * U.pixelsize;
    GPU_program_point_size(true);
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
    immUniform1f("size", point_size);
    immUniformColor4fv(color);
    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3f(pos, 0.0f, 0.0f, 0.0f);
    immEnd();
    immUnbindProgram();
    GPU_program_point_size(false);
  }
}

static void arrow_draw_intern(ArrowGizmo3D *arrow, const bool select, const bool highlight)
{
  wmGizmo *gz = &arrow->gizmo;
  const float arrow_length = RNA_float_get(gz->ptr, "length");
  float color[4];
  float matrix_final[4][4];

  gizmo_color_get(gz, highlight, color);

  WM_gizmo_calc_matrix_final(gz, matrix_final);

  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);
  GPU_blend(GPU_BLEND_ALPHA);
  arrow_draw_geom(arrow, select, color, arrow_length);
  GPU_blend(GPU_BLEND_NONE);

  GPU_matrix_pop();

  if (gz->interaction_data) {
    ArrowGizmoInteraction *arrow_inter = static_cast<ArrowGizmoInteraction *>(
        gz->interaction_data);

    GPU_matrix_push();
    GPU_matrix_mul(arrow_inter->inter.init_matrix_final);

    GPU_blend(GPU_BLEND_ALPHA);
    arrow_draw_geom(
        arrow, select, blender::float4{0.5f, 0.5f, 0.5f, 0.5f}, arrow_inter->init_arrow_length);
    GPU_blend(GPU_BLEND_NONE);

    GPU_matrix_pop();
  }
}

static void gizmo_arrow_draw_select(const bContext * /*C*/, wmGizmo *gz, int select_id)
{
  GPU_select_load_id(select_id);
  arrow_draw_intern((ArrowGizmo3D *)gz, true, false);
}

static void gizmo_arrow_draw(const bContext * /*C*/, wmGizmo *gz)
{
  arrow_draw_intern((ArrowGizmo3D *)gz, false, (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0);
}

/**
 * Selection for 2D views.
 */
static int gizmo_arrow_test_select(bContext * /*C*/, wmGizmo *gz, const int mval[2])
{
  /* This following values are based on manual inspection of `verts[]` defined in
   * `geom_arrow_gizmo.cc`. */
  const float head_center_z = (0.974306f + 1.268098f) / 2;
  const float head_geo_x = 0.051304f;
  const float stem_geo_x = 0.012320f;

  /* Project into 2D space since it simplifies pixel threshold tests. */
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  const float arrow_length = RNA_float_get(arrow->gizmo.ptr, "length") * head_center_z;

  float matrix_final[4][4];
  WM_gizmo_calc_matrix_final(gz, matrix_final);

  /* Arrow in pixel space. */
  const float arrow_start[2] = {matrix_final[3][0], matrix_final[3][1]};
  float arrow_end[2];
  {
    float co[3] = {0, 0, arrow_length};
    mul_m4_v3(matrix_final, co);
    copy_v2_v2(arrow_end, co);
  }

  const float scale_final = mat4_to_scale(matrix_final);
  const float head_width = ARROW_SELECT_THRESHOLD_PX * scale_final * head_geo_x;
  const float stem_width = ARROW_SELECT_THRESHOLD_PX * scale_final * stem_geo_x;
  float select_threshold_base = gz->line_width * U.pixelsize;

  const float mval_fl[2] = {float(mval[0]), float(mval[1])};

  /* Distance to arrow head. */
  if (len_squared_v2v2(mval_fl, arrow_end) < square_f(select_threshold_base + head_width)) {
    return 0;
  }

  /* Distance to arrow stem. */
  float co_isect[2];
  const float lambda = closest_to_line_v2(co_isect, mval_fl, arrow_start, arrow_end);
  /* Clamp inside the line, to avoid overlapping with other gizmos,
   * especially around the start of the arrow. */
  if (lambda >= 0.0f && lambda <= 1.0f) {
    if (len_squared_v2v2(mval_fl, co_isect) < square_f(select_threshold_base + stem_width)) {
      return 0;
    }
  }

  return -1;
}

/**
 * Calculate arrow offset independent from prop min value,
 * meaning the range will not be offset by min value first.
 */
static wmOperatorStatus gizmo_arrow_modal(bContext *C,
                                          wmGizmo *gz,
                                          const wmEvent *event,
                                          eWM_GizmoFlagTweak tweak_flag)
{
  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  GizmoInteraction *inter = static_cast<GizmoInteraction *>(gz->interaction_data);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  float offset[3];
  float facdir = 1.0f;

  /* A pair: (source, destination). */
  struct {
    blender::float2 mval;
    float ray_origin[3], ray_direction[3];
    float location[3];
  } proj[2] = {};

  proj[0].mval = {UNPACK2(inter->init_mval)};
  proj[1].mval = {float(event->mval[0]), float(event->mval[1])};

  float arrow_co[3];
  float arrow_no[3];
  copy_v3_v3(arrow_co, inter->init_matrix_basis[3]);
  normalize_v3_v3(arrow_no, arrow->gizmo.matrix_basis[2]);

  int ok = 0;

  for (int j = 0; j < 2; j++) {
    ED_view3d_win_to_ray(region, proj[j].mval, proj[j].ray_origin, proj[j].ray_direction);
    /* Force Y axis if we're view aligned */
    if (j == 0) {
      if (RAD2DEGF(acosf(dot_v3v3(proj[j].ray_direction, arrow->gizmo.matrix_basis[2]))) < 5.0f) {
        normalize_v3_v3(arrow_no, rv3d->viewinv[1]);
      }
    }

    float arrow_no_proj[3];
    project_plane_v3_v3v3(arrow_no_proj, arrow_no, proj[j].ray_direction);
    normalize_v3(arrow_no_proj);

    float lambda;
    if (isect_ray_plane_v3_factor(arrow_co, arrow_no, proj[j].ray_origin, arrow_no_proj, &lambda))
    {
      madd_v3_v3v3fl(proj[j].location, arrow_co, arrow_no, lambda);
      ok++;
    }
  }

  if (ok != 2) {
    return OPERATOR_RUNNING_MODAL;
  }

  sub_v3_v3v3(offset, proj[1].location, proj[0].location);
  facdir = dot_v3v3(arrow_no, offset) < 0.0f ? -1 : 1;

  GizmoCommonData *data = &arrow->data;
  const float ofs_new = facdir * len_v3(offset);

  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");

  /* set the property for the operator and call its modal function */
  if (WM_gizmo_target_property_is_valid(gz_prop)) {
    const int transform_flag = RNA_enum_get(arrow->gizmo.ptr, "transform");
    const bool constrained = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED) != 0;
    const bool inverted = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_INVERTED) != 0;
    const bool use_precision = (tweak_flag & WM_GIZMO_TWEAK_PRECISE) != 0;
    float value = gizmo_value_from_offset(
        data, inter, ofs_new, constrained, inverted, use_precision);

    WM_gizmo_target_property_float_set(C, gz, gz_prop, value);
    /* get clamped value */
    value = WM_gizmo_target_property_float_get(gz, gz_prop);

    data->offset = gizmo_offset_from_value(data, value, constrained, inverted);
  }
  else {
    data->offset = ofs_new;
  }

  /* tag the region for redraw */
  ED_region_tag_redraw_editor_overlays(region);

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_arrow_setup(wmGizmo *gz)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

  arrow->gizmo.flag |= WM_GIZMO_DRAW_MODAL;

  arrow->data.range_fac = 1.0f;
}

static wmOperatorStatus gizmo_arrow_invoke(bContext * /*C*/, wmGizmo *gz, const wmEvent *event)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  GizmoInteraction *inter = static_cast<GizmoInteraction *>(
      MEM_callocN(sizeof(ArrowGizmoInteraction), __func__));
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");

  /* Some gizmos don't use properties. */
  if (WM_gizmo_target_property_is_valid(gz_prop)) {
    inter->init_value = WM_gizmo_target_property_float_get(gz, gz_prop);
  }

  inter->init_offset = arrow->data.offset;

  inter->init_mval[0] = event->mval[0];
  inter->init_mval[1] = event->mval[1];

  gizmo_arrow_matrix_basis_get(gz, inter->init_matrix_basis);
  WM_gizmo_calc_matrix_final(gz, inter->init_matrix_final);

  ((ArrowGizmoInteraction *)inter)->init_arrow_length = RNA_float_get(gz->ptr, "length");

  gz->interaction_data = inter;

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_arrow_property_update(wmGizmo *gz, wmGizmoProperty *gz_prop)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  const int transform_flag = RNA_enum_get(arrow->gizmo.ptr, "transform");
  const bool constrained = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED) != 0;
  const bool inverted = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_INVERTED) != 0;
  gizmo_property_data_update(gz, &arrow->data, gz_prop, constrained, inverted);
}

static void gizmo_arrow_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  GizmoCommonData *data = &arrow->data;
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
  const bool is_prop_valid = WM_gizmo_target_property_is_valid(gz_prop);

  if (cancel) {
    GizmoInteraction *inter = static_cast<GizmoInteraction *>(gz->interaction_data);
    if (is_prop_valid) {
      gizmo_property_value_reset(C, gz, inter, gz_prop);
    }
    data->offset = inter->init_offset;
  }
  else {
    /* Assign in case applying the operation needs an updated offset
     * edit-mesh bisect needs this. */
    if (is_prop_valid) {
      const int transform_flag = RNA_enum_get(arrow->gizmo.ptr, "transform");
      const bool constrained = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED) != 0;
      const bool inverted = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_INVERTED) != 0;
      const float value = WM_gizmo_target_property_float_get(gz, gz_prop);
      data->offset = gizmo_offset_from_value(data, value, constrained, inverted);
    }
  }

  if (!cancel) {
    if (is_prop_valid) {
      WM_gizmo_target_property_anim_autokey(C, gz, gz_prop);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Arrow Gizmo API
 * \{ */

void ED_gizmo_arrow3d_set_ui_range(wmGizmo *gz, const float min, const float max)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

  BLI_assert(min < max);
  BLI_assert_msg(!WM_gizmo_target_property_is_valid(WM_gizmo_target_property_find(gz, "offset")),
                 "Make sure this function is called before WM_gizmo_target_property_def_rna");

  arrow->data.range = max - min;
  arrow->data.min = min;
  arrow->data.max = max;
  arrow->data.is_custom_range_set = true;
}

void ED_gizmo_arrow3d_set_range_fac(wmGizmo *gz, const float range_fac)
{
  ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
  BLI_assert_msg(!WM_gizmo_target_property_is_valid(WM_gizmo_target_property_find(gz, "offset")),
                 "Make sure this function is called before WM_gizmo_target_property_def_rna");

  arrow->data.range_fac = range_fac;
}

static void GIZMO_GT_arrow_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_arrow_3d";

  /* API callbacks. */
  gzt->draw = gizmo_arrow_draw;
  gzt->draw_select = gizmo_arrow_draw_select;
  gzt->test_select = gizmo_arrow_test_select;
  gzt->matrix_basis_get = gizmo_arrow_matrix_basis_get;
  gzt->modal = gizmo_arrow_modal;
  gzt->setup = gizmo_arrow_setup;
  gzt->invoke = gizmo_arrow_invoke;
  gzt->property_update = gizmo_arrow_property_update;
  gzt->exit = gizmo_arrow_exit;

  gzt->struct_size = sizeof(ArrowGizmo3D);

  /* rna */
  static const EnumPropertyItem rna_enum_draw_style_items[] = {
      {ED_GIZMO_ARROW_STYLE_NORMAL, "NORMAL", 0, "Normal", ""},
      {ED_GIZMO_ARROW_STYLE_CROSS, "CROSS", 0, "Cross", ""},
      {ED_GIZMO_ARROW_STYLE_BOX, "BOX", 0, "Box", ""},
      {ED_GIZMO_ARROW_STYLE_CONE, "CONE", 0, "Cone", ""},
      {ED_GIZMO_ARROW_STYLE_PLANE, "PLANE", 0, "Plane", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem rna_enum_draw_options_items[] = {
      {ED_GIZMO_ARROW_DRAW_FLAG_STEM, "STEM", 0, "Stem", ""},
      {ED_GIZMO_ARROW_DRAW_FLAG_ORIGIN, "ORIGIN", 0, "Origin", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem rna_enum_transform_items[] = {
      {ED_GIZMO_ARROW_XFORM_FLAG_INVERTED, "INVERT", 0, "Inverted", ""},
      {ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED, "CONSTRAIN", 0, "Constrained", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(gzt->srna,
               "draw_style",
               rna_enum_draw_style_items,
               ED_GIZMO_ARROW_STYLE_NORMAL,
               "Draw Style",
               "");
  RNA_def_enum_flag(gzt->srna,
                    "draw_options",
                    rna_enum_draw_options_items,
                    ED_GIZMO_ARROW_DRAW_FLAG_STEM,
                    "Draw Options",
                    "");
  RNA_def_enum_flag(gzt->srna, "transform", rna_enum_transform_items, 0, "Transform", "");

  RNA_def_float(
      gzt->srna, "length", 1.0f, -FLT_MAX, FLT_MAX, "Arrow Line Length", "", -FLT_MAX, FLT_MAX);
  RNA_def_float_vector(
      gzt->srna, "aspect", 2, nullptr, 0, FLT_MAX, "Aspect", "Cone/box style only", 0.0f, FLT_MAX);

  WM_gizmotype_target_property_def(gzt, "offset", PROP_FLOAT, 1);
}

void ED_gizmotypes_arrow_3d()
{
  WM_gizmotype_append(GIZMO_GT_arrow_3d);
}

/** \} */
