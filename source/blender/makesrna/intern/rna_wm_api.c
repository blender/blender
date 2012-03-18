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

/** \file blender/makesrna/intern/rna_wm_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"

static wmKeyMap *rna_keymap_active(wmKeyMap *km, bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	return WM_keymap_active(wm, km);
}

static void rna_keymap_restore_item_to_default(wmKeyMap *km, bContext *C, wmKeyMapItem *kmi)
{
	WM_keymap_restore_item_to_default(C, km, kmi);
}

static void rna_Operator_report(wmOperator *op, int type, const char *msg)
{
	BKE_report(op->reports, type, msg);
}

/* since event isn't needed... */
static void rna_Operator_enum_search_invoke(bContext *C, wmOperator *op)
{
	WM_enum_search_invoke(C, op, NULL);
	
}

static int rna_event_modal_handler_add(struct bContext *C, struct wmOperator *operator)
{
	return WM_event_add_modal_handler(C, operator) != NULL;
}

/* XXX, need a way for python to know event types, 0x0110 is hard coded */
struct wmTimer *rna_event_timer_add(struct wmWindowManager *wm, float time_step, wmWindow *win)
{
	return WM_event_add_timer(wm, win, 0x0110, time_step);
}

void rna_event_timer_remove(struct wmWindowManager *wm, wmTimer *timer)
{
	WM_event_remove_timer(wm, timer->win, timer);
}

static wmKeyMapItem *rna_KeyMap_item_new(wmKeyMap *km, ReportList *reports, const char *idname, int type, int value, int any, int shift, int ctrl, int alt, int oskey, int keymodifier)
{
/*	wmWindowManager *wm = CTX_wm_manager(C); */
	char idname_bl[OP_MAX_TYPENAME];
	int modifier = 0;

	/* only on non-modal maps */
	if (km->flag & KEYMAP_MODAL) {
		BKE_report(reports, RPT_ERROR, "Not a non-modal keymap");
		return NULL;
	}

	WM_operator_bl_idname(idname_bl, idname);

	if (shift) modifier |= KM_SHIFT;
	if (ctrl) modifier |= KM_CTRL;
	if (alt) modifier |= KM_ALT;
	if (oskey) modifier |= KM_OSKEY;

	if (any) modifier = KM_ANY;

	return WM_keymap_add_item(km, idname_bl, type, value, modifier, keymodifier);
}

static wmKeyMapItem *rna_KeyMap_item_new_modal(wmKeyMap *km, ReportList *reports, const char *propvalue_str, int type, int value, int any, int shift, int ctrl, int alt, int oskey, int keymodifier)
{
	int modifier = 0;
	int propvalue = 0;

	/* only modal maps */
	if ((km->flag & KEYMAP_MODAL) == 0) {
		BKE_report(reports, RPT_ERROR, "Not a modal keymap");
		return NULL;
	}

	if (!km->modal_items) {
		BKE_report(reports, RPT_ERROR, "No property values defined");
		return NULL;
	}


	if (RNA_enum_value_from_id(km->modal_items, propvalue_str, &propvalue) == 0) {
		BKE_report(reports, RPT_WARNING, "Property value not in enumeration");
	}

	if (shift) modifier |= KM_SHIFT;
	if (ctrl) modifier |= KM_CTRL;
	if (alt) modifier |= KM_ALT;
	if (oskey) modifier |= KM_OSKEY;

	if (any) modifier = KM_ANY;

	return WM_modalkeymap_add_item(km, type, value, modifier, keymodifier, propvalue);
}

static wmKeyMap *rna_keymap_new(wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid, int modal)
{
	if (modal == 0) {
		return WM_keymap_find(keyconf, idname, spaceid, regionid);
	} else {
		return WM_modalkeymap_add(keyconf, idname, NULL); /* items will be lazy init */
	}
}

static wmKeyMap *rna_keymap_find(wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid)
{
	return WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);
}

static wmKeyMap *rna_keymap_find_modal(wmKeyConfig *UNUSED(keyconf), const char *idname)
{
	wmOperatorType *ot = WM_operatortype_find(idname, 0);

	if (!ot)
		return NULL;
	else
		return ot->modalkeymap;
}

#else

#define WM_GEN_INVOKE_EVENT (1<<0)
#define WM_GEN_INVOKE_SIZE (1<<1)
#define WM_GEN_INVOKE_RETURN (1<<2)

static void rna_generic_op_invoke(FunctionRNA *func, int flag)
{
	PropertyRNA *parm;

	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "operator", "Operator", "", "Operator to call");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	if (flag & WM_GEN_INVOKE_EVENT) {
		parm = RNA_def_pointer(func, "event", "Event", "", "Event");
		RNA_def_property_flag(parm, PROP_REQUIRED);
	}

	if (flag & WM_GEN_INVOKE_SIZE) {
		RNA_def_int(func, "width", 300, 0, INT_MAX, "", "Width of the popup", 0, INT_MAX);
		RNA_def_int(func, "height", 20, 0, INT_MAX, "", "Height of the popup", 0, INT_MAX);
	}

	if (flag & WM_GEN_INVOKE_RETURN) {
		parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", "");
		RNA_def_function_return(func, parm);
	}
}

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "fileselect_add", "WM_event_add_fileselect");
	RNA_def_function_ui_description(func, "Opens a file selector with an operator. "
	                                "The string properties 'filepath', 'filename', 'directory' and a 'files' collection "
	                                "are assigned when present in the operator");
	rna_generic_op_invoke(func, 0);

	func = RNA_def_function(srna, "modal_handler_add", "rna_event_modal_handler_add");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "operator", "Operator", "", "Operator to call");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_return(func, RNA_def_boolean(func, "handle", 1, "", ""));


	func = RNA_def_function(srna, "event_timer_add", "rna_event_timer_add");
	parm = RNA_def_property(func, "time_step", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_range(parm, 0.0, FLT_MAX);
	RNA_def_property_ui_text(parm, "Time Step", "Interval in seconds between timer events");
	RNA_def_pointer(func, "window", "Window", "", "Window to attach the timer to or None");
	parm = RNA_def_pointer(func, "result", "Timer", "", "");
	RNA_def_function_return(func, parm);


	func = RNA_def_function(srna, "event_timer_remove", "rna_event_timer_remove");
	parm = RNA_def_pointer(func, "timer", "Timer", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);


	/* invoke functions, for use with python */
	func = RNA_def_function(srna, "invoke_props_popup", "WM_operator_props_popup");
	RNA_def_function_ui_description(func, "Operator popup invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT|WM_GEN_INVOKE_RETURN);

	/* invoked dialog opens popup with OK button, does not auto-exec operator. */
	func = RNA_def_function(srna, "invoke_props_dialog", "WM_operator_props_dialog_popup");
	RNA_def_function_ui_description(func, "Operator dialog (non-autoexec popup) invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE|WM_GEN_INVOKE_RETURN);

	/* invoke enum */
	func = RNA_def_function(srna, "invoke_search_popup", "rna_Operator_enum_search_invoke");
	rna_generic_op_invoke(func, 0);

	/* invoke functions, for use with python */
	func = RNA_def_function(srna, "invoke_popup", "WM_operator_ui_popup");
	RNA_def_function_ui_description(func, "Operator popup invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE|WM_GEN_INVOKE_RETURN);

	func = RNA_def_function(srna, "invoke_confirm", "WM_operator_confirm");
	RNA_def_function_ui_description(func, "Operator confirmation");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT|WM_GEN_INVOKE_RETURN);
	
}

void RNA_api_operator(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* utility, not for registering */
	func = RNA_def_function(srna, "report", "rna_Operator_report");
	parm = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "message", "", 0, "Report Message", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);


	/* Registration */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the operator can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	/* exec */
	func = RNA_def_function(srna, "execute", NULL);
	RNA_def_function_ui_description(func, "Execute the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);

	/* check */
	func = RNA_def_function(srna, "check", NULL);
	RNA_def_function_ui_description(func, "Check the operator settings, return True to signal a change to redraw");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	parm = RNA_def_boolean(func, "result", 0, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);
	
	/* invoke */
	func = RNA_def_function(srna, "invoke", NULL);
	RNA_def_function_ui_description(func, "Invoke the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "modal", NULL); /* same as invoke */
	RNA_def_function_ui_description(func, "Modal operator function");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw function for the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	/* cancel */
	func = RNA_def_function(srna, "cancel", NULL);
	RNA_def_function_ui_description(func, "Called when the operator is canceled");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);
}

void RNA_api_macro(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* utility, not for registering */
	func = RNA_def_function(srna, "report", "rna_Operator_report");
	parm = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "message", "", 0, "Report Message", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);


	/* Registration */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the operator can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw function for the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
}

void RNA_api_keyconfig(StructRNA *srna)
{
	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */
}

void RNA_api_keymap(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "active", "rna_keymap_active");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Active key map");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "restore_to_default", "WM_keymap_restore_to_default");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "restore_item_to_default", "rna_keymap_restore_item_to_default");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
}

void RNA_api_keymapitem(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "compare", "WM_keymap_item_compare");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_boolean(func, "result", 0, "Comparison result", "");
	RNA_def_function_return(func, parm);
}

void RNA_api_keymapitems(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "new", "rna_KeyMap_item_new");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "idname", "", 0, "Operator Identifier", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "type", event_type_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "value", event_value_items, 0, "Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "any", 0, "Any", "");
	RNA_def_boolean(func, "shift", 0, "Shift", "");
	RNA_def_boolean(func, "ctrl", 0, "Ctrl", "");
	RNA_def_boolean(func, "alt", 0, "Alt", "");
	RNA_def_boolean(func, "oskey", 0, "OS Key", "");
	RNA_def_enum(func, "key_modifier", event_type_items, 0, "Key Modifier", "");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "new_modal", "rna_KeyMap_item_new_modal");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "propvalue", "", 0, "Property Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "type", event_type_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "value", event_value_items, 0, "Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "any", 0, "Any", "");
	RNA_def_boolean(func, "shift", 0, "Shift", "");
	RNA_def_boolean(func, "ctrl", 0, "Ctrl", "");
	RNA_def_boolean(func, "alt", 0, "Alt", "");
	RNA_def_boolean(func, "oskey", 0, "OS Key", "");
	RNA_def_enum(func, "key_modifier", event_type_items, 0, "Key Modifier", "");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "remove", "WM_keymap_remove_item");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "from_id", "WM_keymap_item_find_id");
	parm = RNA_def_property(func, "id", PROP_INT, PROP_NONE);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_ui_text(parm, "id", "ID of the item");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_function_return(func, parm);
}

void RNA_api_keymaps(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "new", "rna_keymap_new"); /* add_keymap */
	parm = RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	RNA_def_boolean(func, "modal", 0, "Modal", "");
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Added key map");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "find", "rna_keymap_find"); /* find_keymap */
	parm = RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "find_modal", "rna_keymap_find_modal"); /* find_keymap_modal */
	parm = RNA_def_string(func, "name", "", 0, "Operator Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
	RNA_def_function_return(func, parm);
}

void RNA_api_keyconfigs(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "new", "WM_keyconfig_new_user"); /* add_keyconfig */
	parm = RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Added key configuration");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "WM_keyconfig_remove"); /* remove_keyconfig */
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Removed key configuration");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif

