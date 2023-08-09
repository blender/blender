/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Primitive Gizmo
 *
 * 3D Gizmo
 *
 * \brief Gizmo with primitive drawing type (plane, cube, etc.).
 * Currently only plane primitive supported without own handling, use with operator only.
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gizmo_library.hh"

/* own includes */
#include "../gizmo_library_intern.h"

static float verts_plane[4][3] = {
    {-1, -1, 0},
    {1, -1, 0},
    {1, 1, 0},
    {-1, 1, 0},
};

struct PrimitiveGizmo3D {
  wmGizmo gizmo;

  int draw_style;
  float arc_inner_factor;
  bool draw_inner;
};

/* -------------------------------------------------------------------- */
/** \name RNA callbacks */

static PrimitiveGizmo3D *gizmo_primitive_rna_find_operator(PointerRNA *ptr)
{
  return (PrimitiveGizmo3D *)gizmo_find_from_properties(
      static_cast<const IDProperty *>(ptr->data), SPACE_TYPE_ANY, RGN_TYPE_ANY);
}

static int gizmo_primitive_rna__draw_style_get_fn(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  return gz_prim->draw_style;
}

static void gizmo_primitive_rna__draw_style_set_fn(PointerRNA *ptr,
                                                   PropertyRNA * /*prop*/,
                                                   int value)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  gz_prim->draw_style = value;
}

static float gizmo_primitive_rna__arc_inner_factor_get_fn(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  return gz_prim->arc_inner_factor;
}

static void gizmo_primitive_rna__arc_inner_factor_set_fn(PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         float value)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  gz_prim->arc_inner_factor = value;
}

static bool gizmo_primitive_rna__draw_inner_get_fn(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  return gz_prim->draw_inner;
}

static void gizmo_primitive_rna__draw_inner_set_fn(PointerRNA *ptr,
                                                   PropertyRNA * /*prop*/,
                                                   bool value)
{
  PrimitiveGizmo3D *gz_prim = gizmo_primitive_rna_find_operator(ptr);
  gz_prim->draw_inner = value;
}

/* -------------------------------------------------------------------- */

static void gizmo_primitive_draw_geom(PrimitiveGizmo3D *gz_prim,
                                      const float col_inner[4],
                                      const float col_outer[4],
                                      const int nsegments,
                                      const bool draw_inner)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const bool use_polyline_shader = gz_prim->gizmo.line_width > 1.0f;

  if (draw_inner || !use_polyline_shader) {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  }

  if (draw_inner) {
    if (gz_prim->draw_style == ED_GIZMO_PRIMITIVE_STYLE_PLANE) {
      wm_gizmo_vec_draw(col_inner, verts_plane, ARRAY_SIZE(verts_plane), pos, GPU_PRIM_TRI_FAN);
    }
    else {
      immUniformColor4fv(col_inner);
      if (gz_prim->draw_style == ED_GIZMO_PRIMITIVE_STYLE_CIRCLE) {
        imm_draw_circle_fill_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
      }
      else {
        BLI_assert(gz_prim->draw_style == ED_GIZMO_PRIMITIVE_STYLE_ANNULUS);
        imm_draw_disk_partial_fill_3d(
            pos, 0.0f, 0.0f, 0.0f, gz_prim->arc_inner_factor, 1.0f, nsegments, 0.0f, 360.0f);
      }
    }
  }

  /* Draw outline. */

  if (use_polyline_shader) {
    if (draw_inner) {
      immUnbindProgram();
    }
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

    float viewport[4];
    GPU_viewport_size_get_f(viewport);
    immUniform2fv("viewportSize", &viewport[2]);
    immUniform1f("lineWidth", gz_prim->gizmo.line_width * U.pixelsize);
  }

  if (gz_prim->draw_style == ED_GIZMO_PRIMITIVE_STYLE_PLANE) {
    wm_gizmo_vec_draw(col_outer, verts_plane, ARRAY_SIZE(verts_plane), pos, GPU_PRIM_LINE_LOOP);
  }
  else {
    immUniformColor4fv(col_outer);
    if (gz_prim->draw_style == ED_GIZMO_PRIMITIVE_STYLE_CIRCLE) {
      imm_draw_circle_wire_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
    }
    else {
      imm_draw_circle_wire_3d(pos, 0.0f, 0.0f, gz_prim->arc_inner_factor, nsegments);
      imm_draw_circle_wire_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
    }
  }
  immUnbindProgram();
}

static void gizmo_primitive_draw_intern(wmGizmo *gz, const bool select, const bool highlight)
{
  PrimitiveGizmo3D *gz_prim = (PrimitiveGizmo3D *)gz;

  float color_inner[4], color_outer[4];
  float matrix_final[4][4];

  gizmo_color_get(gz, highlight, color_outer);
  copy_v4_v4(color_inner, color_outer);
  color_inner[3] *= 0.5f;

  WM_gizmo_calc_matrix_final(gz, matrix_final);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);

  gizmo_primitive_draw_geom(gz_prim,
                            color_inner,
                            color_outer,
                            select ? 24 : DIAL_RESOLUTION,
                            gz_prim->draw_inner || select);

  GPU_matrix_pop();

  if (gz->interaction_data) {
    GizmoInteraction *inter = static_cast<GizmoInteraction *>(gz->interaction_data);

    copy_v4_fl(color_inner, 0.5f);
    copy_v3_fl(color_outer, 0.5f);
    color_outer[3] = 0.8f;

    GPU_matrix_push();
    GPU_matrix_mul(inter->init_matrix_final);

    gizmo_primitive_draw_geom(
        gz_prim, color_inner, color_outer, DIAL_RESOLUTION, gz_prim->draw_inner);

    GPU_matrix_pop();
  }
  GPU_blend(GPU_BLEND_NONE);
}

static void gizmo_primitive_draw_select(const bContext * /*C*/, wmGizmo *gz, int select_id)
{
  GPU_select_load_id(select_id);
  gizmo_primitive_draw_intern(gz, true, false);
}

static void gizmo_primitive_draw(const bContext * /*C*/, wmGizmo *gz)
{
  gizmo_primitive_draw_intern(gz, false, (gz->state & WM_GIZMO_STATE_HIGHLIGHT));
}

static void gizmo_primitive_setup(wmGizmo *gz)
{
  gz->flag |= WM_GIZMO_DRAW_MODAL;

  /* Default Values. */
  PrimitiveGizmo3D *gz_prim = (PrimitiveGizmo3D *)gz;
  gz_prim->draw_style = ED_GIZMO_PRIMITIVE_STYLE_PLANE;
  gz_prim->arc_inner_factor = true;
  gz_prim->draw_inner = true;
}

static int gizmo_primitive_invoke(bContext * /*C*/, wmGizmo *gz, const wmEvent * /*event*/)
{
  GizmoInteraction *inter = static_cast<GizmoInteraction *>(
      MEM_callocN(sizeof(GizmoInteraction), __func__));

  WM_gizmo_calc_matrix_final(gz, inter->init_matrix_final);

  gz->interaction_data = inter;

  return OPERATOR_RUNNING_MODAL;
}

/* -------------------------------------------------------------------- */
/** \name Primitive Gizmo API
 * \{ */

static void GIZMO_GT_primitive_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_primitive_3d";

  /* api callbacks */
  gzt->draw = gizmo_primitive_draw;
  gzt->draw_select = gizmo_primitive_draw_select;
  gzt->setup = gizmo_primitive_setup;
  gzt->invoke = gizmo_primitive_invoke;

  gzt->struct_size = sizeof(PrimitiveGizmo3D);

  static EnumPropertyItem rna_enum_draw_style[] = {
      {ED_GIZMO_PRIMITIVE_STYLE_PLANE, "PLANE", 0, "Plane", ""},
      {ED_GIZMO_PRIMITIVE_STYLE_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {ED_GIZMO_PRIMITIVE_STYLE_ANNULUS, "ANNULUS", 0, "Annulus", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;
  prop = RNA_def_enum(gzt->srna,
                      "draw_style",
                      rna_enum_draw_style,
                      ED_GIZMO_PRIMITIVE_STYLE_PLANE,
                      "Draw Style",
                      "");
  RNA_def_property_enum_funcs_runtime(prop,
                                      gizmo_primitive_rna__draw_style_get_fn,
                                      gizmo_primitive_rna__draw_style_set_fn,
                                      nullptr);

  prop = RNA_def_float_factor(
      gzt->srna, "arc_inner_factor", 0.0f, 0.0f, FLT_MAX, "Arc Inner Factor", "", 0.0f, 1.0f);
  RNA_def_property_float_funcs_runtime(prop,
                                       gizmo_primitive_rna__arc_inner_factor_get_fn,
                                       gizmo_primitive_rna__arc_inner_factor_set_fn,
                                       nullptr);

  prop = RNA_def_boolean(gzt->srna, "draw_inner", true, "Draw Inner", "");
  RNA_def_property_boolean_funcs_runtime(
      prop, gizmo_primitive_rna__draw_inner_get_fn, gizmo_primitive_rna__draw_inner_set_fn);
}

void ED_gizmotypes_primitive_3d()
{
  WM_gizmotype_append(GIZMO_GT_primitive_3d);
}

/** \} */
