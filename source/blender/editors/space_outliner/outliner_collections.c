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

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"

#include "outliner_intern.h" /* own include */

/* -------------------------------------------------------------------- */

static LayerCollection *outliner_collection_active(bContext *C)
{
	TODO_LAYER_OPERATORS;
	/* consider that we may have overrides or objects active
	 * leading to no active collections */
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

#if 0
static CollectionOverride *outliner_override_active(bContext *UNUSED(C))
{
	TODO_LAYER_OPERATORS;
	TODO_LAYER_OVERRIDE;
	return NULL;
}
#endif

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
	SceneCollection *sc;
	TreeElement *te;
} TreeElementFindData;

static TreeTraversalAction tree_element_find_by_scene_collection(TreeElement *te, void *customdata)
{
	TreeElementFindData *data = customdata;

	SceneCollection *current_element_sc = outliner_scene_collection_from_tree_element(te);

	if (current_element_sc == data->sc) {
		data->te = te;
		return TRAVERSE_BREAK;
	}

	return TRAVERSE_CONTINUE;
}

static TreeElement *outliner_tree_element_from_layer_collection(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);

	LayerCollection *lc = BKE_layer_collection_from_index(view_layer, 0);

	if (lc == NULL) {
		return NULL;
	}

	/* Find the tree element containing the LayerCollection's scene_collection. */
	TreeElementFindData data = {
		.sc = lc->scene_collection,
		.te = NULL,
	};
	outliner_tree_traverse(soops, &soops->tree, 0, 0, tree_element_find_by_scene_collection, &data);

	return data.te;
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
	ot->name = "Add Collection";
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
	ot->name = "Add Collection";
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
	SceneCollection *scene_collection = BKE_collection_add(&scene->id, NULL, COLLECTION_TYPE_NONE, NULL);
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
	}

	return TRAVERSE_CONTINUE;
}

static int collection_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	struct CollectionDeleteData data = {.scene = scene, .soops = soops};

	data.collections_to_delete = BLI_gset_ptr_new(__func__);

	TODO_LAYER_OVERRIDE; /* handle overrides */

	/* We first walk over and find the SceneCollections we actually want to delete (ignoring duplicates). */
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_delete, &data);

	/* Effectively delete the collections. */
	GSetIterator collections_to_delete_iter;
	GSET_ITER(collections_to_delete_iter, data.collections_to_delete) {

		SceneCollection *sc = BLI_gsetIterator_getKey(&collections_to_delete_iter);
		BKE_collection_remove(&data.scene->id, sc);
	}

	BLI_gset_free(data.collections_to_delete, NULL);

	/* Rebuild the outliner tree before we select the tree element */
	outliner_build_tree(bmain, scene, view_layer, soops);

	TreeElement *select_te = outliner_tree_element_from_layer_collection(C);

	if (select_te) {
		outliner_item_select(soops, select_te, false, false);
	}

	DEG_relations_tag_update(CTX_data_main(C));

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

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
