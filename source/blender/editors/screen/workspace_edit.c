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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/workspace_edit.c
 *  \ingroup edscr
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_appdir.h"
#include "BKE_blendfile.h"
#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_toolsystem.h"

#include "screen_intern.h"


/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */

WorkSpace *ED_workspace_add(Main *bmain, const char *name)
{
	return BKE_workspace_add(bmain, name);
}

/**
 * Changes the object mode (if needed) to the one set in \a workspace_new.
 * Object mode is still stored on object level. In future it should all be workspace level instead.
 */
static void workspace_change_update(
        WorkSpace *workspace_new, const WorkSpace *workspace_old,
        bContext *C, wmWindowManager *wm)
{
	/* needs to be done before changing mode! (to ensure right context) */
	UNUSED_VARS(workspace_old, workspace_new, C, wm);
#if 0
	Object *ob_act = CTX_data_active_object(C)
	eObjectMode mode_old = workspace_old->object_mode;
	eObjectMode mode_new = workspace_new->object_mode;

	if (mode_old != mode_new) {
		ED_object_mode_compat_set(C, ob_act, mode_new, &wm->reports);
		ED_object_mode_toggle(C, mode_new);
	}
#endif
}

static bool workspace_change_find_new_layout_cb(const WorkSpaceLayout *layout, void *UNUSED(arg))
{
	/* return false to stop the iterator if we've found a layout that can be activated */
	return workspace_layout_set_poll(layout) ? false : true;
}

static WorkSpaceLayout *workspace_change_get_new_layout(
        Main *bmain, WorkSpace *workspace_new, wmWindow *win)
{
	/* ED_workspace_duplicate may have stored a layout to activate once the workspace gets activated. */
	WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
	WorkSpaceLayout *layout_new;
	bScreen *screen_new;

	if (win->workspace_hook->temp_workspace_store) {
		layout_new = win->workspace_hook->temp_layout_store;
	}
	else {
		layout_new = BKE_workspace_hook_layout_for_workspace_get(win->workspace_hook, workspace_new);
		if (!layout_new) {
			layout_new = BKE_workspace_layouts_get(workspace_new)->first;
		}
	}
	screen_new = BKE_workspace_layout_screen_get(layout_new);

	if (screen_new->winid) {
		/* screen is already used, try to find a free one */
		WorkSpaceLayout *layout_temp = BKE_workspace_layout_iter_circular(
		                                   workspace_new, layout_new, workspace_change_find_new_layout_cb,
		                                   NULL, false);
		if (!layout_temp) {
			/* fallback solution: duplicate layout from old workspace */
			layout_temp = ED_workspace_layout_duplicate(bmain, workspace_new, layout_old, win);
		}
		layout_new = layout_temp;
	}

	return layout_new;
}

/**
 * \brief Change the active workspace.
 *
 * Operator call, WM + Window + screen already existed before
 * Pretty similar to #ED_screen_change since changing workspace also changes screen.
 *
 * \warning Do NOT call in area/region queues!
 * \returns if workspace changing was successful.
 */
bool ED_workspace_change(
        WorkSpace *workspace_new, bContext *C, wmWindowManager *wm, wmWindow *win)
{
	Main *bmain = CTX_data_main(C);
	WorkSpace *workspace_old = WM_window_get_active_workspace(win);
	WorkSpaceLayout *layout_new = workspace_change_get_new_layout(bmain, workspace_new, win);
	bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);
	bScreen *screen_old = BKE_workspace_active_screen_get(win->workspace_hook);

	win->workspace_hook->temp_layout_store = NULL;
	if (workspace_old == workspace_new) {
		/* Could also return true, everything that needs to be done was done (nothing :P), but nothing changed */
		return false;
	}

	screen_new = screen_change_prepare(screen_old, screen_new, bmain, C, win);
	BLI_assert(BKE_workspace_layout_screen_get(layout_new) == screen_new);

	if (screen_new) {
		BKE_workspace_hook_layout_for_workspace_set(win->workspace_hook, workspace_new, layout_new);
		BKE_workspace_active_set(win->workspace_hook, workspace_new);

		/* update screen *after* changing workspace - which also causes the
		 * actual screen change and updates context (including CTX_wm_workspace) */
		screen_change_update(C, win, screen_new);
		workspace_change_update(workspace_new, workspace_old, C, wm);

		BLI_assert(CTX_wm_workspace(C) == workspace_new);

		WM_toolsystem_unlink_all(C, workspace_old);
		WM_toolsystem_reinit_all(C, win);

		return true;
	}

	return false;
}

/**
 * Duplicate a workspace including its layouts. Does not activate the workspace, but
 * it stores the screen-layout to be activated (BKE_workspace_temp_layout_store)
 */
WorkSpace *ED_workspace_duplicate(
        WorkSpace *workspace_old, Main *bmain, wmWindow *win)
{
	WorkSpaceLayout *layout_active_old = BKE_workspace_active_layout_get(win->workspace_hook);
	ListBase *layouts_old = BKE_workspace_layouts_get(workspace_old);
	WorkSpace *workspace_new = ED_workspace_add(bmain, workspace_old->id.name + 2);

	/* TODO(campbell): tools */

	for (WorkSpaceLayout *layout_old = layouts_old->first; layout_old; layout_old = layout_old->next) {
		WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(bmain, workspace_new, layout_old, win);

		if (layout_active_old == layout_old) {
			win->workspace_hook->temp_layout_store = layout_new;
		}
	}
	return workspace_new;
}

/**
 * \return if succeeded.
 */
bool ED_workspace_delete(
        WorkSpace *workspace, Main *bmain, bContext *C, wmWindowManager *wm)
{
	ID *workspace_id = (ID *)workspace;

	if (BLI_listbase_is_single(&bmain->workspaces)) {
		return false;
	}

	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		WorkSpace *prev = workspace_id->prev;
		WorkSpace *next = workspace_id->next;

		ED_workspace_change((prev != NULL) ? prev : next, C, wm, win);
	}
	BKE_libblock_free(bmain, workspace_id);

	return true;
}

/**
 * Some editor data may need to be synced with scene data (3D View camera and layers).
 * This function ensures data is synced for editors in active layout of \a workspace.
 */
void ED_workspace_scene_data_sync(
        WorkSpaceInstanceHook *hook, Scene *scene)
{
	bScreen *screen = BKE_workspace_active_screen_get(hook);
	BKE_screen_view3d_scene_sync(screen, scene);
}

/** \} Workspace API */


/** \name Workspace Operators
 *
 * \{ */

static int workspace_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	WorkSpace *workspace = ED_workspace_duplicate(WM_window_get_active_workspace(win), bmain, win);

	WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_SET, workspace);

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Workspace";
	ot->description = "Add a new workspace";
	ot->idname = "WORKSPACE_OT_workspace_duplicate";

	/* api callbacks */
	ot->exec = workspace_new_exec;
	ot->poll = WM_operator_winactive;
}

static int workspace_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *win = CTX_wm_window(C);

	WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_DELETE, WM_window_get_active_workspace(win));

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Workspace";
	ot->description = "Delete the active workspace";
	ot->idname = "WORKSPACE_OT_workspace_delete";

	/* api callbacks */
	ot->exec = workspace_delete_exec;
}

static bool workspace_append_activate_poll(bContext *C)
{
	wmOperatorType *ot = WM_operatortype_find("WM_OT_append", false);
	return WM_operator_poll(C, ot);
}

static int workspace_append(bContext *C, const char *directory, const char *idname)
{
	wmOperatorType *ot = WM_operatortype_find("WM_OT_append", false);
	PointerRNA opptr;
	int retval;

	WM_operator_properties_create_ptr(&opptr, ot);
	RNA_string_set(&opptr, "directory", directory);
	RNA_string_set(&opptr, "filename", idname);
	RNA_boolean_set(&opptr, "autoselect", false);

	retval = WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &opptr);

	WM_operator_properties_free(&opptr);

	return retval;
}

static int workspace_append_activate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	char idname[MAX_ID_NAME - 2], directory[FILE_MAX];

	if (!RNA_struct_property_is_set(op->ptr, "idname") ||
	    !RNA_struct_property_is_set(op->ptr, "directory"))
	{
		return OPERATOR_CANCELLED;
	}
	RNA_string_get(op->ptr, "idname", idname);
	RNA_string_get(op->ptr, "directory", directory);

	if (workspace_append(C, directory, idname) != OPERATOR_CANCELLED) {
		WorkSpace *appended_workspace = BLI_findstring(&bmain->workspaces, idname, offsetof(ID, name) + 2);

		BLI_assert(appended_workspace != NULL);
		/* Changing workspace changes context. Do delayed! */
		WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_SET, appended_workspace);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static void WORKSPACE_OT_append_activate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Append and Activate Workspace";
	ot->description = "Append a workspace and make it the active one in the current window";
	ot->idname = "WORKSPACE_OT_append_activate";

	/* api callbacks */
	ot->exec = workspace_append_activate_exec;
	ot->poll = workspace_append_activate_poll;

	RNA_def_string(ot->srna, "idname", NULL, MAX_ID_NAME - 2, "Identifier",
	               "Name of the workspace to append and activate");
	RNA_def_string(ot->srna, "directory", NULL, FILE_MAX, "Directory",
	               "Path to the library");
}

static void workspace_config_file_path_from_folder_id(
        const Main *bmain, int folder_id, char *r_path)
{
	const char *app_template = U.app_template[0] ? U.app_template : NULL;
	const char * const cfgdir = BKE_appdir_folder_id(folder_id, app_template);

	if (cfgdir) {
		BLI_make_file_string(bmain->name, r_path, cfgdir, BLENDER_WORKSPACES_FILE);
	}
	else {
		r_path[0] = '\0';
	}
}

ATTR_NONNULL(1)
static WorkspaceConfigFileData *workspace_config_file_read(
        const Main *bmain, ReportList *reports)
{
	char workspace_config_path[FILE_MAX];
	bool has_path = false;

	workspace_config_file_path_from_folder_id(bmain, BLENDER_USER_CONFIG, workspace_config_path);
	if (BLI_exists(workspace_config_path)) {
		has_path = true;
	}
	else {
		workspace_config_file_path_from_folder_id(bmain, BLENDER_SYSTEM_DATAFILES, workspace_config_path);
		if (BLI_exists(workspace_config_path)) {
			has_path = true;
		}
	}

	return has_path ? BKE_blendfile_workspace_config_read(workspace_config_path, reports) : NULL;
}

static void workspace_append_button(
        uiLayout *layout, wmOperatorType *ot_append, const WorkSpace *workspace, const Main *from_main)
{
	const ID *id = (ID *)workspace;
	PointerRNA opptr;
	char lib_path[FILE_MAX_LIBEXTRA];

	BLI_path_join(
	        lib_path, sizeof(lib_path), from_main->name, BKE_idcode_to_name(GS(id->name)), NULL);

	BLI_assert(STREQ(ot_append->idname, "WORKSPACE_OT_append_activate"));
	uiItemFullO_ptr(
	        layout, ot_append, workspace->id.name + 2, ICON_NONE, NULL,
	        WM_OP_EXEC_DEFAULT, 0, &opptr);
	RNA_string_set(&opptr, "idname", id->name + 2);
	RNA_string_set(&opptr, "directory", lib_path);
}

ATTR_NONNULL(1, 2)
static void workspace_config_file_append_buttons(
        uiLayout *layout, const Main *bmain, ReportList *reports)
{
	WorkspaceConfigFileData *workspace_config = workspace_config_file_read(bmain, reports);

	if (workspace_config) {
		wmOperatorType *ot_append = WM_operatortype_find("WORKSPACE_OT_append_activate", true);

		for (WorkSpace *workspace = workspace_config->workspaces.first; workspace; workspace = workspace->id.next) {
			workspace_append_button(layout, ot_append, workspace, workspace_config->main);
		}

		BKE_blendfile_workspace_config_data_free(workspace_config);
	}
}

static int workspace_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	const Main *bmain = CTX_data_main(C);

	uiPopupMenu *pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
	uiLayout *layout = UI_popup_menu_layout(pup);

	uiItemO(layout, "Duplicate Current", ICON_NONE, "WORKSPACE_OT_workspace_duplicate");
	uiItemS(layout);
	workspace_config_file_append_buttons(layout, bmain, op->reports);

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

static void WORKSPACE_OT_workspace_add_menu(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Workspace";
	ot->description = "Add a new workspace by duplicating the current one or appending one "
	                  "from the user configuration";
	ot->idname = "WORKSPACE_OT_workspace_add_menu";

	/* api callbacks */
	ot->invoke = workspace_add_invoke;
}

void ED_operatortypes_workspace(void)
{
	WM_operatortype_append(WORKSPACE_OT_workspace_duplicate);
	WM_operatortype_append(WORKSPACE_OT_workspace_delete);
	WM_operatortype_append(WORKSPACE_OT_workspace_add_menu);
	WM_operatortype_append(WORKSPACE_OT_append_activate);
}

/** \} Workspace Operators */
