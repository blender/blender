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

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "wm_cursors.h"

#include "rna_internal.h"  /* own include */

/* confusingm 2 enums mixed up here */
EnumPropertyItem window_cursor_items[] = {
	{CURSOR_STD, "DEFAULT", 0, "Default", ""},
	{CURSOR_NONE, "NONE", 0, "None", ""},
	{CURSOR_WAIT, "WAIT", 0, "Wait", ""},
	{CURSOR_EDIT, "CROSSHAIR", 0, "Crosshair", ""},
	{CURSOR_X_MOVE, "MOVE_X", 0, "Move-X", ""},
	{CURSOR_Y_MOVE, "MOVE_Y", 0, "Move-Y", ""},

	/* new */
	{BC_KNIFECURSOR, "KNIFE", 0, "Knife", ""},
	{BC_TEXTEDITCURSOR, "TEXT", 0, "Text", ""},
	{BC_PAINTBRUSHCURSOR, "PAINT_BRUSH", 0, "Paint Brush", ""},
	{BC_HANDCURSOR, "HAND", 0, "Hand", ""},
	{BC_EW_SCROLLCURSOR, "SCROLL_X", 0, "Scroll-X", ""},
	{BC_NS_SCROLLCURSOR, "SCROLL_Y", 0, "Scroll-Y", ""},
	{BC_NSEW_SCROLLCURSOR, "SCROLL_XY", 0, "Scroll-XY", ""},
	{BC_EYEDROPPER_CURSOR, "EYEDROPPER", 0, "Eyedropper", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "UI_interface.h"
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
static wmTimer *rna_event_timer_add(struct wmWindowManager *wm, float time_step, wmWindow *win)
{
	return WM_event_add_timer(wm, win, 0x0110, time_step);
}

static void rna_event_timer_remove(struct wmWindowManager *wm, wmTimer *timer)
{
	WM_event_remove_timer(wm, timer->win, timer);
}

/* placeholder data for final implementation of a true progressbar */
struct wmStaticProgress {
	float min;
	float max;
	bool  is_valid;
} wm_progress_state = {0, 0, false};


static void rna_progress_begin(struct wmWindowManager *UNUSED(wm), float min, float max)
{
	float range = max - min;
	if (range != 0) {
		wm_progress_state.min = min;
		wm_progress_state.max = max;
		wm_progress_state.is_valid = true;
	}
	else {
		wm_progress_state.is_valid = false;
	}
}

static void rna_progress_update(struct wmWindowManager *wm, float value)
{
	if (wm_progress_state.is_valid) {
		/* Map to cursor_time range [0,9999] */
		wmWindow *win = wm->winactive;
		if (win) {
			int val = (int)(10000 * (value - wm_progress_state.min) / (wm_progress_state.max - wm_progress_state.min));
			WM_cursor_time(win, val);
		}
	}
}

static void rna_progress_end(struct wmWindowManager *wm)
{
	if (wm_progress_state.is_valid) {
		wmWindow *win = wm->winactive;
		if (win) {
			WM_cursor_modal_restore(win);
			wm_progress_state.is_valid = false;
		}
	}
}

/* wrap these because of 'const wmEvent *' */
static int rna_Operator_confirm(bContext *C, wmOperator *op, wmEvent *event)
{
	return WM_operator_confirm(C, op, event);
}
static int rna_Operator_props_popup(bContext *C, wmOperator *op, wmEvent *event)
{
	return WM_operator_props_popup(C, op, event);
}

static wmKeyMapItem *rna_KeyMap_item_new(wmKeyMap *km, ReportList *reports, const char *idname, int type, int value,
                                         int any, int shift, int ctrl, int alt, int oskey, int keymodifier, int head)
{
/*	wmWindowManager *wm = CTX_wm_manager(C); */
	wmKeyMapItem *kmi = NULL;
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
	
	/* create keymap item */
	kmi = WM_keymap_add_item(km, idname_bl, type, value, modifier, keymodifier);
	
	/* [#32437] allow scripts to define hotkeys that get added to start of keymap 
	 *          so that they stand a chance against catch-all defines later on
	 */
	if (head) {
		BLI_remlink(&km->items, kmi);
		BLI_addhead(&km->items, kmi);
	}
	
	return kmi;
}

static wmKeyMapItem *rna_KeyMap_item_new_modal(wmKeyMap *km, ReportList *reports, const char *propvalue_str,
                                               int type, int value, int any, int shift, int ctrl, int alt,
                                               int oskey, int keymodifier)
{
	int modifier = 0;
	int propvalue = 0;

	/* only modal maps */
	if ((km->flag & KEYMAP_MODAL) == 0) {
		BKE_report(reports, RPT_ERROR, "Not a modal keymap");
		return NULL;
	}

	if (shift) modifier |= KM_SHIFT;
	if (ctrl) modifier |= KM_CTRL;
	if (alt) modifier |= KM_ALT;
	if (oskey) modifier |= KM_OSKEY;

	if (any) modifier = KM_ANY;

	/* not initialized yet, do delayed lookup */
	if (!km->modal_items)
		return WM_modalkeymap_add_item_str(km, type, value, modifier, keymodifier, propvalue_str);

	if (RNA_enum_value_from_id(km->modal_items, propvalue_str, &propvalue) == 0)
		BKE_report(reports, RPT_WARNING, "Property value not in enumeration");

	return WM_modalkeymap_add_item(km, type, value, modifier, keymodifier, propvalue);
}

static void rna_KeyMap_item_remove(wmKeyMap *km, ReportList *reports, PointerRNA *kmi_ptr)
{
	wmKeyMapItem *kmi = kmi_ptr->data;

	if (WM_keymap_remove_item(km, kmi) == false) {
		BKE_reportf(reports, RPT_ERROR, "KeyMapItem '%s' cannot be removed from '%s'", kmi->idname, km->idname);
		return;
	}

	RNA_POINTER_INVALIDATE(kmi_ptr);
}

static wmKeyMap *rna_keymap_new(wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid, int modal)
{
	if (modal == 0) {
		return WM_keymap_find(keyconf, idname, spaceid, regionid);
	}
	else {
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

static void rna_KeyMap_remove(wmKeyConfig *keyconfig, ReportList *reports, PointerRNA *keymap_ptr)
{
	wmKeyMap *keymap = keymap_ptr->data;

	if (WM_keymap_remove(keyconfig, keymap) == false) {
		BKE_reportf(reports, RPT_ERROR, "KeyConfig '%s' cannot be removed", keymap->idname);
		return;
	}

	RNA_POINTER_INVALIDATE(keymap_ptr);
}

static void rna_KeyConfig_remove(wmWindowManager *wm, ReportList *reports, PointerRNA *keyconf_ptr)
{
	wmKeyConfig *keyconf = keyconf_ptr->data;

	if (WM_keyconfig_remove(wm, keyconf) == false) {
		BKE_reportf(reports, RPT_ERROR, "KeyConfig '%s' cannot be removed", keyconf->idname);
		return;
	}

	RNA_POINTER_INVALIDATE(keyconf_ptr);
}

/* popup menu wrapper */
static PointerRNA rna_PupMenuBegin(bContext *C, const char *title, int icon)
{
	PointerRNA r_ptr;
	void *data;

	data = (void *)uiPupMenuBegin(C, title, icon);

	RNA_pointer_create(NULL, &RNA_UIPopupMenu, data, &r_ptr);

	return r_ptr;
}

static void rna_PupMenuEnd(bContext *C, PointerRNA *handle)
{
	uiPupMenuEnd(C, handle->data);
}

/* pie menu wrapper */
static PointerRNA rna_PieMenuBegin(bContext *C, const char *title, int icon, PointerRNA *event)
{
	PointerRNA r_ptr;
	void *data;

	data = (void *)uiPieMenuBegin(C, title, icon, event->data);

	RNA_pointer_create(NULL, &RNA_UIPieMenu, data, &r_ptr);

	return r_ptr;
}

static void rna_PieMenuEnd(bContext *C, PointerRNA *handle)
{
	uiPieMenuEnd(C, handle->data);
}

#else

#define WM_GEN_INVOKE_EVENT (1 << 0)
#define WM_GEN_INVOKE_SIZE (1 << 1)
#define WM_GEN_INVOKE_RETURN (1 << 2)

static void rna_generic_op_invoke(FunctionRNA *func, int flag)
{
	PropertyRNA *parm;

	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
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

void RNA_api_window(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "cursor_warp", "WM_cursor_warp");
	parm = RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Set the cursor position");

	func = RNA_def_function(srna, "cursor_set", "WM_cursor_set");
	parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(parm, window_cursor_items);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Set the cursor");

	func = RNA_def_function(srna, "cursor_modal_set", "WM_cursor_modal_set");
	parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(parm, window_cursor_items);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Set the cursor, so the previous cursor can be restored");

	RNA_def_function(srna, "cursor_modal_restore", "WM_cursor_modal_restore");
	RNA_def_function_ui_description(func, "Restore the previous cursor after calling ``cursor_modal_set``");
}

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "fileselect_add", "WM_event_add_fileselect");
	RNA_def_function_ui_description(func, "Opens a file selector with an operator. "
	                                "The string properties 'filepath', 'filename', 'directory' and a 'files' "
	                                "collection are assigned when present in the operator");
	rna_generic_op_invoke(func, 0);

	func = RNA_def_function(srna, "modal_handler_add", "rna_event_modal_handler_add");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* Progress bar interface */
	func = RNA_def_function(srna, "progress_begin", "rna_progress_begin");
	RNA_def_function_ui_description(func, "Start Progress bar");

	parm = RNA_def_property(func, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(parm, "min", "any value in range [0,9999]");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	parm = RNA_def_property(func, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_ui_text(parm, "max", "any value in range [min+1,9998]");

	func = RNA_def_function(srna, "progress_update", "rna_progress_update");
	parm = RNA_def_property(func, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_ui_text(parm, "value", "any value between min and max as set in progress_begin()");

	func = RNA_def_function(srna, "progress_end", "rna_progress_end");
	RNA_def_function_ui_description(func, "Terminate Progress bar");

	/* invoke functions, for use with python */
	func = RNA_def_function(srna, "invoke_props_popup", "rna_Operator_props_popup");
	RNA_def_function_ui_description(func, "Operator popup invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT | WM_GEN_INVOKE_RETURN);

	/* invoked dialog opens popup with OK button, does not auto-exec operator. */
	func = RNA_def_function(srna, "invoke_props_dialog", "WM_operator_props_dialog_popup");
	RNA_def_function_ui_description(func, "Operator dialog (non-autoexec popup) invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE | WM_GEN_INVOKE_RETURN);

	/* invoke enum */
	func = RNA_def_function(srna, "invoke_search_popup", "rna_Operator_enum_search_invoke");
	rna_generic_op_invoke(func, 0);

	/* invoke functions, for use with python */
	func = RNA_def_function(srna, "invoke_popup", "WM_operator_ui_popup");
	RNA_def_function_ui_description(func, "Operator popup invoke");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE | WM_GEN_INVOKE_RETURN);

	func = RNA_def_function(srna, "invoke_confirm", "rna_Operator_confirm");
	RNA_def_function_ui_description(func, "Operator confirmation");
	rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT | WM_GEN_INVOKE_RETURN);


	/* wrap uiPupMenuBegin */
	func = RNA_def_function(srna, "pupmenu_begin__internal", "rna_PupMenuBegin");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "title", NULL, 0, "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(parm, icon_items);
	/* return */
	parm = RNA_def_pointer(func, "menu", "UIPopupMenu", "", "");
	RNA_def_property_flag(parm, PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_function_return(func, parm);

	/* wrap uiPupMenuEnd */
	func = RNA_def_function(srna, "pupmenu_end__internal", "rna_PupMenuEnd");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "menu", "UIPopupMenu", "", "");
	RNA_def_property_flag(parm, PROP_RNAPTR | PROP_NEVER_NULL);

	/* wrap uiPieMenuBegin */
	func = RNA_def_function(srna, "piemenu_begin__internal", "rna_PieMenuBegin");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "title", NULL, 0, "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(parm, icon_items);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_property_flag(parm, PROP_RNAPTR | PROP_NEVER_NULL);
	/* return */
	parm = RNA_def_pointer(func, "menu_pie", "UIPieMenu", "", "");
	RNA_def_property_flag(parm, PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_function_return(func, parm);

	/* wrap uiPieMenuEnd */
	func = RNA_def_function(srna, "piemenu_end__internal", "rna_PieMenuEnd");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "menu", "UIPieMenu", "", "");
	RNA_def_property_flag(parm, PROP_RNAPTR | PROP_NEVER_NULL);
}

void RNA_api_operator(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* utility, not for registering */
	func = RNA_def_function(srna, "report", "rna_Operator_report");
	parm = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);


	/* Registration */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the operator can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* exec */
	func = RNA_def_function(srna, "execute", NULL);
	RNA_def_function_ui_description(func, "Execute the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* better name? */
	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);

	/* check */
	func = RNA_def_function(srna, "check", NULL);
	RNA_def_function_ui_description(func, "Check the operator settings, return True to signal a change to redraw");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	parm = RNA_def_boolean(func, "result", 0, "result", ""); /* better name? */
	RNA_def_function_return(func, parm);
	
	/* invoke */
	func = RNA_def_function(srna, "invoke", NULL);
	RNA_def_function_ui_description(func, "Invoke the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* better name? */
	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "modal", NULL); /* same as invoke */
	RNA_def_function_ui_description(func, "Modal operator function");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* better name? */
	parm = RNA_def_enum_flag(func, "result", operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw function for the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* cancel */
	func = RNA_def_function(srna, "cancel", NULL);
	RNA_def_function_ui_description(func, "Called when the operator is canceled");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

void RNA_api_macro(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* utility, not for registering */
	func = RNA_def_function(srna, "report", "rna_Operator_report");
	parm = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);


	/* Registration */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the operator can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw function for the operator");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

void RNA_api_keyconfig(StructRNA *UNUSED(srna))
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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
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
	parm = RNA_def_string(func, "idname", NULL, 0, "Operator Identifier", "");
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
	RNA_def_boolean(func, "head", 0, "At Head", 
	                "Force item to be added at start (not end) of key map so that "
	                "it doesn't get blocked by an existing key map item");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "new_modal", "rna_KeyMap_item_new_modal");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "propvalue", NULL, 0, "Property Value", "");
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
	
	func = RNA_def_function(srna, "remove", "rna_KeyMap_item_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

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
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	RNA_def_boolean(func, "modal", 0, "Modal", "");
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Added key map");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_KeyMap_remove"); /* remove_keymap */
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Removed key map");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	func = RNA_def_function(srna, "find", "rna_keymap_find"); /* find_keymap */
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "find_modal", "rna_keymap_find_modal"); /* find_keymap_modal */
	parm = RNA_def_string(func, "name", NULL, 0, "Operator Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
	RNA_def_function_return(func, parm);
}

void RNA_api_keyconfigs(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "new", "WM_keyconfig_new_user"); /* add_keyconfig */
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Added key configuration");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_KeyConfig_remove"); /* remove_keyconfig */
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Removed key configuration");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
}

#endif
