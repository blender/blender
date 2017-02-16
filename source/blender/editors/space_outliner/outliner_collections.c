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
 * Contributor(s): Blender Foundation, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_collections.c
 *  \ingroup spoutliner
 */

#include "BKE_context.h"
#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "BLI_listbase.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "outliner_intern.h" /* own include */

/* -------------------------------------------------------------------- */

static LayerCollection *outliner_collection_active(bContext *C)
{
	TODO_LAYER_OPERATORS;
	/* consider that we may have overrides or objects active
	 * leading to no active collections */
	return CTX_data_layer_collection(C);
}

static CollectionOverride *outliner_override_active(bContext *UNUSED(C))
{
	TODO_LAYER_OPERATORS;
	TODO_LAYER_OVERRIDE;
	return NULL;
}

/* -------------------------------------------------------------------- */
/* collection manager operators */

static int collection_link_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "OUTLINER_OT_collections_link not implemented yet");
	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_collection_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Collection";
	ot->idname = "OUTLINER_OT_collection_link";
	ot->description = "Link a new collection to the active layer";

	/* api callbacks */
	ot->invoke = collection_link_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Returns true if selected element is a collection directly
 * linked to the active SceneLayer (not a nested collection)
 */
static int collection_unlink_poll(bContext *C)
{
	LayerCollection *lc = outliner_collection_active(C);

	if (lc == NULL) {
		return 0;
	}

	SceneLayer *sl = CTX_data_scene_layer(C);
	return BLI_findindex(&sl->layer_collections, lc) != -1 ? 1 : 0;
}

static int collection_unlink_exec(bContext *C, wmOperator *op)
{
	LayerCollection *lc = outliner_collection_active(C);

	if (lc == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Active element is not a collection");
		return OPERATOR_CANCELLED;
	}

	SceneLayer *sl = CTX_data_scene_layer(C);
	BKE_collection_unlink(sl, lc);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Collection";
	ot->idname = "OUTLINER_OT_collection_unlink";
	ot->description = "Unlink collection from the active layer";

	/* api callbacks */
	ot->exec = collection_unlink_exec;
	ot->poll = collection_unlink_poll;

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

void OUTLINER_OT_collection_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Collection";
	ot->idname = "OUTLINER_OT_collection_new";
	ot->description = "Add a new collection to the scene, and link it to the active layer";

	/* api callbacks */
	ot->exec = collection_new_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Returns true is selected element is a collection
 */
static int collection_override_new_poll(bContext *(C))
{
#ifdef TODO_LAYER_OVERRIDE
	/* disable for now, since it's not implemented */
	(void) C;
	return 0;
#else
	return outliner_collection_active(C) ? 1 : 0;
#endif
}

static int collection_override_new_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	TODO_LAYER_OVERRIDE;
	BKE_report(op->reports, RPT_ERROR, "OUTLINER_OT_collections_override_new not implemented yet");
	return OPERATOR_CANCELLED;
}

/* in the middle of renames remove s */
void OUTLINER_OT_collection_override_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Override";
	ot->idname = "OUTLINER_OT_collection_override_new";
	ot->description = "Add a new override to the active collection";

	/* api callbacks */
	ot->invoke = collection_override_new_invoke;
	ot->poll = collection_override_new_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Returns true if selected element is a collection
 * or an override, but not a master collection
 */
static int collection_delete_poll(bContext *C)
{
	LayerCollection *lc = outliner_collection_active(C);

	if (lc == NULL) {
		/* try override */
		return outliner_override_active(C) ? 1 : 0;
	}

	return  (lc->scene_collection == BKE_collection_master(CTX_data_scene(C))) ? 0 : 1;
}

static int collection_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	LayerCollection *lc = outliner_collection_active(C);

	TODO_LAYER_OVERRIDE; /* handle operators */

	if (lc == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Active element is not a collection");
		return OPERATOR_CANCELLED;
	}

	if (lc->scene_collection == BKE_collection_master(scene)) {
		BKE_report(op->reports, RPT_ERROR, "You cannot delete the master collection, try unliking it instead");
		return OPERATOR_CANCELLED;
	}

	BKE_collection_remove(scene, lc->scene_collection);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->idname = "OUTLINER_OT_collection_delete";
	ot->description = "Delete active override or collection";

	/* api callbacks */
	ot->exec = collection_delete_exec;
	ot->poll = collection_delete_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_select_exec(bContext *C, wmOperator *op)
{
	SceneLayer *sl = CTX_data_scene_layer(C);
	const int collection_index = RNA_int_get(op->ptr, "collection_index");
	sl->active_collection = collection_index;
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->idname = "OUTLINER_OT_collection_select";
	ot->description = "Change active collection or override";

	/* api callbacks */
	ot->exec = collection_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "collection_index", 0, 0, INT_MAX, "Index",
	            "Index of collection to select", 0, INT_MAX);
}

/* -------------------------------------------------------------------- */

static int stubs_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(event))
{
	TODO_LAYER_OPERATORS;
	BKE_report(op->reports, RPT_ERROR, "Operator not implemented yet");
	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_collection_objects_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Objects";
	ot->idname = "OUTLINER_OT_collection_objects_add";
	ot->description = "Add selected objects to collection";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_objects_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Object";
	ot->idname = "OUTLINER_OT_collection_objects_remove";
	ot->description = "Remove objects from collection";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_objects_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Objects";
	ot->idname = "OUTLINER_OT_collection_objects_select";
	ot->description = "Select collection objects";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_objects_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Objects";
	ot->idname = "OUTLINER_OT_collection_objects_deselect";
	ot->description = "Deselect collection objects";

	/* api callbacks */
	ot->invoke = stubs_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
