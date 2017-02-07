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

/** \file blender/editors/space_collections/collections_ops.c
 *  \ingroup spcollections
 */

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "collections_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/* polls */

static SceneCollection *collection_manager_collection_active(bContext *C)
{
	TODO_LAYER_OPERATORS;
	/* consider that we may have overrides active
	 * leading to no active collections */
	return CTX_data_scene_collection(C);
}

static int operator_not_master_collection_active(bContext *C)
{
	SceneCollection *sc = collection_manager_collection_active(C);
	if (sc == NULL) {
		return 1;
	}

	return  (sc == BKE_collection_master(CTX_data_scene(C))) ? 0 : 1;
}

static int operator_top_collection_active(bContext *C)
{
	SceneCollection *sc = collection_manager_collection_active(C);
	if (sc == NULL) {
		return 0;
	}

	TODO_LAYER_OPERATORS;
	/* see if it's a top collection */
	return 1;
}

static int operator_collection_active(bContext *C)
{
	return collection_manager_collection_active(C) ? 1 : 0;
}

/* -------------------------------------------------------------------- */
/* collection manager operators */

static int collection_link_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "COLLECTIONS_OT_collection_link not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_collection_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Collection";
	ot->idname = "COLLECTIONS_OT_collection_link";
	ot->description = "Link a new collection to the active layer";

	/* api callbacks */
	ot->invoke = collection_link_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_unlink_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "COLLECTIONS_OT_collection_unlink not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_collection_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Collection";
	ot->idname = "COLLECTIONS_OT_collection_unlink";
	ot->description = "Link a new collection to the active layer";

	/* api callbacks */
	ot->invoke = collection_unlink_invoke;
	ot->poll = operator_top_collection_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneLayer *sl = CTX_data_scene_layer(C);

	SceneCollection *sc = BKE_collection_add(scene, NULL, NULL);
	BKE_collection_link(sl, sc);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

static void COLLECTIONS_OT_collection_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Collection";
	ot->idname = "COLLECTIONS_OT_collection_new";
	ot->description = "Add a new collection to the scene, and link it to the active layer";

	/* api callbacks */
	ot->exec = collection_new_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int override_new_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	TODO_LAYER_OVERRIDE;
	BKE_report(op->reports, RPT_ERROR, "COLLECTIONS_OT_override_new not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_override_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Override";
	ot->idname = "COLLECTIONS_OT_override_new";
	ot->description = "Add a new override to the active collection";

	/* api callbacks */
	ot->invoke = override_new_invoke;
	ot->poll = operator_collection_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int delete_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "COLLECTIONS_OT_delete not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->idname = "COLLECTIONS_OT_delete";
	ot->description = "Delete active  override or collection";

	/* api callbacks */
	ot->invoke = delete_invoke;
	ot->poll = operator_not_master_collection_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_exec(bContext *C, wmOperator *op)
{
	SceneLayer *sl = CTX_data_scene_layer(C);
	const int collection_index = RNA_int_get(op->ptr, "collection_index");
	sl->active_collection = collection_index;
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

static void COLLECTIONS_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->idname = "COLLECTIONS_OT_select";
	ot->description = "Change active collection or override";

	/* api callbacks */
	ot->exec = select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "collection_index", 0, 0, INT_MAX, "Index",
	            "Index of collection to select", 0, INT_MAX);
}

static int rename_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "COLLECTIONS_rename not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_rename(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rename";
	ot->idname = "COLLECTIONS_OT_rename";
	ot->description = "Rename active collection or override";

	/* api callbacks */
	ot->invoke = rename_invoke;
	ot->poll = operator_not_master_collection_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */
/* property editor operators */

static int stubs_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "Operator not implemented yet");
	return OPERATOR_CANCELLED;
}

static void COLLECTIONS_OT_objects_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Objects";
	ot->idname = "COLLECTIONS_OT_objects_add";
	ot->description = "Add selected objects to collection";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void COLLECTIONS_OT_objects_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Object";
	ot->idname = "COLLECTIONS_OT_objects_remove";
	ot->description = "Remove object from collection";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void COLLECTIONS_OT_objects_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Objects";
	ot->idname = "COLLECTIONS_OT_objects_select";
	ot->description = "Selected collection objects";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void COLLECTIONS_OT_objects_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Objects";
	ot->idname = "COLLECTIONS_OT_objects_deselect";
	ot->description = "Deselected collection objects";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************** registration - operator types **********************************/

void collections_operatortypes(void)
{
	WM_operatortype_append(COLLECTIONS_OT_delete);
	WM_operatortype_append(COLLECTIONS_OT_select);
	WM_operatortype_append(COLLECTIONS_OT_rename);
	WM_operatortype_append(COLLECTIONS_OT_collection_link);
	WM_operatortype_append(COLLECTIONS_OT_collection_unlink);
	WM_operatortype_append(COLLECTIONS_OT_collection_new);
	WM_operatortype_append(COLLECTIONS_OT_override_new);

	WM_operatortype_append(COLLECTIONS_OT_objects_add);
	WM_operatortype_append(COLLECTIONS_OT_objects_remove);
	WM_operatortype_append(COLLECTIONS_OT_objects_select);
	WM_operatortype_append(COLLECTIONS_OT_objects_deselect);
}

void collections_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Collections Manager", SPACE_COLLECTIONS, 0);

	/* selection */
	WM_keymap_add_item(keymap, "COLLECTIONS_OT_select", LEFTMOUSE, KM_CLICK, 0, 0);

	WM_keymap_add_item(keymap, "COLLECTIONS_OT_rename", LEFTMOUSE, KM_DBL_CLICK, 0, 0);
	WM_keymap_add_item(keymap, "COLLECTIONS_OT_rename", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "COLLECTIONS_OT_collection_new", NKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "COLLECTIONS_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "COLLECTIONS_OT_delete", DELKEY, KM_PRESS, 0, 0);
}
