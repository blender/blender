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

/** \file blender/editors/scene/scene_edit.c
 *  \ingroup edscene
 */

#include <stdio.h>

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BLI_compiler_attrs.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "DNA_workspace_types.h"

#include "ED_object.h"
#include "ED_render.h"
#include "ED_scene.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"


Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
{
	Scene *scene_new;

	if (method == SCE_COPY_NEW) {
		scene_new = BKE_scene_add(bmain, DATA_("Scene"));
	}
	else { /* different kinds of copying */
		Scene *scene_old = WM_window_get_active_scene(win);

		scene_new = BKE_scene_copy(bmain, scene_old, method);

		/* these can't be handled in blenkernel currently, so do them here */
		if (method == SCE_COPY_LINK_DATA) {
			ED_object_single_users(bmain, scene_new, false, true);
		}
		else if (method == SCE_COPY_FULL) {
			ED_editors_flush_edits(C, false);
			ED_object_single_users(bmain, scene_new, true, true);
		}
	}

	WM_window_change_active_scene(bmain, C, win, scene_new);

	WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene_new);

	return scene_new;
}

/**
 * \note Only call outside of area/region loops
 * \return true if successful
 */
bool ED_scene_delete(bContext *C, Main *bmain, wmWindow *win, Scene *scene)
{
	Scene *scene_new;

	if (scene->id.prev)
		scene_new = scene->id.prev;
	else if (scene->id.next)
		scene_new = scene->id.next;
	else
		return false;

	WM_window_change_active_scene(bmain, C, win, scene_new);

	BKE_libblock_remap(bmain, scene, scene_new, ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);

	id_us_clear_real(&scene->id);
	if (scene->id.us == 0) {
		BKE_libblock_free(bmain, scene);
	}

	return true;
}

void ED_scene_exit(bContext *C)
{
	ED_object_editmode_exit(C, EM_FREEDATA | EM_DO_UNDO);
}

void ED_scene_changed_update(Main *bmain, bContext *C, Scene *scene_new, const bScreen *active_screen)
{
	/* XXX Just using active scene render-layer for workspace when switching,
	 * but workspace should remember the last one set. Could store render-layer
	 * per window-workspace combination (using WorkSpaceDataRelation) */
	SceneLayer *layer_new = BLI_findlink(&scene_new->render_layers, scene_new->active_layer);
	Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene_new, layer_new);
	/* TODO(sergey): This is a temporary solution. */
	if (depsgraph == NULL) {
		scene_new->depsgraph_legacy = depsgraph = DEG_graph_new();
	}

	CTX_data_scene_set(C, scene_new);
	BKE_workspace_render_layer_set(CTX_wm_workspace(C), layer_new);
	BKE_scene_set_background(bmain, scene_new);
	DEG_graph_relations_update(depsgraph, bmain, scene_new);
	DEG_on_visible_update(bmain, false);

	ED_screen_update_after_scene_change(active_screen, scene_new);
	ED_render_engine_changed(bmain);
	ED_update_for_newframe(bmain, scene_new, depsgraph);

	/* complete redraw */
	WM_event_add_notifier(C, NC_WINDOW, NULL);
}

static bool scene_render_layer_remove_poll(
        const Scene *scene, const SceneLayer *layer)
{
	const int act = BLI_findindex(&scene->render_layers, layer);

	if (act == -1) {
		return false;
	}
	else if ((scene->render_layers.first == scene->render_layers.last) &&
	         (scene->render_layers.first == layer))
	{
		/* ensure 1 layer is kept */
		return false;
	}

	return true;
}

static void scene_render_layer_remove_unset_nodetrees(const Main *bmain, Scene *scene, SceneLayer *layer)
{
	int act_layer_index = BLI_findindex(&scene->render_layers, layer);

	for (Scene *sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			BKE_nodetree_remove_layer_n(sce->nodetree, scene, act_layer_index);
		}
	}
}

bool ED_scene_render_layer_delete(
        Main *bmain, Scene *scene, SceneLayer *layer,
        ReportList *reports)
{
	if (scene_render_layer_remove_poll(scene, layer) == false) {
		if (reports) {
			BKE_reportf(reports, RPT_ERROR, "Render layer '%s' could not be removed from scene '%s'",
			            layer->name, scene->id.name + 2);
		}

		return false;
	}

	BLI_remlink(&scene->render_layers, layer);
	BLI_assert(BLI_listbase_is_empty(&scene->render_layers) == false);
	scene->active_layer = 0;

	ED_workspace_render_layer_unset(bmain, layer, scene->render_layers.first);
	scene_render_layer_remove_unset_nodetrees(bmain, scene, layer);

	BKE_scene_layer_free(layer);

	DEG_id_tag_update(&scene->id, 0);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER | NA_REMOVED, scene);

	return true;
}

static int scene_new_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	int type = RNA_enum_get(op->ptr, "type");

	ED_scene_add(bmain, C, win, type);

	return OPERATOR_FINISHED;
}

static void SCENE_OT_new(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{SCE_COPY_NEW, "NEW", 0, "New", "Add new scene"},
		{SCE_COPY_EMPTY, "EMPTY", 0, "Copy Settings", "Make a copy without any objects"},
		{SCE_COPY_LINK_OB, "LINK_OBJECTS", 0, "Link Objects", "Link to the objects from the current scene"},
		{SCE_COPY_LINK_DATA, "LINK_OBJECT_DATA", 0, "Link Object Data", "Copy objects linked to data from the current scene"},
		{SCE_COPY_FULL, "FULL_COPY", 0, "Full Copy", "Make a full copy of the current scene"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "New Scene";
	ot->description = "Add new scene by type";
	ot->idname = "SCENE_OT_new";

	/* api callbacks */
	ot->exec = scene_new_exec;
	ot->invoke = WM_menu_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

static int scene_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	if (ED_scene_delete(C, CTX_data_main(C), CTX_wm_window(C), scene) == false) {
		return OPERATOR_CANCELLED;
	}

	if (G.debug & G_DEBUG)
		printf("scene delete %p\n", scene);

	WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, scene);

	return OPERATOR_FINISHED;
}

static void SCENE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Scene";
	ot->description = "Delete active scene";
	ot->idname = "SCENE_OT_delete";

	/* api callbacks */
	ot->exec = scene_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_operatortypes_scene(void)
{
	WM_operatortype_append(SCENE_OT_new);
	WM_operatortype_append(SCENE_OT_delete);
}
