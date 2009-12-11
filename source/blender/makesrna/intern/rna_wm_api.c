/**
 * $Id$
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
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

static wmKeyMap *rna_keymap_add(wmKeyConfig *keyconf, char *idname, int spaceid, int regionid, int modal)
{
	if (modal == 0) {
		return WM_keymap_find(keyconf, idname, spaceid, regionid);
	} else {
		return WM_modalkeymap_add(keyconf, idname, NULL); /* items will be lazy init */
	}
}

static wmKeyMap *rna_keymap_find(wmKeyConfig *keyconf, char *idname, int spaceid, int regionid)
{
	return WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);
}

static wmKeyMap *rna_keymap_find_modal(wmKeyConfig *keyconf, char *idname)
{
	wmOperatorType *ot = WM_operatortype_find(idname, 0);

	if (!ot)
		return NULL;
	else
		return ot->modalkeymap;
}

static wmKeyMap *rna_keymap_active(wmKeyMap *km, bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	return WM_keymap_active(wm, km);
}


static wmKeyMapItem *rna_KeyMap_add_modal_item(wmKeyMap *km, bContext *C, ReportList *reports, char* propvalue_str, int type, int value, int any, int shift, int ctrl, int alt, int oskey, int keymodifier)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	int modifier= 0;
	int propvalue = 0;

	/* only modal maps */
	if ((km->flag & KEYMAP_MODAL) == 0) {
		BKE_report(reports, RPT_ERROR, "Not a modal keymap.");
		return NULL;
	}

	if (!km->modal_items) {
		if(!WM_keymap_user_init(wm, km)) {
			BKE_report(reports, RPT_ERROR, "User defined keymap doesn't correspond to a system keymap.");
			return NULL;
		}
	}

	if (!km->modal_items) {
		BKE_report(reports, RPT_ERROR, "No property values defined.");
		return NULL;
	}


	if(RNA_enum_value_from_id(km->modal_items, propvalue_str, &propvalue)==0) {
		BKE_report(reports, RPT_WARNING, "Property value not in enumeration.");
	}

	if(shift) modifier |= KM_SHIFT;
	if(ctrl) modifier |= KM_CTRL;
	if(alt) modifier |= KM_ALT;
	if(oskey) modifier |= KM_OSKEY;

	if(any) modifier = KM_ANY;

	return WM_modalkeymap_add_item(km, type, value, modifier, keymodifier, propvalue);
}

static wmKeyMapItem *rna_KeyMap_add_item(wmKeyMap *km, ReportList *reports, char *idname, int type, int any, int value, int shift, int ctrl, int alt, int oskey, int keymodifier)
{
//	wmWindowManager *wm = CTX_wm_manager(C);
	int modifier= 0;

	/* only on non-modal maps */
	if (km->flag & KEYMAP_MODAL) {
		BKE_report(reports, RPT_ERROR, "Not a non-modal keymap.");
		return NULL;
	}

	if(shift) modifier |= KM_SHIFT;
	if(ctrl) modifier |= KM_CTRL;
	if(alt) modifier |= KM_ALT;
	if(oskey) modifier |= KM_OSKEY;

	if(any) modifier = KM_ANY;

	return WM_keymap_add_item(km, idname, type, value, modifier, keymodifier);
}

static void rna_Operator_report(wmOperator *op, int type, char *msg)
{
	BKE_report(op->reports, type, msg);
}

#else

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_fileselect", "WM_event_add_fileselect");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Show up the file selector.");
	parm= RNA_def_pointer(func, "operator", "Operator", "", "Operator to call.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_keyconfig", "WM_keyconfig_add");
	parm= RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Added key configuration.");
	RNA_def_function_return(func, parm);
	
	/* invoke functions, for use with python */
	func= RNA_def_function(srna, "invoke_props_popup", "WM_operator_props_popup");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Operator popup invoke.");
	parm= RNA_def_pointer(func, "operator", "Operator", "", "Operator to call.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "event", "Event", "", "Event.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	parm= RNA_def_enum(func, "result", operator_return_items, 0, "result", ""); // better name?
	RNA_def_property_flag(parm, PROP_ENUM_FLAG);
	RNA_def_function_return(func, parm);


	/* invoke functions, for use with python */
	func= RNA_def_function(srna, "invoke_popup", "WM_operator_ui_popup");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Operator popup invoke.");
	parm= RNA_def_pointer(func, "operator", "Operator", "", "Operator to call.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "width", 300, 0, INT_MAX, "", "Width of the popup.", 0, INT_MAX);
	parm= RNA_def_int(func, "height", 20, 0, INT_MAX, "", "Height of the popup.", 0, INT_MAX);
}

void RNA_api_operator(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "report", "rna_Operator_report");
	parm= RNA_def_enum(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_ENUM_FLAG);
	parm= RNA_def_string(func, "message", "", 0, "Report Message", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_api_keyconfig(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_keymap", "rna_keymap_add");
	parm= RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	RNA_def_boolean(func, "modal", 0, "Modal", "");
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Added key map.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "find_keymap", "rna_keymap_find");
	parm= RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "find_keymap_modal", "rna_keymap_find_modal");
	parm= RNA_def_string(func, "name", "", 0, "Operator Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map.");
	RNA_def_function_return(func, parm);
}

void RNA_api_keymap(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_item", "rna_KeyMap_add_item");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm= RNA_def_string(func, "idname", "", 0, "Operator Identifier", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", event_type_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "value", event_value_items, 0, "Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "any", 0, "Any", "");
	RNA_def_boolean(func, "shift", 0, "Shift", "");
	RNA_def_boolean(func, "ctrl", 0, "Ctrl", "");
	RNA_def_boolean(func, "alt", 0, "Alt", "");
	RNA_def_boolean(func, "oskey", 0, "OS Key", "");
	RNA_def_enum(func, "key_modifier", event_type_items, 0, "Key Modifier", "");
	parm= RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_modal_item", "rna_KeyMap_add_modal_item");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_string(func, "propvalue", "", 0, "Property Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", event_type_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "value", event_value_items, 0, "Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "any", 0, "Any", "");
	RNA_def_boolean(func, "shift", 0, "Shift", "");
	RNA_def_boolean(func, "ctrl", 0, "Ctrl", "");
	RNA_def_boolean(func, "alt", 0, "Alt", "");
	RNA_def_boolean(func, "oskey", 0, "OS Key", "");
	RNA_def_enum(func, "key_modifier", event_type_items, 0, "Key Modifier", "");
	parm= RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "active", "rna_keymap_active");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Active key map.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove_item", "WM_keymap_remove_item");
	parm= RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "copy_to_user", "WM_keymap_copy_to_user");
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "User editable key map.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "restore_to_default", "WM_keymap_restore_to_default");
}

#endif

