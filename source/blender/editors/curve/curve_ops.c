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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edcurve
 */

#include <stdlib.h>
#include <math.h>

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_transform.h"

#include "curve_intern.h"

/************************* registration ****************************/

void ED_operatortypes_curve(void)
{
  WM_operatortype_append(FONT_OT_text_insert);
  WM_operatortype_append(FONT_OT_line_break);

  WM_operatortype_append(FONT_OT_case_toggle);
  WM_operatortype_append(FONT_OT_case_set);
  WM_operatortype_append(FONT_OT_style_toggle);
  WM_operatortype_append(FONT_OT_style_set);

  WM_operatortype_append(FONT_OT_select_all);

  WM_operatortype_append(FONT_OT_text_copy);
  WM_operatortype_append(FONT_OT_text_cut);
  WM_operatortype_append(FONT_OT_text_paste);
  WM_operatortype_append(FONT_OT_text_paste_from_file);

  WM_operatortype_append(FONT_OT_move);
  WM_operatortype_append(FONT_OT_move_select);
  WM_operatortype_append(FONT_OT_delete);

  WM_operatortype_append(FONT_OT_change_character);
  WM_operatortype_append(FONT_OT_change_spacing);

  WM_operatortype_append(FONT_OT_open);
  WM_operatortype_append(FONT_OT_unlink);

  WM_operatortype_append(FONT_OT_textbox_add);
  WM_operatortype_append(FONT_OT_textbox_remove);

  WM_operatortype_append(CURVE_OT_hide);
  WM_operatortype_append(CURVE_OT_reveal);

  WM_operatortype_append(CURVE_OT_separate);
  WM_operatortype_append(CURVE_OT_split);
  WM_operatortype_append(CURVE_OT_duplicate);
  WM_operatortype_append(CURVE_OT_delete);
  WM_operatortype_append(CURVE_OT_dissolve_verts);

  WM_operatortype_append(CURVE_OT_spline_type_set);
  WM_operatortype_append(CURVE_OT_radius_set);
  WM_operatortype_append(CURVE_OT_spline_weight_set);
  WM_operatortype_append(CURVE_OT_handle_type_set);
  WM_operatortype_append(CURVE_OT_normals_make_consistent);
  WM_operatortype_append(CURVE_OT_decimate);
  WM_operatortype_append(CURVE_OT_shade_smooth);
  WM_operatortype_append(CURVE_OT_shade_flat);
  WM_operatortype_append(CURVE_OT_tilt_clear);

  WM_operatortype_append(CURVE_OT_primitive_bezier_curve_add);
  WM_operatortype_append(CURVE_OT_primitive_bezier_circle_add);
  WM_operatortype_append(CURVE_OT_primitive_nurbs_curve_add);
  WM_operatortype_append(CURVE_OT_primitive_nurbs_circle_add);
  WM_operatortype_append(CURVE_OT_primitive_nurbs_path_add);

  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_curve_add);
  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_circle_add);
  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_surface_add);
  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_cylinder_add);
  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_sphere_add);
  WM_operatortype_append(SURFACE_OT_primitive_nurbs_surface_torus_add);

  WM_operatortype_append(CURVE_OT_smooth);
  WM_operatortype_append(CURVE_OT_smooth_weight);
  WM_operatortype_append(CURVE_OT_smooth_radius);
  WM_operatortype_append(CURVE_OT_smooth_tilt);

  WM_operatortype_append(CURVE_OT_de_select_first);
  WM_operatortype_append(CURVE_OT_de_select_last);
  WM_operatortype_append(CURVE_OT_select_all);
  WM_operatortype_append(CURVE_OT_select_linked);
  WM_operatortype_append(CURVE_OT_select_linked_pick);
  WM_operatortype_append(CURVE_OT_select_row);
  WM_operatortype_append(CURVE_OT_select_next);
  WM_operatortype_append(CURVE_OT_select_previous);
  WM_operatortype_append(CURVE_OT_select_more);
  WM_operatortype_append(CURVE_OT_select_less);
  WM_operatortype_append(CURVE_OT_select_random);
  WM_operatortype_append(CURVE_OT_select_nth);
  WM_operatortype_append(CURVE_OT_select_similar);
  WM_operatortype_append(CURVE_OT_shortest_path_pick);

  WM_operatortype_append(CURVE_OT_switch_direction);
  WM_operatortype_append(CURVE_OT_subdivide);
  WM_operatortype_append(CURVE_OT_make_segment);
  WM_operatortype_append(CURVE_OT_spin);
  WM_operatortype_append(CURVE_OT_vertex_add);
  WM_operatortype_append(CURVE_OT_draw);
  WM_operatortype_append(CURVE_OT_extrude);
  WM_operatortype_append(CURVE_OT_cyclic_toggle);

  WM_operatortype_append(CURVE_OT_match_texture_space);
}

void ED_operatormacros_curve(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("CURVE_OT_duplicate_move",
                                    "Add Duplicate",
                                    "Duplicate curve and move",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CURVE_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("CURVE_OT_extrude_move",
                                    "Extrude Curve and Move",
                                    "Extrude curve and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CURVE_OT_extrude");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void ED_keymap_curve(wmKeyConfig *keyconf)
{
  /* only set in editmode font, by space_view3d listener */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Font", 0, 0);
  keymap->poll = ED_operator_editfont;

  /* only set in editmode curve, by space_view3d listener */
  keymap = WM_keymap_ensure(keyconf, "Curve", 0, 0);
  keymap->poll = ED_operator_editsurfcurve;
}
