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
 * The Original Code is Copyright (C) 2009, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "BLI_sys_types.h"

#include "BKE_context.h"
#include "BKE_brush.h"
#include "BKE_gpencil.h"
#include "BKE_paint.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_toolsystem.h"

#include "RNA_access.h"

#include "ED_gpencil.h"
#include "ED_select_utils.h"
#include "ED_object.h"
#include "ED_transform.h"

#include "gpencil_intern.h"

/* ****************************************** */
/* Grease Pencil Keymaps */

/* Generic Drawing Keymap - Annotations */
static void ed_keymap_gpencil_general(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Grease Pencil", 0, 0);
}

/* ==================== */

/* Poll callback for stroke editing mode */
static bool gp_stroke_editmode_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  return (gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE));
}

/* Poll callback for stroke painting mode */
static bool gp_stroke_paintmode_poll(bContext *C)
{
  /* TODO: limit this to mode, but review 2D editors */
  bGPdata *gpd = CTX_data_gpencil_data(C);
  return (gpd && (gpd->flag & GP_DATA_STROKE_PAINTMODE));
}

static bool gp_stroke_paintmode_poll_with_tool(bContext *C, const char gpencil_tool)
{
  /* TODO: limit this to mode, but review 2D editors */
  bGPdata *gpd = CTX_data_gpencil_data(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  return ((gpd) && (gpd->flag & GP_DATA_STROKE_PAINTMODE) && (brush && brush->gpencil_settings) &&
          WM_toolsystem_active_tool_is_brush(C) && (brush->gpencil_tool == gpencil_tool));
}

/* Poll callback for stroke painting (draw brush) */
static bool gp_stroke_paintmode_draw_poll(bContext *C)
{
  return gp_stroke_paintmode_poll_with_tool(C, GPAINT_TOOL_DRAW);
}

/* Poll callback for stroke painting (erase brush) */
static bool gp_stroke_paintmode_erase_poll(bContext *C)
{
  return gp_stroke_paintmode_poll_with_tool(C, GPAINT_TOOL_ERASE);
}

/* Poll callback for stroke painting (fill) */
static bool gp_stroke_paintmode_fill_poll(bContext *C)
{
  return gp_stroke_paintmode_poll_with_tool(C, GPAINT_TOOL_FILL);
}

/* Poll callback for stroke sculpting mode */
static bool gp_stroke_sculptmode_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  Object *ob = CTX_data_active_object(C);
  ScrArea *sa = CTX_wm_area(C);

  /* if not gpencil object and not view3d, need sculpt keys if edit mode */
  if (sa->spacetype != SPACE_VIEW3D) {
    return ((gpd) && (gpd->flag & GP_DATA_STROKE_EDITMODE));
  }
  else {
    /* weight paint is a submode of sculpt */
    if ((ob) && (ob->type == OB_GPENCIL)) {
      return GPENCIL_SCULPT_OR_WEIGHT_MODE(gpd);
    }
  }

  return 0;
}

/* Poll callback for stroke weight paint mode */
static bool gp_stroke_weightmode_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  Object *ob = CTX_data_active_object(C);

  if ((ob) && (ob->type == OB_GPENCIL)) {
    return (gpd && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE));
  }

  return 0;
}

/* Stroke Editing Keymap - Only when editmode is enabled */
static void ed_keymap_gpencil_editing(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Edit Mode", 0, 0);

  /* set poll callback - so that this keymap only gets enabled when stroke editmode is enabled */
  keymap->poll = gp_stroke_editmode_poll;
}

/* keys for draw with a drawing brush (no fill) */
static void ed_keymap_gpencil_painting_draw(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Paint (Draw brush)", 0, 0);
  keymap->poll = gp_stroke_paintmode_draw_poll;
}

/* keys for draw with a eraser brush (erase) */
static void ed_keymap_gpencil_painting_erase(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Paint (Erase)", 0, 0);
  keymap->poll = gp_stroke_paintmode_erase_poll;
}

/* keys for draw with a fill brush */
static void ed_keymap_gpencil_painting_fill(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Paint (Fill)", 0, 0);
  keymap->poll = gp_stroke_paintmode_fill_poll;
}

/* Stroke Painting Keymap - Only when paintmode is enabled */
static void ed_keymap_gpencil_painting(wmKeyConfig *keyconf)
{
  /* set poll callback - so that this keymap only gets enabled when stroke paintmode is enabled */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Paint Mode", 0, 0);
  keymap->poll = gp_stroke_paintmode_poll;
}

/* Stroke Sculpting Keymap - Only when sculptmode is enabled */
static void ed_keymap_gpencil_sculpting(wmKeyConfig *keyconf)
{
  /* set poll callback - so that this keymap only gets enabled when stroke sculptmode is enabled */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Sculpt Mode", 0, 0);
  keymap->poll = gp_stroke_sculptmode_poll;
}

/* Stroke Weight Paint Keymap - Only when weight is enabled */
static void ed_keymap_gpencil_weightpainting(wmKeyConfig *keyconf)
{
  /* set poll callback - so that this keymap only gets enabled when stroke sculptmode is enabled */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Grease Pencil Stroke Weight Mode", 0, 0);
  keymap->poll = gp_stroke_weightmode_poll;
}
/* ==================== */

void ED_keymap_gpencil(wmKeyConfig *keyconf)
{
  ed_keymap_gpencil_general(keyconf);
  ed_keymap_gpencil_editing(keyconf);
  ed_keymap_gpencil_painting(keyconf);
  ed_keymap_gpencil_painting_draw(keyconf);
  ed_keymap_gpencil_painting_erase(keyconf);
  ed_keymap_gpencil_painting_fill(keyconf);
  ed_keymap_gpencil_sculpting(keyconf);
  ed_keymap_gpencil_weightpainting(keyconf);
}

/* ****************************************** */

void ED_operatortypes_gpencil(void)
{
  /* Annotations -------------------- */

  WM_operatortype_append(GPENCIL_OT_annotate);

  /* Drawing ----------------------- */

  WM_operatortype_append(GPENCIL_OT_draw);
  WM_operatortype_append(GPENCIL_OT_fill);

  /* Guides ----------------------- */

  WM_operatortype_append(GPENCIL_OT_guide_rotate);

  /* Editing (Strokes) ------------ */

  WM_operatortype_append(GPENCIL_OT_editmode_toggle);
  WM_operatortype_append(GPENCIL_OT_selectmode_toggle);
  WM_operatortype_append(GPENCIL_OT_paintmode_toggle);
  WM_operatortype_append(GPENCIL_OT_sculptmode_toggle);
  WM_operatortype_append(GPENCIL_OT_weightmode_toggle);
  WM_operatortype_append(GPENCIL_OT_selection_opacity_toggle);

  WM_operatortype_append(GPENCIL_OT_select);
  WM_operatortype_append(GPENCIL_OT_select_all);
  WM_operatortype_append(GPENCIL_OT_select_circle);
  WM_operatortype_append(GPENCIL_OT_select_box);
  WM_operatortype_append(GPENCIL_OT_select_lasso);

  WM_operatortype_append(GPENCIL_OT_select_linked);
  WM_operatortype_append(GPENCIL_OT_select_grouped);
  WM_operatortype_append(GPENCIL_OT_select_more);
  WM_operatortype_append(GPENCIL_OT_select_less);
  WM_operatortype_append(GPENCIL_OT_select_first);
  WM_operatortype_append(GPENCIL_OT_select_last);
  WM_operatortype_append(GPENCIL_OT_select_alternate);

  WM_operatortype_append(GPENCIL_OT_duplicate);
  WM_operatortype_append(GPENCIL_OT_delete);
  WM_operatortype_append(GPENCIL_OT_dissolve);
  WM_operatortype_append(GPENCIL_OT_copy);
  WM_operatortype_append(GPENCIL_OT_paste);
  WM_operatortype_append(GPENCIL_OT_extrude);

  WM_operatortype_append(GPENCIL_OT_move_to_layer);
  WM_operatortype_append(GPENCIL_OT_layer_change);

  WM_operatortype_append(GPENCIL_OT_snap_to_grid);
  WM_operatortype_append(GPENCIL_OT_snap_to_cursor);
  WM_operatortype_append(GPENCIL_OT_snap_cursor_to_selected);

  WM_operatortype_append(GPENCIL_OT_reproject);

  WM_operatortype_append(GPENCIL_OT_sculpt_paint);

  /* Editing (Buttons) ------------ */

  WM_operatortype_append(GPENCIL_OT_data_add);
  WM_operatortype_append(GPENCIL_OT_data_unlink);

  WM_operatortype_append(GPENCIL_OT_layer_add);
  WM_operatortype_append(GPENCIL_OT_layer_remove);
  WM_operatortype_append(GPENCIL_OT_layer_move);
  WM_operatortype_append(GPENCIL_OT_layer_duplicate);
  WM_operatortype_append(GPENCIL_OT_layer_duplicate_object);

  WM_operatortype_append(GPENCIL_OT_hide);
  WM_operatortype_append(GPENCIL_OT_reveal);
  WM_operatortype_append(GPENCIL_OT_lock_all);
  WM_operatortype_append(GPENCIL_OT_unlock_all);
  WM_operatortype_append(GPENCIL_OT_layer_isolate);
  WM_operatortype_append(GPENCIL_OT_layer_merge);

  WM_operatortype_append(GPENCIL_OT_blank_frame_add);

  WM_operatortype_append(GPENCIL_OT_active_frame_delete);
  WM_operatortype_append(GPENCIL_OT_active_frames_delete_all);
  WM_operatortype_append(GPENCIL_OT_frame_duplicate);
  WM_operatortype_append(GPENCIL_OT_frame_clean_fill);
  WM_operatortype_append(GPENCIL_OT_frame_clean_loose);

  WM_operatortype_append(GPENCIL_OT_convert);

  WM_operatortype_append(GPENCIL_OT_stroke_arrange);
  WM_operatortype_append(GPENCIL_OT_stroke_change_color);
  WM_operatortype_append(GPENCIL_OT_stroke_lock_color);
  WM_operatortype_append(GPENCIL_OT_stroke_apply_thickness);
  WM_operatortype_append(GPENCIL_OT_stroke_cyclical_set);
  WM_operatortype_append(GPENCIL_OT_stroke_caps_set);
  WM_operatortype_append(GPENCIL_OT_stroke_join);
  WM_operatortype_append(GPENCIL_OT_stroke_flip);
  WM_operatortype_append(GPENCIL_OT_stroke_subdivide);
  WM_operatortype_append(GPENCIL_OT_stroke_simplify);
  WM_operatortype_append(GPENCIL_OT_stroke_simplify_fixed);
  WM_operatortype_append(GPENCIL_OT_stroke_separate);
  WM_operatortype_append(GPENCIL_OT_stroke_split);
  WM_operatortype_append(GPENCIL_OT_stroke_smooth);
  WM_operatortype_append(GPENCIL_OT_stroke_merge);
  WM_operatortype_append(GPENCIL_OT_stroke_cutter);
  WM_operatortype_append(GPENCIL_OT_stroke_trim);

  WM_operatortype_append(GPENCIL_OT_brush_presets_create);

  /* vertex groups */
  WM_operatortype_append(GPENCIL_OT_vertex_group_assign);
  WM_operatortype_append(GPENCIL_OT_vertex_group_remove_from);
  WM_operatortype_append(GPENCIL_OT_vertex_group_select);
  WM_operatortype_append(GPENCIL_OT_vertex_group_deselect);
  WM_operatortype_append(GPENCIL_OT_vertex_group_invert);
  WM_operatortype_append(GPENCIL_OT_vertex_group_smooth);
  WM_operatortype_append(GPENCIL_OT_vertex_group_normalize);
  WM_operatortype_append(GPENCIL_OT_vertex_group_normalize_all);

  /* color handle */
  WM_operatortype_append(GPENCIL_OT_lock_layer);
  WM_operatortype_append(GPENCIL_OT_color_isolate);
  WM_operatortype_append(GPENCIL_OT_color_hide);
  WM_operatortype_append(GPENCIL_OT_color_reveal);
  WM_operatortype_append(GPENCIL_OT_color_lock_all);
  WM_operatortype_append(GPENCIL_OT_color_unlock_all);
  WM_operatortype_append(GPENCIL_OT_color_select);

  /* Editing (Time) --------------- */

  /* Interpolation */
  WM_operatortype_append(GPENCIL_OT_interpolate);
  WM_operatortype_append(GPENCIL_OT_interpolate_sequence);
  WM_operatortype_append(GPENCIL_OT_interpolate_reverse);

  /* Primitives */
  WM_operatortype_append(GPENCIL_OT_primitive);

  /* convert old 2.7 files to 2.8 */
  WM_operatortype_append(GPENCIL_OT_convert_old_files);

  /* armatures */
  WM_operatortype_append(GPENCIL_OT_generate_weights);
}

void ED_operatormacros_gpencil(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  /* Duplicate + Move = Interactively place newly duplicated strokes */
  ot = WM_operatortype_append_macro(
      "GPENCIL_OT_duplicate_move",
      "Duplicate Strokes",
      "Make copies of the selected Grease Pencil strokes and move them",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GPENCIL_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "gpencil_strokes", true);
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  /* Extrude + Move = Interactively add new points */
  ot = WM_operatortype_append_macro("GPENCIL_OT_extrude_move",
                                    "Extrude Stroke Points",
                                    "Extrude selected points and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GPENCIL_OT_extrude");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "gpencil_strokes", true);
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

/* ****************************************** */
