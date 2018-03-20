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

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "screen_intern.h"


/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */

WorkSpace *ED_workspace_add(
        Main *bmain, const char *name, Scene *scene,
        ViewLayer *act_view_layer, ViewRender *view_render)
{
	WorkSpace *workspace = BKE_workspace_add(bmain, name);

	BKE_workspace_view_layer_set(workspace, act_view_layer, scene);
	BKE_viewrender_copy(&workspace->view_render, view_render);

	return workspace;
}

static void workspace_change_update_view_layer(
        WorkSpace *workspace_new, const WorkSpace *workspace_old,
        Scene *scene)
{
	if (!BKE_workspace_view_layer_get(workspace_new, scene)) {
		BKE_workspace_view_layer_set(workspace_new, BKE_workspace_view_layer_get(workspace_old, scene), scene);
	}
}

static void workspace_change_update(
        WorkSpace *workspace_new, const WorkSpace *workspace_old,
        bContext *C)
{
	/* needs to be done before changing mode! (to ensure right context) */
	workspace_change_update_view_layer(workspace_new, workspace_old, CTX_data_scene(C));
}

static bool workspace_change_find_new_layout_cb(const WorkSpaceLayout *layout, void *UNUSED(arg))
{
	/* return false to stop the iterator if we've found a layout that can be activated */
	return workspace_layout_set_poll(layout) ? false : true;
}

static WorkSpaceLayout *workspace_change_get_new_layout(
        WorkSpace *workspace_new, wmWindow *win)
{
	/* ED_workspace_duplicate may have stored a layout to activate once the workspace gets activated. */
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
			/* fallback solution: duplicate layout */
			layout_temp = ED_workspace_layout_duplicate(workspace_new, layout_new, win);
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
        WorkSpace *workspace_new, bContext *C, wmWindow *win)
{
	Main *bmain = CTX_data_main(C);
	WorkSpace *workspace_old = WM_window_get_active_workspace(win);
	WorkSpaceLayout *layout_new = workspace_change_get_new_layout(workspace_new, win);
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
		Scene *scene = WM_window_get_active_scene(win);
		bool use_object_mode = false;

		/* Store old context for exiting edit-mode. */
		EvaluationContext eval_ctx_old;
		CTX_data_eval_ctx(C, &eval_ctx_old);


		WM_window_set_active_layout(win, workspace_new, layout_new);
		WM_window_set_active_workspace(win, workspace_new);

		/* update screen *after* changing workspace - which also causes the actual screen change */
		screen_change_update(C, win, screen_new);
		workspace_change_update(workspace_new, workspace_old, C);

		BLI_assert(BKE_workspace_view_layer_get(workspace_new, CTX_data_scene(C)) != NULL);
		BLI_assert(CTX_wm_workspace(C) == workspace_new);

		WM_toolsystem_unlink(C, workspace_old);
		WM_toolsystem_link(C, workspace_new);

		ViewLayer *view_layer_old = BKE_workspace_view_layer_get(workspace_old, scene);
		Object *obact_old = OBACT(view_layer_old);

		ViewLayer *view_layer_new = BKE_workspace_view_layer_get(workspace_new, scene);
		Object *obact_new = OBACT(view_layer_new);

		/* Handle object mode switching */
		if ((workspace_old->object_mode != OB_MODE_OBJECT) ||
		    (workspace_new->object_mode != OB_MODE_OBJECT))
		{
			if ((workspace_old->object_mode == workspace_new->object_mode) &&
			    (obact_old == obact_new))
			{
				/* pass */
			}
			else {
				use_object_mode = true;
			}
		}

		if (use_object_mode) {
			/* weak, set it back so it's used when activating again. */
			eObjectMode object_mode = workspace_old->object_mode;
			ED_object_mode_generic_exit_or_other_window(&eval_ctx_old, bmain->wm.first, workspace_old, scene, obact_old);
			workspace_old->object_mode = object_mode;
			ED_workspace_object_mode_sync_from_object(bmain->wm.first, workspace_old, obact_old);
			ED_object_mode_generic_enter_or_other_window(C, NULL, workspace_new->object_mode);
		}
		else {
			if (obact_new == NULL) {
				workspace_new->object_mode = OB_MODE_OBJECT;
			}
		}

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
	Scene *scene = WM_window_get_active_scene(win);
	WorkSpace *workspace_new = ED_workspace_add(
	        bmain, workspace_old->id.name + 2, scene,
	        BKE_workspace_view_layer_get(workspace_old, scene),
	        &workspace_old->view_render);
	ListBase *transform_orientations_old = BKE_workspace_transform_orientations_get(workspace_old);
	ListBase *transform_orientations_new = BKE_workspace_transform_orientations_get(workspace_new);

	BLI_duplicatelist(transform_orientations_new, transform_orientations_old);

	workspace_new->tool = workspace_old->tool;
	workspace_new->object_mode = workspace_old->object_mode;

	for (WorkSpaceLayout *layout_old = layouts_old->first; layout_old; layout_old = layout_old->next) {
		WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(workspace_new, layout_old, win);

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

		ED_workspace_change((prev != NULL) ? prev : next, C, win);
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

void ED_workspace_view_layer_unset(
        const Main *bmain, Scene *scene,
        const ViewLayer *layer_unset, ViewLayer *layer_new)
{
	for (WorkSpace *workspace = bmain->workspaces.first; workspace; workspace = workspace->id.next) {
		if (BKE_workspace_view_layer_get(workspace, scene) == layer_unset) {
			BKE_workspace_view_layer_set(workspace, layer_new, scene);
		}
	}
}

/**
 * When a work-space mode has changed,
 * flush it to all other visible work-spaces using the same object
 * since we don't support one object being in two different modes at once.
 * \note We could support this but it's more trouble than it's worth.
 */

void ED_workspace_object_mode_sync_from_object(wmWindowManager *wm, WorkSpace *workspace, Object *obact)
{
	if (obact == NULL) {
		return;
	}
	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		WorkSpace *workspace_iter = BKE_workspace_active_get(win->workspace_hook);
		if ((workspace != workspace_iter) && (workspace->object_mode != workspace_iter->object_mode)) {
			Scene *scene_iter = WM_window_get_active_scene(win);
			ViewLayer *view_layer = BKE_view_layer_from_workspace_get(scene_iter, workspace_iter);
			if (obact == OBACT(view_layer)) {
				workspace_iter->object_mode = workspace->object_mode;
				/* TODO(campbell), use msgbus */
				WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene_iter);
			}
		}
	}
}

void ED_workspace_object_mode_sync_from_scene(wmWindowManager *wm, WorkSpace *workspace, Scene *scene)
{
	ViewLayer *view_layer = BKE_workspace_view_layer_get(workspace, scene);
	if (view_layer) {
		Object *obact = OBACT(view_layer);
		ED_workspace_object_mode_sync_from_object(wm, workspace, obact);
	}
}

bool ED_workspace_object_mode_in_other_window(
        struct wmWindowManager *wm, const wmWindow *win_compare, Object *obact,
        eObjectMode *r_object_mode)
{
	for (wmWindow *win_iter = wm->windows.first; win_iter; win_iter = win_iter->next) {
		if (win_compare != win_iter) {
			WorkSpace *workspace_iter = BKE_workspace_active_get(win_iter->workspace_hook);
			Scene *scene_iter = WM_window_get_active_scene(win_iter);
			ViewLayer *view_layer_iter = BKE_view_layer_from_workspace_get(scene_iter, workspace_iter);
			Object *obact_iter = OBACT(view_layer_iter);
			if (obact == obact_iter) {
				if (r_object_mode) {
					*r_object_mode = workspace_iter->object_mode;
				}
				return true;
			}
		}
	}

	return false;
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
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);

	ED_workspace_delete(WM_window_get_active_workspace(win), bmain, C, wm);

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
		workspace_config_file_path_from_folder_id(bmain, BLENDER_DATAFILES, workspace_config_path);
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

	BLI_assert(STREQ(ot_append->idname, "WM_OT_append"));
	uiItemFullO_ptr(
	        layout, ot_append, workspace->id.name + 2, ICON_NONE, NULL,
	        WM_OP_EXEC_DEFAULT, 0, &opptr);
	RNA_string_set(&opptr, "directory", lib_path);
	RNA_string_set(&opptr, "filename", id->name + 2);
	RNA_boolean_set(&opptr, "autoselect", false);
}

ATTR_NONNULL(1, 2)
static void workspace_config_file_append_buttons(
        uiLayout *layout, const Main *bmain, ReportList *reports)
{
	WorkspaceConfigFileData *workspace_config = workspace_config_file_read(bmain, reports);

	if (workspace_config) {
		wmOperatorType *ot_append = WM_operatortype_find("WM_OT_append", true);

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
}

/** \} Workspace Operators */
