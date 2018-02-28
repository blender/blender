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

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"

#include "outliner_intern.h" /* own include */

/* Prototypes. */
static int collection_delete_exec(struct bContext *C, struct wmOperator *op);

/* -------------------------------------------------------------------- */

static LayerCollection *outliner_collection_active(bContext *C)
{
	return CTX_data_layer_collection(C);
}

SceneCollection *outliner_scene_collection_from_tree_element(TreeElement *te)
{
	TreeStoreElem *tselem = TREESTORE(te);

	if (tselem->type == TSE_SCENE_COLLECTION) {
		return te->directdata;
	}
	else if (tselem->type == TSE_LAYER_COLLECTION) {
		LayerCollection *lc = te->directdata;
		return lc->scene_collection;
	}

	return NULL;
}

/* -------------------------------------------------------------------- */
/* Poll functions. */

static int collections_editor_poll(bContext *C)
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	return (so != NULL) && (so->outlinevis == SO_COLLECTIONS);
}

static int view_layer_editor_poll(bContext *C)
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	return (so != NULL) && (so->outlinevis == SO_VIEW_LAYER);
}

static int outliner_either_collection_editor_poll(bContext *C)
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	return (so != NULL) && (ELEM(so->outlinevis, SO_VIEW_LAYER, SO_COLLECTIONS));
}

static int outliner_objects_collection_poll(bContext *C)
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	if (so == NULL) {
		return 0;
	}

	/* Groups don't support filtering. */
	if ((so->outlinevis != SO_GROUPS) &&
	    ((so->filter & (SO_FILTER_ENABLE | SO_FILTER_NO_COLLECTION)) ==
	    (SO_FILTER_ENABLE | SO_FILTER_NO_COLLECTION)))
	{
		return 0;
	}

	return ELEM(so->outlinevis, SO_VIEW_LAYER, SO_COLLECTIONS, SO_GROUPS);
}

/* -------------------------------------------------------------------- */
/* collection manager operators */

/**
 * Recursively get the collection for a given index
 */
static SceneCollection *scene_collection_from_index(ListBase *lb, const int number, int *i)
{
	for (SceneCollection *sc = lb->first; sc; sc = sc->next) {
		if (*i == number) {
			return sc;
		}

		(*i)++;

		SceneCollection *sc_nested = scene_collection_from_index(&sc->scene_collections, number, i);
		if (sc_nested) {
			return sc_nested;
		}
	}
	return NULL;
}

typedef struct TreeElementFindData {
	SceneCollection *collection;
	TreeElement *r_result_te;
} TreeElementFindData;

static TreeTraversalAction tree_element_find_by_scene_collection_cb(TreeElement *te, void *customdata)
{
	TreeElementFindData *data = customdata;
	const SceneCollection *current_element_sc = outliner_scene_collection_from_tree_element(te);

	if (current_element_sc == data->collection) {
		data->r_result_te = te;
		return TRAVERSE_BREAK;
	}

	return TRAVERSE_CONTINUE;
}

static TreeElement *outliner_tree_element_from_layer_collection_index(
        SpaceOops *soops, ViewLayer *view_layer,
        const int index)
{
	LayerCollection *lc = BKE_layer_collection_from_index(view_layer, index);

	if (lc == NULL) {
		return NULL;
	}

	/* Find the tree element containing the LayerCollection's scene_collection. */
	TreeElementFindData data = {
		.collection = lc->scene_collection,
		.r_result_te = NULL,
	};
	outliner_tree_traverse(soops, &soops->tree, 0, 0, tree_element_find_by_scene_collection_cb, &data);

	return data.r_result_te;
}

static int collection_link_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SceneCollection *sc_master = BKE_collection_master(&scene->id);
	SceneCollection *sc;

	int scene_collection_index = RNA_enum_get(op->ptr, "scene_collection");
	if (scene_collection_index == 0) {
		sc = sc_master;
	}
	else {
		int index = 1;
		sc = scene_collection_from_index(&sc_master->scene_collections, scene_collection_index, &index);
		BLI_assert(sc);
	}

	BKE_collection_link(view_layer, sc);

	DEG_relations_tag_update(CTX_data_main(C));

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

static int collection_link_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	SceneCollection *master_collection = BKE_collection_master(&scene->id);
	if (master_collection->scene_collections.first == NULL) {
		RNA_enum_set(op->ptr, "scene_collection", 0);
		return collection_link_exec(C, op);
	}
	else {
		return WM_enum_search_invoke(C, op, event);
	}
}

static void collection_scene_collection_itemf_recursive(
        EnumPropertyItem *tmp, EnumPropertyItem **item, int *totitem, int *value, SceneCollection *sc)
{
	tmp->value = *value;
	tmp->icon = ICON_COLLAPSEMENU;
	tmp->identifier = sc->name;
	tmp->name = sc->name;
	RNA_enum_item_add(item, totitem, tmp);

	(*value)++;

	for (SceneCollection *ncs = sc->scene_collections.first; ncs; ncs = ncs->next) {
		collection_scene_collection_itemf_recursive(tmp, item, totitem, value, ncs);
	}
}

static const EnumPropertyItem *collection_scene_collection_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item = NULL;
	int value = 0, totitem = 0;

	Scene *scene = CTX_data_scene(C);
	SceneCollection *sc = BKE_collection_master(&scene->id);

	collection_scene_collection_itemf_recursive(&tmp, &item, &totitem, &value, sc);
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OUTLINER_OT_collection_link(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Link Collection";
	ot->idname = "OUTLINER_OT_collection_link";
	ot->description = "Link a new collection to the active layer";

	/* api callbacks */
	ot->exec = collection_link_exec;
	ot->invoke = collection_link_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_enum(ot->srna, "scene_collection", DummyRNA_NULL_items, 0, "Scene Collection", "");
	RNA_def_enum_funcs(prop, collection_scene_collection_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/**
 * Returns true if selected element is a collection directly
 * linked to the active ViewLayer (not a nested collection)
 */
static int collection_unlink_poll(bContext *C)
{
	if (view_layer_editor_poll(C) == 0) {
		return 0;
	}

	LayerCollection *lc = outliner_collection_active(C);

	if (lc == NULL) {
		return 0;
	}

	ViewLayer *view_layer = CTX_data_view_layer(C);
	return BLI_findindex(&view_layer->layer_collections, lc) != -1 ? 1 : 0;
}

static int collection_unlink_exec(bContext *C, wmOperator *op)
{
	LayerCollection *lc = outliner_collection_active(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);

	if (lc == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Active element is not a collection");
		return OPERATOR_CANCELLED;
	}

	ViewLayer *view_layer = CTX_data_view_layer(C);
	BKE_collection_unlink(view_layer, lc);

	if (soops) {
		outliner_cleanup_tree(soops);
	}

	DEG_relations_tag_update(CTX_data_main(C));

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&CTX_data_scene(C)->id, 0);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink Collection";
	ot->idname = "OUTLINER_OT_collection_unlink";
	ot->description = "Unlink collection from the active layer";

	/* api callbacks */
	ot->exec = collection_unlink_exec;
	ot->poll = collection_unlink_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**********************************************************************************/
/* Add new collection. */

static int collection_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SceneCollection *scene_collection_parent = BKE_collection_master(&scene->id);
	SceneCollection *scene_collection = BKE_collection_add(&scene->id, scene_collection_parent, COLLECTION_TYPE_NONE, NULL);
	BKE_collection_link(view_layer, scene_collection);

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Collection";
	ot->idname = "OUTLINER_OT_collection_new";
	ot->description = "Add a new collection to the scene";

	/* api callbacks */
	ot->exec = collection_new_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**********************************************************************************/
/* Add new nested collection. */

struct CollectionNewData
{
	bool error;
	SceneCollection *scene_collection;
};

static TreeTraversalAction collection_find_selected_to_add(TreeElement *te, void *customdata)
{
	struct CollectionNewData *data = customdata;
	SceneCollection *scene_collection = outliner_scene_collection_from_tree_element(te);

	if (!scene_collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	if (data->scene_collection != NULL) {
		data->error = true;
		return TRAVERSE_BREAK;
	}

	data->scene_collection = scene_collection;
	return TRAVERSE_CONTINUE;
}

static int collection_nested_new_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	struct CollectionNewData data = {
		.error = false,
		.scene_collection = NULL,
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_selected_to_add, &data);

	if (data.error) {
		BKE_report(op->reports, RPT_ERROR, "More than one collection is selected");
		return OPERATOR_CANCELLED;
	}

	BKE_collection_add(
	            &scene->id,
	            data.scene_collection,
	            COLLECTION_TYPE_NONE,
	            NULL);

	outliner_cleanup_tree(soops);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_nested_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Nested Collection";
	ot->idname = "OUTLINER_OT_collection_nested_new";
	ot->description = "Add a new collection inside selected collection";

	/* api callbacks */
	ot->exec = collection_nested_new_exec;
	ot->poll = collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**********************************************************************************/
/* Delete selected collection. */

void OUTLINER_OT_collection_delete_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Selected Collections";
	ot->idname = "OUTLINER_OT_collection_delete_selected";
	ot->description = "Delete all the selected collections";

	/* api callbacks */
	ot->exec = collection_delete_exec;
	ot->poll = collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**********************************************************************************/
/* Add new selected objects. */

struct SceneCollectionSelectedData {
	ListBase scene_collections_array;
};

static TreeTraversalAction collection_find_selected_scene_collections(TreeElement *te, void *customdata)
{
	struct SceneCollectionSelectedData *data = customdata;
	SceneCollection *scene_collection = outliner_scene_collection_from_tree_element(te);

	if (!scene_collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	BLI_addtail(&data->scene_collections_array, BLI_genericNodeN(scene_collection));
	return TRAVERSE_CONTINUE;
}

static int collection_objects_add_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	struct SceneCollectionSelectedData data = {
		.scene_collections_array = {NULL, NULL},
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_selected_scene_collections, &data);

	if (BLI_listbase_is_empty(&data.scene_collections_array)) {
		BKE_report(op->reports, RPT_ERROR, "No collection is selected");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN (C, struct Object *, ob, selected_objects)
	{
		LISTBASE_FOREACH (LinkData *, link, &data.scene_collections_array) {
			SceneCollection *scene_collection = link->data;
			BKE_collection_object_add(
			            &scene->id,
			            scene_collection,
			            ob);
		}
	}
	CTX_DATA_END;
	BLI_freelistN(&data.scene_collections_array);

	outliner_cleanup_tree(soops);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_objects_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Objects";
	ot->idname = "OUTLINER_OT_collection_objects_add";
	ot->description = "Add selected objects to collection";

	/* api callbacks */
	ot->exec = collection_objects_add_exec;
	ot->poll = collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**********************************************************************************/
/* Remove selected objects. */


static int collection_objects_remove_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	struct SceneCollectionSelectedData data = {
		.scene_collections_array = {NULL, NULL},
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_selected_scene_collections, &data);

	if (BLI_listbase_is_empty(&data.scene_collections_array)) {
		BKE_report(op->reports, RPT_ERROR, "No collection is selected");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN (C, struct Object *, ob, selected_objects)
	{
		LISTBASE_FOREACH (LinkData *, link, &data.scene_collections_array) {
			SceneCollection *scene_collection = link->data;
			BKE_collection_object_remove(
			            bmain,
			            &scene->id,
			            scene_collection,
			            ob,
			            true);
		}
	}
	CTX_DATA_END;
	BLI_freelistN(&data.scene_collections_array);

	outliner_cleanup_tree(soops);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_objects_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Objects";
	ot->idname = "OUTLINER_OT_collection_objects_remove";
	ot->description = "Remove selected objects from collection";

	/* api callbacks */
	ot->exec = collection_objects_remove_exec;
	ot->poll = collections_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static TreeElement *outliner_collection_parent_element_get(TreeElement *te)
{
	TreeElement *te_parent = te;
	while ((te_parent = te_parent->parent)) {
		if (outliner_scene_collection_from_tree_element(te->parent)) {
			return te_parent;
		}
	}
	return NULL;
}

static int object_collection_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Main *bmain = CTX_data_main(C);

	struct ObjectsSelectedData data = {
		.objects_selected_array = {NULL, NULL},
	};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &data);

	LISTBASE_FOREACH (LinkData *, link, &data.objects_selected_array) {
		TreeElement *te = (TreeElement *)link->data;
		Object *ob = (Object *)TREESTORE(te)->id;
		SceneCollection *scene_collection = NULL;

		TreeElement *te_parent = outliner_collection_parent_element_get(te);
		if (te_parent != NULL) {
			scene_collection = outliner_scene_collection_from_tree_element(te_parent);
			ID *owner_id = TREESTORE(te_parent)->id;
			BKE_collection_object_remove(bmain, owner_id, scene_collection, ob, true);
			DEG_id_tag_update(owner_id, DEG_TAG_BASE_FLAGS_UPDATE);
		}
	}

	BLI_freelistN(&data.objects_selected_array);

	outliner_cleanup_tree(soops);
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
	WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, NULL);
	WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_object_remove_from_collection(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Object from Collection";
	ot->idname = "OUTLINER_OT_object_remove_from_collection";
	ot->description = "Remove selected objects from their respective collection";

	/* api callbacks */
	ot->exec = object_collection_remove_exec;
	ot->poll = outliner_objects_collection_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_add_to_new_collection_exec(bContext *C, wmOperator *op)
{
	int operator_result = OPERATOR_CANCELLED;

	SpaceOops *soops = CTX_wm_space_outliner(C);
	Main *bmain = CTX_data_main(C);

	SceneCollection *scene_collection_parent, *scene_collection_new;
	TreeElement *te_active, *te_parent;

	struct ObjectsSelectedData data = {{NULL}}, active = {{NULL}};

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_HIGHLIGHTED, outliner_find_selected_objects, &active);
	if (BLI_listbase_is_empty(&active.objects_selected_array)) {
		BKE_report(op->reports, RPT_ERROR, "No object is selected");
		goto cleanup;
	}

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &data);
	if (BLI_listbase_is_empty(&data.objects_selected_array)) {
		BKE_report(op->reports, RPT_ERROR, "No objects are selected");
		goto cleanup;
	}

	/* Heuristic to get the "active" / "last object" */
	te_active = ((LinkData *)active.objects_selected_array.first)->data;
	te_parent = outliner_collection_parent_element_get(te_active);

	if (te_parent == NULL) {
		BKE_reportf(op->reports, RPT_ERROR, "Couldn't find collection of \"%s\" object", te_active->name);
		goto cleanup;
	}

	ID *owner_id = TREESTORE(te_parent)->id;
	scene_collection_parent = outliner_scene_collection_from_tree_element(te_parent);
	scene_collection_new = BKE_collection_add(owner_id, scene_collection_parent, scene_collection_parent->type, NULL);

	LISTBASE_FOREACH (LinkData *, link, &data.objects_selected_array) {
		TreeElement *te = (TreeElement *)link->data;
		Object *ob = (Object *)TREESTORE(te)->id;
		BKE_collection_object_add(owner_id, scene_collection_new, ob);
	}

	outliner_cleanup_tree(soops);
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	operator_result = OPERATOR_FINISHED;
cleanup:
	BLI_freelistN(&active.objects_selected_array);
	BLI_freelistN(&data.objects_selected_array);
	return operator_result;
}

void OUTLINER_OT_object_add_to_new_collection(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Objects to New Collection";
	ot->idname = "OUTLINER_OT_object_add_to_new_collection";
	ot->description = "Add objects to a new collection";

	/* api callbacks */
	ot->exec = object_add_to_new_collection_exec;
	ot->poll = outliner_objects_collection_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

struct CollectionDeleteData {
	Scene *scene;
	SpaceOops *soops;
	GSet *collections_to_delete;
};

static TreeTraversalAction collection_find_data_to_delete(TreeElement *te, void *customdata)
{
	struct CollectionDeleteData *data = customdata;
	SceneCollection *scene_collection = outliner_scene_collection_from_tree_element(te);

	if (!scene_collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	if (scene_collection == BKE_collection_master(&data->scene->id)) {
		/* skip - showing warning/error message might be missleading
		 * when deleting multiple collections, so just do nothing */
	}
	else {
		BLI_gset_add(data->collections_to_delete, scene_collection);
		return TRAVERSE_SKIP_CHILDS; /* Childs will be gone anyway, no need to recurse deeper. */
	}

	return TRAVERSE_CONTINUE;
}

static TreeTraversalAction collection_delete_elements_from_collection(TreeElement *te, void *customdata)
{
	struct CollectionDeleteData *data = customdata;
	SceneCollection *scene_collection = outliner_scene_collection_from_tree_element(te);

	if (!scene_collection) {
		return TRAVERSE_SKIP_CHILDS;
	}

	const bool will_be_deleted = BLI_gset_haskey(data->collections_to_delete, scene_collection);
	if (will_be_deleted) {
		outliner_free_tree_element(te, te->parent ? &te->parent->subtree : &data->soops->tree);
		/* Childs are freed now, so don't recurse into them. */
		return TRAVERSE_SKIP_CHILDS;
	}

	return TRAVERSE_CONTINUE;
}

static int collection_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionDeleteData data = {.scene = scene, .soops = soops};

	data.collections_to_delete = BLI_gset_ptr_new(__func__);

	/* We first walk over and find the SceneCollections we actually want to delete (ignoring duplicates). */
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_delete, &data);

	/* Now, delete all tree elements representing a collection that will be deleted. We'll look for a
	 * new element to select in a few lines, so we can't wait until the tree is recreated on redraw. */
	outliner_tree_traverse(soops, &soops->tree, 0, 0, collection_delete_elements_from_collection, &data);

	/* Effectively delete the collections. */
	GSetIterator collections_to_delete_iter;
	GSET_ITER(collections_to_delete_iter, data.collections_to_delete) {
		SceneCollection *sc = BLI_gsetIterator_getKey(&collections_to_delete_iter);
		BKE_collection_remove(&data.scene->id, sc);
	}

	BLI_gset_free(data.collections_to_delete, NULL);

	TreeElement *select_te = outliner_tree_element_from_layer_collection_index(soops, CTX_data_view_layer(C), 0);
	if (select_te) {
		outliner_item_select(soops, select_te, false, false);
	}

	DEG_relations_tag_update(CTX_data_main(C));

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

	soops->storeflag |= SO_TREESTORE_REDRAW;
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collections_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->idname = "OUTLINER_OT_collections_delete";
	ot->description = "Delete selected overrides or collections";

	/* api callbacks */
	ot->exec = collection_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_select_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	const int collection_index = RNA_int_get(op->ptr, "collection_index");
	view_layer->active_collection = collection_index;
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

#define ACTION_DISABLE 0
#define ACTION_ENABLE 1
#define ACTION_TOGGLE 2

static int collection_toggle_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int action = RNA_enum_get(op->ptr, "action");
	LayerCollection *layer_collection = CTX_data_layer_collection(C);

	if (layer_collection->flag & COLLECTION_DISABLED) {
		if (ELEM(action, ACTION_TOGGLE, ACTION_ENABLE)) {
			layer_collection->flag &= ~COLLECTION_DISABLED;
		}
		else { /* ACTION_DISABLE */
			BKE_reportf(op->reports, RPT_ERROR, "Layer collection %s already disabled",
			            layer_collection->scene_collection->name);
			return OPERATOR_CANCELLED;
		}
	}
	else {
		if (ELEM(action, ACTION_TOGGLE, ACTION_DISABLE)) {
			layer_collection->flag |= COLLECTION_DISABLED;
		}
		else { /* ACTION_ENABLE */
			BKE_reportf(op->reports, RPT_ERROR, "Layer collection %s already enabled",
			            layer_collection->scene_collection->name);
			return OPERATOR_CANCELLED;
		}
	}

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_toggle(wmOperatorType *ot)
{
	PropertyRNA *prop;

	static EnumPropertyItem actions_items[] = {
		{ACTION_DISABLE, "DISABLE", 0, "Disable", "Disable selected markers"},
		{ACTION_ENABLE, "ENABLE", 0, "Enable", "Enable selected markers"},
		{ACTION_TOGGLE, "TOGGLE", 0, "Toggle", "Toggle disabled flag for selected markers"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Toggle Collection";
	ot->idname = "OUTLINER_OT_collection_toggle";
	ot->description = "Deselect collection objects";

	/* api callbacks */
	ot->exec = collection_toggle_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_int(ot->srna, "collection_index", -1, -1, INT_MAX, "Collection Index", "Index of collection to toggle", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_enum(ot->srna, "action", actions_items, ACTION_TOGGLE, "Action", "Selection action to execute");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#undef ACTION_TOGGLE
#undef ACTION_ENABLE
#undef ACTION_DISABLE

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
		case TSE_LAYER_COLLECTION_BASE:
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

static int collection_objects_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	LayerCollection *layer_collection = outliner_active_layer_collection(C);

	if (layer_collection == NULL) {
		return OPERATOR_CANCELLED;
	}

	BKE_layer_collection_objects_select(layer_collection);
	WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_objects_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Objects";
	ot->idname = "OUTLINER_OT_collection_objects_select";
	ot->description = "Select all the collection objects";

	/* api callbacks */
	ot->exec = collection_objects_select_exec;
	ot->poll = view_layer_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

struct CollectionDuplicateData {
	TreeElement *te;
};

static TreeTraversalAction outliner_find_first_selected_collection(TreeElement *te, void *customdata)
{
	struct CollectionDuplicateData *data = customdata;
	TreeStoreElem *tselem = TREESTORE(te);

	switch (tselem->type) {
		case TSE_LAYER_COLLECTION:
		case TSE_SCENE_COLLECTION:
			data->te = te;
			return TRAVERSE_BREAK;
		case TSE_LAYER_COLLECTION_BASE:
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
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = outliner_active_collection(C);

	BLI_assert(te != NULL);
	if (BKE_collection_master(TREESTORE(te)->id) == outliner_scene_collection_from_tree_element(te)) {
		BKE_report(op->reports, RPT_ERROR, "You can't duplicate the master collection");
		return OPERATOR_CANCELLED;
	}

	switch (soops->outlinevis) {
		case SO_COLLECTIONS:
			BKE_collection_duplicate(TREESTORE(te)->id, (SceneCollection *)te->directdata);
			break;
		case SO_VIEW_LAYER:
		case SO_GROUPS:
			BKE_layer_collection_duplicate(TREESTORE(te)->id, (LayerCollection *)te->directdata);
			break;
	}

	DEG_relations_tag_update(CTX_data_main(C));
	WM_main_add_notifier(NC_SCENE | ND_LAYER, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Collection";
	ot->idname = "OUTLINER_OT_collection_duplicate";
	ot->description = "Duplicate collection";

	/* api callbacks */
	ot->exec = collection_duplicate_exec;
	ot->poll = outliner_either_collection_editor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
