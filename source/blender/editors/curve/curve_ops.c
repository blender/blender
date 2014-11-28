/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/curve_ops.c
 *  \ingroup edcurve
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
#include "ED_transform.h"

#include "curve_intern.h"

/************************* registration ****************************/

void ED_operatortypes_curve(void)
{
	WM_operatortype_append(FONT_OT_text_insert);
	WM_operatortype_append(FONT_OT_line_break);
	WM_operatortype_append(FONT_OT_insert_lorem);

	WM_operatortype_append(FONT_OT_case_toggle);
	WM_operatortype_append(FONT_OT_case_set);
	WM_operatortype_append(FONT_OT_style_toggle);
	WM_operatortype_append(FONT_OT_style_set);

	WM_operatortype_append(FONT_OT_select_all);

	WM_operatortype_append(FONT_OT_text_copy);
	WM_operatortype_append(FONT_OT_text_cut);
	WM_operatortype_append(FONT_OT_text_paste);
	WM_operatortype_append(FONT_OT_text_paste_from_file);
	WM_operatortype_append(FONT_OT_text_paste_from_clipboard);

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

	WM_operatortype_append(CURVE_OT_spline_type_set);
	WM_operatortype_append(CURVE_OT_radius_set);
	WM_operatortype_append(CURVE_OT_spline_weight_set);
	WM_operatortype_append(CURVE_OT_handle_type_set);
	WM_operatortype_append(CURVE_OT_normals_make_consistent);
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

	WM_operatortype_append(CURVE_OT_switch_direction);
	WM_operatortype_append(CURVE_OT_subdivide);
	WM_operatortype_append(CURVE_OT_make_segment);
	WM_operatortype_append(CURVE_OT_spin);
	WM_operatortype_append(CURVE_OT_vertex_add);
	WM_operatortype_append(CURVE_OT_extrude);
	WM_operatortype_append(CURVE_OT_cyclic_toggle);

	WM_operatortype_append(CURVE_OT_match_texture_space);
}

void ED_operatormacros_curve(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;

	ot = WM_operatortype_append_macro("CURVE_OT_duplicate_move", "Add Duplicate", "Duplicate curve and move",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "CURVE_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("CURVE_OT_extrude_move", "Extrude Curve and Move",
	                                  "Extrude curve and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "CURVE_OT_extrude");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void ED_keymap_curve(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap = WM_keymap_find(keyconf, "Font", 0, 0);
	keymap->poll = ED_operator_editfont;
	
	/* only set in editmode font, by space_view3d listener */
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", BKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_CHINFO_BOLD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_CHINFO_ITALIC);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", UKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_CHINFO_UNDERLINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", PKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_CHINFO_SMALLCAPS);

	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", DELKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_NEXT_SEL);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_PREV_SEL);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", BACKSPACEKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", DEL_PREV_SEL); /* same as above [#26623] */
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", BACKSPACEKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_ALL);

	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", HOMEKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", ENDKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_END);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", LEFTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", RIGHTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", LEFTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", RIGHTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", UPARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", DOWNARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", PAGEUPKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move", PAGEDOWNKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_PAGE);

	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", HOMEKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", ENDKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", LINE_END);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", UPARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", DOWNARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", PAGEUPKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_PAGE);

	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_spacing", LEFTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_spacing", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_character", UPARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_character", DOWNARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", -1);

	WM_keymap_add_item(keymap, "FONT_OT_select_all", AKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "FONT_OT_text_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_cut", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_paste", VKEY, KM_PRESS, KM_CTRL, 0);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "FONT_OT_text_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_cut", XKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif

	WM_keymap_add_item(keymap, "FONT_OT_line_break", RETKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_insert", KM_TEXTINPUT, KM_ANY, KM_ANY, 0); // last!
	kmi = WM_keymap_add_item(keymap, "FONT_OT_text_insert", BACKSPACEKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "accent", true); /* accented characters */

	/* only set in editmode curve, by space_view3d listener */
	keymap = WM_keymap_find(keyconf, "Curve", 0, 0);
	keymap->poll = ED_operator_editsurfcurve;

	WM_keymap_add_menu(keymap, "INFO_MT_edit_curve_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "CURVE_OT_handle_type_set", VKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CURVE_OT_vertex_add", ACTIONMOUSE, KM_CLICK, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "CURVE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "CURVE_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "CURVE_OT_select_row", RKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "CURVE_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "CURVE_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	WM_keymap_add_item(keymap, "CURVE_OT_separate", PKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_split", YKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_extrude_move", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_make_segment", FKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_cyclic_toggle", CKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CURVE_OT_tilt_clear", TKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_tilt", TKEY, KM_PRESS, KM_CTRL, 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", TFM_CURVE_SHRINKFATTEN);

	WM_keymap_add_item(keymap, "CURVE_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "CURVE_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	kmi = WM_keymap_add_item(keymap, "CURVE_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);

	WM_keymap_add_item(keymap, "CURVE_OT_normals_make_consistent", NKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_vertex_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_curve_specials", WKEY, KM_PRESS, 0, 0);

	/* menus */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_hook", HKEY, KM_PRESS, KM_CTRL, 0);

	ED_keymap_proportional_cycle(keyconf, keymap);
	ED_keymap_proportional_editmode(keyconf, keymap, true);
}
