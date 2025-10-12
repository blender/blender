/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Button Gizmo
 *
 * 2D Gizmo, also works in 3D views.
 *
 * \brief Single click button action for use in gizmo groups.
 *
 * \note Currently only basic icon & vector-shape buttons are supported.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "DNA_userdef_types.h"

#include "BKE_context.hh"

#include "GPU_batch.hh"
#include "GPU_batch_utils.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_select.hh"
#include "GPU_state.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gizmo_library.hh"
#include "ED_view3d.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "interface_intern.hh"

/* own includes */
#include "../gizmo_library_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

struct ButtonGizmo2D {
  wmGizmo gizmo;
  bool is_init;
  /* Use an icon or shape */
  int icon;
  blender::gpu::Batch *shape_batch[2];
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal API
 * \{ */

static void button2d_geom_draw_backdrop(const wmGizmo *gz,
                                        const float color[4],
                                        const float fill_alpha,
                                        const bool select,
                                        const float screen_scale)
{
  float viewport[4];
  GPU_viewport_size_get_f(viewport);

  const float max_pixel_error = 0.25f;
  int nsegments = int(ceilf(M_PI / acosf(1.0f - max_pixel_error / screen_scale)));
  nsegments = max_ff(nsegments, 8);
  nsegments = min_ff(nsegments, 1000);

  GPUVertFormat *format = immVertexFormat();
  /* NOTE(Metal): Prefer 3D coordinate for 2D rendering when using 3D shader. */
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  /* TODO: other draw styles. */
  if (color[3] == 1.0 && fill_alpha == 1.0 && select == false) {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4fv(color);
    imm_draw_circle_fill_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
    immUniform2fv("viewportSize", &viewport[2]);
    immUniform1f("lineWidth", (gz->line_width * U.pixelsize) + WM_gizmo_select_bias(select));
    immUniformColor4fv(color);
    imm_draw_circle_wire_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
    immUnbindProgram();
  }
  else {
    /* Draw fill. */
    if ((fill_alpha != 0.0f) || (select == true)) {
      const float fill_color[4] = {UNPACK3(color), fill_alpha * color[3]};
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor4fv(fill_color);
      imm_draw_circle_fill_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
      immUnbindProgram();
    }

    /* Draw outline. */
    if ((fill_alpha != 1.0f) && (select == false)) {
      immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
      immUniform2fv("viewportSize", &viewport[2]);
      immUniform1f("lineWidth", (gz->line_width * U.pixelsize) + WM_gizmo_select_bias(select));
      immUniformColor4fv(color);
      imm_draw_circle_wire_3d(pos, 0.0f, 0.0f, 1.0f, nsegments);
      immUnbindProgram();
    }
  }

  UNUSED_VARS(select);
}

static void button2d_draw_intern(const bContext *C,
                                 wmGizmo *gz,
                                 const bool select,
                                 const bool highlight)
{
  ButtonGizmo2D *button = (ButtonGizmo2D *)gz;
  float viewport[4];
  GPU_viewport_size_get_f(viewport);

  const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
  if (button->is_init == false) {
    button->is_init = true;
    button->icon = -1;

    PropertyRNA *icon_prop = RNA_struct_find_property(gz->ptr, "icon");
    PropertyRNA *icon_value_prop = RNA_struct_find_property(gz->ptr, "icon_value");
    PropertyRNA *shape_prop = RNA_struct_find_property(gz->ptr, "shape");

    /* Same logic as in the RNA UI API, use icon_value only if icon is not defined. */
    if (RNA_property_is_set(gz->ptr, icon_prop)) {
      button->icon = RNA_property_enum_get(gz->ptr, icon_prop);
    }
    else if (RNA_property_is_set(gz->ptr, icon_value_prop)) {
      button->icon = RNA_property_int_get(gz->ptr, icon_value_prop);
      ui_icon_ensure_deferred(C, button->icon, false);
    }
    else if (RNA_property_is_set(gz->ptr, shape_prop)) {
      const uint polys_len = RNA_property_string_length(gz->ptr, shape_prop);
      if (LIKELY(polys_len > 0)) {
        char *polys = MEM_malloc_arrayN<char>(polys_len, __func__);
        RNA_property_string_get(gz->ptr, shape_prop, polys);
        /* Subtract 1 because this holds a null byte. */
        button->shape_batch[0] = GPU_batch_tris_from_poly_2d_encoded(
            (const uchar *)polys, polys_len - 1, nullptr);
        button->shape_batch[1] = GPU_batch_wire_from_poly_2d_encoded(
            (const uchar *)polys, polys_len - 1, nullptr);
        MEM_freeN(polys);
      }
    }
  }

  float color[4];
  float matrix_final[4][4];

  gizmo_color_get(gz, highlight, color);
  WM_gizmo_calc_matrix_final(gz, matrix_final);

  bool is_3d = (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) != 0;

  if ((select == false) && (draw_options & ED_GIZMO_BUTTON_SHOW_HELPLINE)) {
    float matrix_final_no_offset[4][4];
    WM_gizmo_calc_matrix_final_no_offset(gz, matrix_final_no_offset);
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
    immUniform2fv("viewportSize", &viewport[2]);
    immUniform1f("lineWidth", (gz->line_width * U.pixelsize) + WM_gizmo_select_bias(select));
    immUniformColor4fv(color);
    immBegin(GPU_PRIM_LINE_STRIP, 2);
    immVertex3fv(pos, matrix_final[3]);
    immVertex3fv(pos, matrix_final_no_offset[3]);
    immEnd();
    immUnbindProgram();
  }

  bool need_to_pop = true;
  GPU_matrix_push();
  GPU_matrix_mul(matrix_final);

  float screen_scale = 200.0f;
  if (is_3d) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    float matrix_align[4][4];
    float matrix_final_unit[4][4];
    normalize_m4_m4(matrix_final_unit, matrix_final);
    mul_m4_m4m4(matrix_align, rv3d->viewmat, matrix_final_unit);
    zero_v3(matrix_align[3]);
    transpose_m4(matrix_align);
    GPU_matrix_mul(matrix_align);
  }
  else {
    screen_scale = mat4_to_scale(matrix_final);
  }

  if (select) {
    BLI_assert(is_3d);
    button2d_geom_draw_backdrop(gz, color, 1.0, select, screen_scale);
  }
  else {

    GPU_blend(GPU_BLEND_ALPHA);

    if (draw_options & ED_GIZMO_BUTTON_SHOW_BACKDROP) {
      const float fill_alpha = RNA_float_get(gz->ptr, "backdrop_fill_alpha");
      button2d_geom_draw_backdrop(gz, color, fill_alpha, select, screen_scale);
    }

    if (button->shape_batch[0] != nullptr) {
      GPU_line_smooth(true);
      GPU_polygon_smooth(false);
      for (uint i = 0; i < ARRAY_SIZE(button->shape_batch) && button->shape_batch[i]; i++) {
        const bool do_wires = (i == 1);
        if (do_wires) {
          GPU_batch_program_set_builtin(button->shape_batch[i],
                                        GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
          GPU_batch_uniform_2fv(button->shape_batch[i], "viewportSize", &viewport[2]);
          GPU_batch_uniform_1f(button->shape_batch[i], "lineWidth", gz->line_width * U.pixelsize);
        }
        else {
          GPU_batch_program_set_builtin(button->shape_batch[i], GPU_SHADER_3D_UNIFORM_COLOR);
        }

        /* Invert line color for wire. */
        if (draw_options & ED_GIZMO_BUTTON_SHOW_BACKDROP) {
          /* If we have a backdrop already,
           * draw a contrasting shape over it instead of drawing it the same color.
           * Use a low value instead of 50% so some darker primary colors
           * aren't considered being close to black. */
          float color_contrast[4];
          copy_v3_fl(color_contrast, srgb_to_grayscale(color) < 0.2f ? 1 : 0);
          color_contrast[3] = color[3];
          GPU_shader_uniform_4f(button->shape_batch[i]->shader, "color", UNPACK4(color_contrast));
        }
        else {
          GPU_shader_uniform_4f(button->shape_batch[i]->shader, "color", UNPACK4(color));
        }

        GPU_batch_draw(button->shape_batch[i]);

        if (draw_options & ED_GIZMO_BUTTON_SHOW_OUTLINE) {
          color[0] = 1.0f - color[0];
          color[1] = 1.0f - color[1];
          color[2] = 1.0f - color[2];
        }
      }
      GPU_line_smooth(false);
      GPU_polygon_smooth(true);
    }
    else if (button->icon != -1) {
      float pos[2];
      if (is_3d) {
        const float fac = 2.0f;
        GPU_matrix_translate_2f(-(fac / 2), -(fac / 2));
        GPU_matrix_scale_2f(fac / (ICON_DEFAULT_WIDTH * UI_SCALE_FAC),
                            fac / (ICON_DEFAULT_HEIGHT * UI_SCALE_FAC));
        pos[0] = 1.0f;
        pos[1] = 1.0f;
      }
      else {
        pos[0] = gz->matrix_basis[3][0] - (ICON_DEFAULT_WIDTH / 2.0) * UI_SCALE_FAC;
        pos[1] = gz->matrix_basis[3][1] - (ICON_DEFAULT_HEIGHT / 2.0) * UI_SCALE_FAC;
        GPU_matrix_pop();
        need_to_pop = false;
      }

      float alpha = (highlight) ? 1.0f : 0.8f;
      GPU_polygon_smooth(false);
      UI_icon_draw_alpha(pos[0], pos[1], button->icon, alpha);
      GPU_polygon_smooth(true);
    }
    GPU_blend(GPU_BLEND_NONE);
  }

  if (need_to_pop) {
    GPU_matrix_pop();
  }
}

static void gizmo_button2d_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
  GPU_select_load_id(select_id);
  button2d_draw_intern(C, gz, true, false);
}

static void gizmo_button2d_draw(const bContext *C, wmGizmo *gz)
{
  const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;

  GPU_blend(GPU_BLEND_ALPHA);
  button2d_draw_intern(C, gz, false, is_highlight);
  GPU_blend(GPU_BLEND_NONE);
}

static int gizmo_button2d_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  float point_local[2];

  if (false) {
    /* correct, but unnecessarily slow. */
    if (gizmo_window_project_2d(
            C, gz, blender::float2{blender::int2(mval)}, 2, true, point_local) == false)
    {
      return -1;
    }
  }
  else {
    copy_v2_v2(point_local, blender::float2{blender::int2(mval)});
    sub_v2_v2(point_local, gz->matrix_basis[3]);
    mul_v2_fl(point_local, 1.0f / gz->scale_final);
  }
  /* The 'gz->scale_final' is already applied when projecting. */
  if (len_squared_v2(point_local) < 1.0f) {
    return 0;
  }

  return -1;
}

static int gizmo_button2d_cursor_get(wmGizmo *gz)
{
  if (RNA_boolean_get(gz->ptr, "show_drag")) {
    return WM_CURSOR_NSEW_SCROLL;
  }
  return WM_CURSOR_DEFAULT;
}

#define CIRCLE_RESOLUTION_3D 32
static bool gizmo_button2d_bounds(bContext *C, wmGizmo *gz, rcti *r_bounding_box)
{
  ScrArea *area = CTX_wm_area(C);
  float rad = CIRCLE_RESOLUTION_3D * UI_SCALE_FAC / 2.0f;
  const float *co = nullptr;
  float matrix_final[4][4];
  float co_proj[3];
  WM_gizmo_calc_matrix_final(gz, matrix_final);

  if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
    ARegion *region = CTX_wm_region(C);
    if (ED_view3d_project_float_global(region, matrix_final[3], co_proj, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK)
    {
      float matrix_final_no_offset[4][4];
      const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);
      WM_gizmo_calc_matrix_final_no_offset(gz, matrix_final_no_offset);
      const float factor = ED_view3d_pixel_size_no_ui_scale(rv3d, matrix_final_no_offset[3]) /
                           ED_view3d_pixel_size_no_ui_scale(rv3d, matrix_final[3]);
      /* It's possible (although unlikely) `matrix_final_no_offset` is behind the view.
       * `matrix_final` has already been projected so both can't be negative. */
      if (factor > 0.0f) {
        rad *= factor;
      }
      co = co_proj;
    }
  }
  else {
    rad = mat4_to_scale(matrix_final);
    co = matrix_final[3];
  }

  if (co != nullptr) {
    r_bounding_box->xmin = co[0] + area->totrct.xmin - rad;
    r_bounding_box->ymin = co[1] + area->totrct.ymin - rad;
    r_bounding_box->xmax = r_bounding_box->xmin + rad;
    r_bounding_box->ymax = r_bounding_box->ymin + rad;
    return true;
  }
  return false;
}

static void gizmo_button2d_free(wmGizmo *gz)
{
  ButtonGizmo2D *shape = (ButtonGizmo2D *)gz;

  for (uint i = 0; i < ARRAY_SIZE(shape->shape_batch); i++) {
    GPU_BATCH_DISCARD_SAFE(shape->shape_batch[i]);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Gizmo API
 * \{ */

static void GIZMO_GT_button_2d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_button_2d";

  /* API callbacks. */
  gzt->draw = gizmo_button2d_draw;
  gzt->draw_select = gizmo_button2d_draw_select;
  gzt->test_select = gizmo_button2d_test_select;
  gzt->cursor_get = gizmo_button2d_cursor_get;
  gzt->screen_bounds_get = gizmo_button2d_bounds;
  gzt->free = gizmo_button2d_free;

  gzt->struct_size = sizeof(ButtonGizmo2D);

  /* rna */
  static const EnumPropertyItem rna_enum_draw_options[] = {
      {ED_GIZMO_BUTTON_SHOW_OUTLINE, "OUTLINE", 0, "Outline", ""},
      {ED_GIZMO_BUTTON_SHOW_BACKDROP, "BACKDROP", 0, "Backdrop", ""},
      {ED_GIZMO_BUTTON_SHOW_HELPLINE, "HELPLINE", 0, "Help Line", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop;

  RNA_def_enum_flag(gzt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

  prop = RNA_def_property(gzt->srna, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_icon_items);

  RNA_def_property(gzt->srna, "icon_value", PROP_INT, PROP_UNSIGNED);

  /* Passed to 'GPU_batch_tris_from_poly_2d_encoded' */
  RNA_def_property(gzt->srna, "shape", PROP_STRING, PROP_BYTESTRING);

  /* Currently only used for cursor display. */
  RNA_def_boolean(gzt->srna, "show_drag", true, "Show Drag", "");

  RNA_def_float(gzt->srna,
                "backdrop_fill_alpha",
                1.0f,
                0.0f,
                1.0,
                "When below 1.0, draw the interior with a reduced alpha compared to the outline",
                "",
                0.0f,
                1.0f);
}

void ED_gizmotypes_button_2d()
{
  WM_gizmotype_append(GIZMO_GT_button_2d);
}

/** \} */ /* Button Gizmo API */
