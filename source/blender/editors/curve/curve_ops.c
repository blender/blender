/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "UI_interface.h"

#include "curve_intern.h"


/**************************** menus *****************************/

static int specials_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Specials", 0);
	layout= uiPupMenuLayout(pup);
	uiItemO(layout, NULL, 0, "CURVE_OT_subdivide");
	uiItemO(layout, NULL, 0, "CURVE_OT_switch_direction");
	uiItemO(layout, NULL, 0, "CURVE_OT_spline_weight_set");
	uiItemO(layout, NULL, 0, "CURVE_OT_radius_set");
	uiItemO(layout, NULL, 0, "CURVE_OT_smooth");
	uiItemO(layout, NULL, 0, "CURVE_OT_smooth_radius");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void CURVE_OT_specials_menu(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Specials Menu";
	ot->idname= "CURVE_OT_specials_menu";
	
	/* api clastbacks */
	ot->invoke= specials_menu_invoke;
	ot->poll= ED_operator_editsurfcurve;
}

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

	WM_operatortype_append(FONT_OT_text_copy);
	WM_operatortype_append(FONT_OT_text_cut);
	WM_operatortype_append(FONT_OT_text_paste);
	WM_operatortype_append(FONT_OT_file_paste);
	WM_operatortype_append(FONT_OT_buffer_paste);

	WM_operatortype_append(FONT_OT_move);
	WM_operatortype_append(FONT_OT_move_select);
	WM_operatortype_append(FONT_OT_delete);

	WM_operatortype_append(FONT_OT_change_character);
	WM_operatortype_append(FONT_OT_change_spacing);

	WM_operatortype_append(CURVE_OT_hide);
	WM_operatortype_append(CURVE_OT_reveal);

	WM_operatortype_append(CURVE_OT_separate);
	WM_operatortype_append(CURVE_OT_duplicate);
	WM_operatortype_append(CURVE_OT_delete);

	WM_operatortype_append(CURVE_OT_spline_type_set);
	WM_operatortype_append(CURVE_OT_radius_set);
	WM_operatortype_append(CURVE_OT_spline_weight_set);
	WM_operatortype_append(CURVE_OT_handle_type_set);
	WM_operatortype_append(CURVE_OT_shade_smooth);
	WM_operatortype_append(CURVE_OT_shade_flat);
	WM_operatortype_append(CURVE_OT_tilt_clear);

	WM_operatortype_append(CURVE_OT_smooth);
	WM_operatortype_append(CURVE_OT_smooth_radius);

	WM_operatortype_append(CURVE_OT_de_select_first);
	WM_operatortype_append(CURVE_OT_de_select_last);
	WM_operatortype_append(CURVE_OT_select_all_toggle);
	WM_operatortype_append(CURVE_OT_select_inverse);
	WM_operatortype_append(CURVE_OT_select_linked);
	WM_operatortype_append(CURVE_OT_select_row);
	WM_operatortype_append(CURVE_OT_select_next);
	WM_operatortype_append(CURVE_OT_select_previous);
	WM_operatortype_append(CURVE_OT_select_more);
	WM_operatortype_append(CURVE_OT_select_less);
	WM_operatortype_append(CURVE_OT_select_random);
	WM_operatortype_append(CURVE_OT_select_every_nth);

	WM_operatortype_append(CURVE_OT_switch_direction);
	WM_operatortype_append(CURVE_OT_subdivide);
	WM_operatortype_append(CURVE_OT_make_segment);
	WM_operatortype_append(CURVE_OT_spin);
	WM_operatortype_append(CURVE_OT_vertex_add);
	WM_operatortype_append(CURVE_OT_extrude);
	WM_operatortype_append(CURVE_OT_cyclic_toggle);

	WM_operatortype_append(CURVE_OT_specials_menu);
}

void ED_keymap_curve(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
//	wmKeyMapItem *kmi;
	
	keymap= WM_keymap_find(keyconf, "Font", 0, 0);
	keymap->poll= ED_operator_editfont;
	
	/* only set in editmode font, by space_view3d listener */
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", BKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_BOLD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_ITALIC);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_style_toggle", UKEY, KM_PRESS, KM_CTRL, 0)->ptr, "style", CU_UNDERLINE);

	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", DELKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_NEXT_SEL);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_delete", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_PREV_SEL);
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
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", UPARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", DOWNARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", PAGEUPKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "FONT_OT_move_select", PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_PAGE);

	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_spacing", LEFTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_spacing", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_character", UPARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "FONT_OT_change_character", DOWNARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "delta", -1);

	WM_keymap_add_item(keymap, "FONT_OT_text_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_cut", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_cut", XKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_paste", VKEY, KM_PRESS, KM_OSKEY, 0);

	WM_keymap_add_item(keymap, "FONT_OT_line_break", RETKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "FONT_OT_text_insert", KM_TEXTINPUT, KM_ANY, KM_ANY, 0); // last!

	/* only set in editmode curve, by space_view3d listener */
	keymap= WM_keymap_find(keyconf, "Curve", 0, 0);
	keymap->poll= ED_operator_editsurfcurve;
	
	WM_keymap_add_item(keymap, "OBJECT_OT_curve_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_vertex_add", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "CURVE_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_row", RKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "CURVE_OT_select_linked", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "deselect", 1);

	WM_keymap_add_item(keymap, "CURVE_OT_separate", PKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_extrude", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_make_segment", FKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_cyclic_toggle", CKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CURVE_OT_tilt_clear", TKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "TFM_OT_transform", TKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", TFM_TILT);
	RNA_enum_set(WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", TFM_CURVE_SHRINKFATTEN);
	RNA_enum_set(WM_keymap_add_item(keymap, "CURVE_OT_handle_type_set", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", 1);
	RNA_enum_set(WM_keymap_add_item(keymap, "CURVE_OT_handle_type_set", HKEY, KM_PRESS, 0, 0)->ptr, "type", 3);
	RNA_enum_set(WM_keymap_add_item(keymap, "CURVE_OT_handle_type_set", VKEY, KM_PRESS, 0, 0)->ptr, "type", 2);

	WM_keymap_add_item(keymap, "CURVE_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "CURVE_OT_hide", HKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "CURVE_OT_hide", HKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0)->ptr, "unselected", 1);

	WM_keymap_add_item(keymap, "CURVE_OT_specials_menu", WKEY, KM_PRESS, 0, 0);

	/* menus */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_hook", HKEY, KM_PRESS, KM_CTRL, 0);

	ED_object_generic_keymap(keyconf, keymap, TRUE);
}

