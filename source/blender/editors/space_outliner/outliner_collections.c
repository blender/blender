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

#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"

#include "outliner_intern.h" /* own include */

/* -------------------------------------------------------------------- */

bool outliner_is_collection_tree_element(const TreeElement *te)
{
	TreeStoreElem *tselem = TREESTORE(te);

	if (!tselem) {
		return false;
	}

	if (ELEM(tselem->type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
		return true;
	}
	else if (tselem->type == 0 && te->idcode == ID_GR) {
		return true;
	}

	return false;
}

Collection *outliner_collection_from_tree_element(const TreeElement *te)
{
	TreeStoreElem *tselem = TREESTORE(te);

	if (!tselem) {
		return false;
	}

	if (tselem->type == TSE_LAYER_COLLECTION) {
		LayerCollection *lc = te->directdata;
		return lc->collection;
	}
	else if (ELEM(tselem->type, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
		Scene *scene = (Scene *)tselem->id;
		return BKE_collection_master(scene);
	}
	else if (tselem->type == 0 && te->idcode == ID_GR) {
		return (Collection *)tselem->id;
	}

	return NULL;
}

/* -------------------------------------------------------------------- */
/* Poll functions. */

bool ED_outliner_collections_editor_poll(bContext *C)
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	return (so != NULL) && ELEM(so->outlinevis, SO_VIEW_LAYER, SO_SCENES, SO_LIBRARIES);
}


/********************************* New Collection ****************************/

struct CollectionNewData
{
	bool error;
	Collection *collection;
};

static TreeTraversalAction collection_find_selected_to_add(TreeElement *te, void *customdata)
{
	struct CollectionNewData *data = customdata;
	Collection *collection = outliner_collection_from_tree_element(te);

	if (!collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	if (data->collection != NULL) {
		data->error = true;
		return TRAVERSE_BREAK;
	}

	data->collection = collection;
	return TRAVERSE_CONTINUE;
}

static int collection_new_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);

	struct CollectionNewData data = {
		.error = false,
		.collection = NULL,
	};

	if (RNA_boolean_get(op->ptr, "nested")) {
		outliner_build_tree(bmain, scene, view_layer, soops, ar);

		outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_selected_to_add, &data);

		if (data.error) {
			BKE_report(op->reports, RPT_ERROR, "More than one collection is selected");
			return OPERATOR_CANCELLED;
		}
	}

	if (!data.collection && (soops->outlinevis == SO_VIEW_LAYER)) {
		data.collection = BKE_collection_master(scene);
	}

	BKE_collection_add(
	            bmain,
	            data.collection,
	            NULL);

	DEG_id_tag_update(&data.collection->id, DEG_TAG_COPY_ON_WRITE);
	DEG_relations_tag_update(bmain);

	outliner_cleanup_tree(soops);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Collection";
	ot->idname = "OUTLINER_OT_collection_new";
	ot->description = "Add a new collection inside selected collection";

	/* api callbacks */
	ot->exec = collection_new_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	PropertyRNA *prop = RNA_def_boolean(ot->srna, "nested", true, "Nested", "Add as child of selected collection");;
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/**************************** Delete Collection ******************************/

struct CollectionEditData {
	Scene *scene;
	SpaceOops *soops;
	GSet *collections_to_edit;
};

static TreeTraversalAction collection_find_data_to_edit(TreeElement *te, void *customdata)
{
	struct CollectionEditData *data = customdata;
	Collection *collection = outliner_collection_from_tree_element(te);

	if (!collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	if (collection == BKE_collection_master(data->scene)) {
		/* skip - showing warning/error message might be missleading
		 * when deleting multiple collections, so just do nothing */
	}
	else {
		/* Delete, duplicate and link don't edit children, those will come along
		 * with the parents. */
		BLI_gset_add(data->collections_to_edit, collection);
		return TRAVERSE_SKIP_CHILDS;
	}

	return TRAVERSE_CONTINUE;
}

static int collection_delete_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionEditData data = {.scene = scene, .soops = soops};
	bool hierarchy = RNA_boolean_get(op->ptr, "hierarchy");

	data.collections_to_edit = BLI_gset_ptr_new(__func__);

	/* We first walk over and find the Collections we actually want to delete (ignoring duplicates). */
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

	/* Effectively delete the collections. */
	GSetIterator collections_to_edit_iter;
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

		/* Test in case collection got deleted as part of another one. */
		if (BLI_findindex(&bmain->collection, collection) != -1) {
			BKE_collection_delete(bmain, collection, hierarchy);
		}
	}

	BLI_gset_free(data.collections_to_edit, NULL);

	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Collection";
	ot->idname = "OUTLINER_OT_collection_delete";
	ot->description = "Delete selected collections";

	/* api callbacks */
	ot->exec = collection_delete_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	PropertyRNA *prop = RNA_def_boolean(ot->srna, "hierarchy", false, "Hierarchy", "Delete child objects and collections");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/****************************** Select Objects *******************************/

struct CollectionObjectsSelectData {
	bool error;
	LayerCollection *layer_collection;
};

static TreeTraversalAction outliner_find_first_selected_layer_collection(TreeElement *te, void *customdata)
{
	struct CollectionObjectsSelectData *data = customdata;
	TreeStoreElem *tselem = TREESTORE(te);

	switch (tselem->type) {
		case TSE_LAYER_COLLECTION:
			data->layer_collection = te->directdata;
			return TRAVERSE_BREAK;
		case TSE_R_LAYER:
		case TSE_SCENE_COLLECTION_BASE:
		case TSE_VIEW_COLLECTION_BASE:
			return TRAVERSE_CONTINUE;
		default:
			return TRAVERSE_SKIP_CHILDS;
	}
}

static LayerCollection *outliner_active_layer_collection(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);

	struct CollectionObjectsSelectData data = {
		.layer_collection = NULL,
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_first_selected_layer_collection, &data);
	return data.layer_collection;
}

static int collection_objects_select_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	LayerCollection *layer_collection = outliner_active_layer_collection(C);
	bool deselect = STREQ(op->idname, "OUTLINER_OT_collection_objects_deselect");

	if (layer_collection == NULL) {
		return OPERATOR_CANCELLED;
	}

	BKE_layer_collection_objects_select(view_layer, layer_collection, deselect);

	Scene *scene = CTX_data_scene(C);
	DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
	WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_objects_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Objects";
	ot->idname = "OUTLINER_OT_collection_objects_select";
	ot->description = "Select objects in collection";

	/* api callbacks */
	ot->exec = collection_objects_select_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_objects_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Objects";
	ot->idname = "OUTLINER_OT_collection_objects_deselect";
	ot->description = "Deselect objects in collection";

	/* api callbacks */
	ot->exec = collection_objects_select_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Duplicate Collection *****************************/

struct CollectionDuplicateData {
	TreeElement *te;
};

static TreeTraversalAction outliner_find_first_selected_collection(TreeElement *te, void *customdata)
{
	struct CollectionDuplicateData *data = customdata;
	TreeStoreElem *tselem = TREESTORE(te);

	switch (tselem->type) {
		case TSE_LAYER_COLLECTION:
			data->te = te;
			return TRAVERSE_BREAK;
		case TSE_R_LAYER:
		case TSE_SCENE_COLLECTION_BASE:
		case TSE_VIEW_COLLECTION_BASE:
		default:
			return TRAVERSE_CONTINUE;
	}
}

static TreeElement *outliner_active_collection(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);

	struct CollectionDuplicateData data = {
		.te = NULL,
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_first_selected_collection, &data);
	return data.te;
}

static int collection_duplicate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = outliner_active_collection(C);
	BLI_assert(te != NULL);

	Collection *collection = outliner_collection_from_tree_element(te);
	Collection *parent = (te->parent) ? outliner_collection_from_tree_element(te->parent) : NULL;

	if (collection->flag & COLLECTION_IS_MASTER) {
		BKE_report(op->reports, RPT_ERROR, "Can't duplicate the master collection");
		return OPERATOR_CANCELLED;
	}

	switch (soops->outlinevis) {
		case SO_SCENES:
		case SO_VIEW_LAYER:
		case SO_LIBRARIES:
			BKE_collection_copy(bmain, parent, collection);
			break;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Collection";
	ot->idname = "OUTLINER_OT_collection_duplicate";
	ot->description = "Duplicate selected collections";

	/* api callbacks */
	ot->exec = collection_duplicate_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**************************** Link Collection ******************************/

static int collection_link_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Collection *active_collection = CTX_data_layer_collection(C)->collection;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionEditData data = {.scene = scene, .soops = soops};

	data.collections_to_edit = BLI_gset_ptr_new(__func__);

	/* We first walk over and find the Collections we actually want to link (ignoring duplicates). */
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

	/* Effectively link the collections. */
	GSetIterator collections_to_edit_iter;
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
		BKE_collection_child_add(bmain, active_collection, collection);
		id_fake_user_clear(&collection->id);
	}

	BLI_gset_free(data.collections_to_edit, NULL);

	DEG_id_tag_update(&active_collection->id, DEG_TAG_COPY_ON_WRITE);
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Link Collection";
	ot->idname = "OUTLINER_OT_collection_link";
	ot->description = "Link selected collections to active scene";

	/* api callbacks */
	ot->exec = collection_link_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Instance Collection ******************************/

static int collection_instance_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionEditData data = {.scene = scene, .soops = soops};

	data.collections_to_edit = BLI_gset_ptr_new(__func__);

	/* We first walk over and find the Collections we actually want to instance (ignoring duplicates). */
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

	/* Find an active collection to add to, that doesn't give dependency cycles. */
	LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);

	GSetIterator collections_to_edit_iter;
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

		while (BKE_collection_find_cycle(active_lc->collection, collection)) {
			active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
		}
	}

	/* Effectively instance the collections. */
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
		Object *ob = ED_object_add_type(C, OB_EMPTY, collection->id.name + 2, scene->cursor.location, NULL, false, scene->layact);
		ob->dup_group = collection;
		ob->transflag |= OB_DUPLICOLLECTION;
		id_lib_extern(&collection->id);
	}

	BLI_gset_free(data.collections_to_edit, NULL);

	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_instance(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Instance Collection";
	ot->idname = "OUTLINER_OT_collection_instance";
	ot->description = "Instance selected collections to active scene";

	/* api callbacks */
	ot->exec = collection_instance_exec;
	ot->poll = ED_outliner_collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Exclude Collection ******************************/

static TreeTraversalAction layer_collection_find_data_to_edit(TreeElement *te, void *customdata)
{
	struct CollectionEditData *data = customdata;
	TreeStoreElem *tselem = TREESTORE(te);

	if (!(tselem && tselem->type == TSE_LAYER_COLLECTION)) {
		return TRAVERSE_CONTINUE;
	}

	LayerCollection *lc = te->directdata;

	if (lc->collection->flag & COLLECTION_IS_MASTER) {
		/* skip - showing warning/error message might be missleading
		 * when deleting multiple collections, so just do nothing */
	}
	else {
		/* Delete, duplicate and link don't edit children, those will come along
		 * with the parents. */
		BLI_gset_add(data->collections_to_edit, lc);
	}

	return TRAVERSE_CONTINUE;
}

static bool collections_view_layer_poll(bContext *C, bool clear, int flag)
{
	/* Poll function so the right click menu show current state of selected collections. */
	SpaceOops *soops = CTX_wm_space_outliner(C);
	if (!(soops && soops->outlinevis == SO_VIEW_LAYER)) {
		return false;
	}

	Scene *scene = CTX_data_scene(C);
	struct CollectionEditData data = {.scene = scene, .soops = soops};
	data.collections_to_edit = BLI_gset_ptr_new(__func__);
	bool result = false;

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

	GSetIterator collections_to_edit_iter;
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		LayerCollection *lc = BLI_gsetIterator_getKey(&collections_to_edit_iter);

		if (clear && (lc->flag & flag)) {
			result = true;
		}
		else if (!clear && !(lc->flag & flag)) {
			result = true;
		}
	}

	BLI_gset_free(data.collections_to_edit, NULL);
	return result;
}

static bool collections_exclude_set_poll(bContext *C)
{
	return collections_view_layer_poll(C, false, LAYER_COLLECTION_EXCLUDE);
}

static bool collections_exclude_clear_poll(bContext *C)
{
	return collections_view_layer_poll(C, true, LAYER_COLLECTION_EXCLUDE);
}

static bool collections_holdout_set_poll(bContext *C)
{
	return collections_view_layer_poll(C, false, LAYER_COLLECTION_HOLDOUT);
}

static bool collections_holdout_clear_poll(bContext *C)
{
	return collections_view_layer_poll(C, true, LAYER_COLLECTION_HOLDOUT);
}

static bool collections_indirect_only_set_poll(bContext *C)
{
	return collections_view_layer_poll(C, false, LAYER_COLLECTION_INDIRECT_ONLY);
}

static bool collections_indirect_only_clear_poll(bContext *C)
{
	return collections_view_layer_poll(C, true, LAYER_COLLECTION_INDIRECT_ONLY);
}

static void layer_collection_flag_recursive_set(LayerCollection *lc, int flag)
{
	for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
		if (lc->flag & flag) {
			nlc->flag |= flag;
		}
		else {
			nlc->flag &= ~flag;
		}

		layer_collection_flag_recursive_set(nlc, flag);
	}
}

static int collection_view_layer_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionEditData data = {.scene = scene, .soops = soops};
	bool clear = strstr(op->idname, "clear") != NULL;
	int flag = strstr(op->idname, "holdout") ?       LAYER_COLLECTION_HOLDOUT :
	           strstr(op->idname, "indirect_only") ? LAYER_COLLECTION_INDIRECT_ONLY :
	                                                 LAYER_COLLECTION_EXCLUDE;

	data.collections_to_edit = BLI_gset_ptr_new(__func__);

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

	GSetIterator collections_to_edit_iter;
	GSET_ITER(collections_to_edit_iter, data.collections_to_edit) {
		LayerCollection *lc = BLI_gsetIterator_getKey(&collections_to_edit_iter);

		if (!(lc->collection->flag & COLLECTION_IS_MASTER)) {
			if (clear) {
				lc->flag &= ~flag;
			}
			else {
				lc->flag |= flag;
			}

			layer_collection_flag_recursive_set(lc, flag);
		}
	}

	BLI_gset_free(data.collections_to_edit, NULL);

	BKE_layer_collection_sync(scene, view_layer);
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_exclude_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Exclude";
	ot->idname = "OUTLINER_OT_collection_exclude_set";
	ot->description = "Exclude collection from the active view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_exclude_set_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_exclude_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Exclude";
	ot->idname = "OUTLINER_OT_collection_exclude_clear";
	ot->description = "Include collection in the active view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_exclude_clear_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_holdout_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Holdout";
	ot->idname = "OUTLINER_OT_collection_holdout_set";
	ot->description = "Mask collection in the active view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_holdout_set_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_holdout_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Holdout";
	ot->idname = "OUTLINER_OT_collection_holdout_clear";
	ot->description = "Clear masking of collection in the active view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_holdout_clear_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_indirect_only_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Indirect Only";
	ot->idname = "OUTLINER_OT_collection_indirect_only_set";
	ot->description = "Set collection to only contribute indirectly (through shadows and reflections) in the view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_indirect_only_set_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_indirect_only_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Indirect Only";
	ot->idname = "OUTLINER_OT_collection_indirect_only_clear";
	ot->description = "Clear collection contributing only indirectly in the view layer";

	/* api callbacks */
	ot->exec = collection_view_layer_exec;
	ot->poll = collections_indirect_only_clear_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Populates the \param objects ListBase with all the outliner selected objects
 * We store it as (Object *)LinkData->data
 * \param objects expected to be empty
 */
void ED_outliner_selected_objects_get(const bContext *C, ListBase *objects)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct ObjectsSelectedData data = {{NULL}};
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &data);
	LISTBASE_FOREACH (LinkData *, link, &data.objects_selected_array) {
		TreeElement *ten_selected = (TreeElement *)link->data;
		Object *ob = (Object *)TREESTORE(ten_selected)->id;
		BLI_addtail(objects, BLI_genericNodeN(ob));
	}
	BLI_freelistN(&data.objects_selected_array);
}
