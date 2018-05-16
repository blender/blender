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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_layer.c
 *  \ingroup RNA
 */

#include "DNA_scene_types.h"
#include "DNA_layer_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "ED_object.h"
#include "ED_render.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

const EnumPropertyItem rna_enum_collection_type_items[] = {
	{COLLECTION_TYPE_NONE, "NONE", 0, "Normal", ""},
	{COLLECTION_TYPE_GROUP_INTERNAL, "GROUP_INTERNAL", 0, "Group Internal", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static StructRNA *rna_SceneCollection_refine(PointerRNA *ptr)
{
	SceneCollection *scene_collection = (SceneCollection *)ptr->data;
	switch (scene_collection->type) {
		case COLLECTION_TYPE_GROUP_INTERNAL:
		case COLLECTION_TYPE_NONE:
			return &RNA_SceneCollection;
		default:
			BLI_assert(!"Collection type not fully implemented");
			break;
	}
	return &RNA_SceneCollection;
}

static void rna_SceneCollection_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	SceneCollection *sc = (SceneCollection *)ptr->data;
	BKE_collection_rename(&scene->id, sc, value);
}

static PointerRNA rna_SceneCollection_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a LinkData list */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ((LinkData *)internal->link)->data);
}

static int rna_SceneCollection_move_above(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_above(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_SceneCollection_move_below(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_below(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_SceneCollection_move_into(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_into(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static SceneCollection *rna_SceneCollection_duplicate(
        ID *id, SceneCollection *scene_collection, Main *bmain, bContext *C, ReportList *reports)
{
	if (scene_collection == BKE_collection_master(id)) {
		BKE_report(reports, RPT_ERROR, "The master collection can't be duplicated");
		return NULL;
	}

	SceneCollection *scene_collection_new = BKE_collection_duplicate(id, scene_collection);

	DEG_relations_tag_update(bmain);
	/* Don't use id here, since the layer collection may come from a group. */
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER, CTX_data_scene(C));

	return scene_collection_new;
}

static SceneCollection *rna_SceneCollection_new(
        ID *id, SceneCollection *sc_parent, Main *bmain, const char *name)
{
	SceneCollection *sc = BKE_collection_add(id, sc_parent, COLLECTION_TYPE_NONE, name);

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return sc;
}

static void rna_SceneCollection_remove(
        ID *id, SceneCollection *sc_parent, Main *bmain, ReportList *reports, PointerRNA *sc_ptr)
{
	SceneCollection *sc = sc_ptr->data;

	const int index = BLI_findindex(&sc_parent->scene_collections, sc);
	if (index == -1) {
		BKE_reportf(reports, RPT_ERROR, "Collection '%s' is not a sub-collection of '%s'",
		            sc->name, sc_parent->name);
		return;
	}

	if (!BKE_collection_remove(id, sc)) {
		BKE_reportf(reports, RPT_ERROR, "Collection '%s' could not be removed from collection '%s'",
		            sc->name, sc_parent->name);
		return;
	}

	RNA_POINTER_INVALIDATE(sc_ptr);

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
}

static int rna_SceneCollection_objects_active_index_get(PointerRNA *ptr)
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	return sc->active_object_index;
}

static void rna_SceneCollection_objects_active_index_set(PointerRNA *ptr, int value)
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	sc->active_object_index = value;
}

static void rna_SceneCollection_objects_active_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&sc->objects) - 1);
}

void rna_SceneCollection_object_link(
        ID *id, SceneCollection *sc, Main *bmain, ReportList *reports, Object *ob)
{
	Scene *scene = (Scene *)id;

	if (BLI_findptr(&sc->objects, ob, offsetof(LinkData, data))) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is already in collection '%s'", ob->id.name + 2, sc->name);
		return;
	}

	BKE_collection_object_add(&scene->id, sc, ob);

	/* TODO(sergey): Only update relations for the current scene. */
	DEG_relations_tag_update(bmain);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

	DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

static void rna_SceneCollection_object_unlink(
        ID *id, SceneCollection *sc, Main *bmain, ReportList *reports, Object *ob)
{
	Scene *scene = (Scene *)id;

	if (!BLI_findptr(&sc->objects, ob, offsetof(LinkData, data))) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is not in collection '%s'", ob->id.name + 2, sc->name);
		return;
	}

	BKE_collection_object_remove(bmain, &scene->id, sc, ob, false);

	/* needed otherwise the depgraph will contain freed objects which can crash, see [#20958] */
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

/***********************************/

static void rna_LayerCollection_name_get(PointerRNA *ptr, char *value)
{
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	strcpy(value, sc->name);
}

static int rna_LayerCollection_name_length(PointerRNA *ptr)
{
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	return strnlen(sc->name, sizeof(sc->name));
}

static void rna_LayerCollection_name_set(PointerRNA *ptr, const char *value)
{
	ID *owner_id = (ID *)ptr->id.data;
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	BKE_collection_rename(owner_id, sc, value);
}

static PointerRNA rna_LayerCollection_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Base *base = ((LinkData *)internal->link)->data;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_LayerCollection_move_above(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_above(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_LayerCollection_move_below(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_below(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_LayerCollection_move_into(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_into(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static void rna_LayerCollection_flag_update(bContext *C, PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
}

static Group *rna_LayerCollection_create_group(
        ID *id, LayerCollection *layer_collection, Main *bmain, bContext *C, ReportList *reports)
{
	Group *group;
	Scene *scene = (Scene *)id;
	SceneCollection *scene_collection = layer_collection->scene_collection;

	/* The master collection can't be converted. */
	if (scene_collection == BKE_collection_master(&scene->id)) {
		BKE_report(reports, RPT_ERROR, "The master collection can't be converted to group");
		return NULL;
	}

	group = BKE_collection_group_create(bmain, scene, layer_collection);
	if (group == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Failed to convert collection %s", scene_collection->name);
		return NULL;
	}

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);
	return group;
}

static LayerCollection *rna_LayerCollection_duplicate(
        ID *id, LayerCollection *layer_collection, Main *bmain, bContext *C, ReportList *reports)
{
	if (layer_collection->scene_collection == BKE_collection_master(id)) {
		BKE_report(reports, RPT_ERROR, "The master collection can't be duplicated");
		return NULL;
	}

	LayerCollection *layer_collection_new = BKE_layer_collection_duplicate(id, layer_collection);

	DEG_relations_tag_update(bmain);
	/* Don't use id here, since the layer collection may come from a group. */
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER, CTX_data_scene(C));

	return layer_collection_new;
}

static int rna_LayerCollections_active_collection_index_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return view_layer->active_collection;
}

static void rna_LayerCollections_active_collection_index_set(PointerRNA *ptr, int value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	int num_collections = BKE_layer_collection_count(view_layer);
	view_layer->active_collection = min_ff(value, num_collections - 1);
}

static void rna_LayerCollections_active_collection_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	*min = 0;
	*max = max_ii(0, BKE_layer_collection_count(view_layer) - 1);
}

static PointerRNA rna_LayerCollections_active_collection_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
	return rna_pointer_inherit_refine(ptr, &RNA_LayerCollection, lc);
}

static void rna_LayerCollections_active_collection_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = (LayerCollection *)value.data;
	const int index = BKE_layer_collection_findindex(view_layer, lc);
	if (index != -1) view_layer->active_collection = index;
}

LayerCollection * rna_ViewLayer_collection_link(
        ID *id, ViewLayer *view_layer, Main *bmain, SceneCollection *sc)
{
	Scene *scene = (Scene *)id;
	LayerCollection *lc = BKE_collection_link(view_layer, sc);

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, scene);

	return lc;
}

static void rna_ViewLayer_collection_unlink(
        ID *id, ViewLayer *view_layer, Main *bmain, ReportList *reports, LayerCollection *lc)
{
	Scene *scene = (Scene *)id;

	if (BLI_findindex(&view_layer->layer_collections, lc) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Layer collection '%s' is not in '%s'",
		            lc->scene_collection->name, view_layer->name);
		return;
	}

	BKE_collection_unlink(view_layer, lc);

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

static PointerRNA rna_LayerObjects_active_object_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, view_layer->basact ? view_layer->basact->object : NULL);
}

static void rna_LayerObjects_active_object_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	if (value.data)
		view_layer->basact = BKE_view_layer_base_find(view_layer, (Object *)value.data);
	else
		view_layer->basact = NULL;
}

static char *rna_ViewLayer_path(PointerRNA *ptr)
{
	ViewLayer *srl = (ViewLayer *)ptr->data;
	char name_esc[sizeof(srl->name) * 2];

	BLI_strescape(name_esc, srl->name, sizeof(name_esc));
	return BLI_sprintfN("view_layers[\"%s\"]", name_esc);
}

static IDProperty *rna_ViewLayer_idprops(PointerRNA *ptr, bool create)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;

	if (create && !view_layer->id_properties) {
		IDPropertyTemplate val = {0};
		view_layer->id_properties = IDP_New(IDP_GROUP, &val, "ViewLayer ID properties");
	}

	return view_layer->id_properties;
}

static void rna_ViewLayer_update_render_passes(ID *id)
{
	Scene *scene = (Scene *)id;
	if (scene->nodetree)
		ntreeCompositUpdateRLayers(scene->nodetree);
}

static PointerRNA rna_ViewLayer_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a ObjectBase list */
	Base *base = (Base *)internal->link;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_ViewLayer_objects_selected_skip(CollectionPropertyIterator *iter, void *UNUSED(data))
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Base *base = (Base *)internal->link;

	if ((base->flag & BASE_SELECTED) != 0) {
		return 0;
	}

	return 1;
};

static PointerRNA rna_ViewLayer_depsgraph_get(PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	if (GS(id->name) == ID_SCE) {
		Scene *scene = (Scene *)id;
		ViewLayer *view_layer = (ViewLayer *)ptr->data;
		Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, false);
		return rna_pointer_inherit_refine(ptr, &RNA_Depsgraph, depsgraph);
	}
	return PointerRNA_NULL;
}

static void rna_LayerObjects_selected_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &view_layer->object_bases, rna_ViewLayer_objects_selected_skip);
}

static void rna_ViewLayer_update_tagged(ViewLayer *UNUSED(view_layer), bContext *C)
{
	Depsgraph *graph = CTX_data_depsgraph(C);
	DEG_OBJECT_ITER_BEGIN(
	        graph, ob, DEG_ITER_OBJECT_MODE_VIEWPORT,
	        DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
	        DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
	        DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY |
	        DEG_ITER_OBJECT_FLAG_VISIBLE |
	        DEG_ITER_OBJECT_FLAG_DUPLI)
	{
		/* Don't do anything, we just need to run the iterator to flush
		 * the base info to the objects. */
		UNUSED_VARS(ob);
	}
	DEG_OBJECT_ITER_END;
}

static void rna_ObjectBase_select_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Base *base = (Base *)ptr->data;
	short mode = (base->flag & BASE_SELECTED) ? BA_SELECT : BA_DESELECT;
	ED_object_base_select(base, mode);
}

#else

static void rna_def_scene_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "SceneCollections");
	srna = RNA_def_struct(brna, "SceneCollections", NULL);
	RNA_def_struct_sdna(srna, "SceneCollection");
	RNA_def_struct_ui_text(srna, "Scene Collection", "Collection of scene collections");

	func = RNA_def_function(srna, "new", "rna_SceneCollection_new");
	RNA_def_function_ui_description(func, "Add a collection to scene");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_string(func, "name", NULL, 0, "", "New name for the collection (not unique)");
	parm = RNA_def_pointer(func, "result", "SceneCollection", "", "Newly created collection");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_SceneCollection_remove");
	RNA_def_function_ui_description(func, "Remove a collection and move its objects to the master collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "collection", "SceneCollection", "", "Collection to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_collection_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "CollectionObjects");
	srna = RNA_def_struct(brna, "CollectionObjects", NULL);
	RNA_def_struct_sdna(srna, "SceneCollection");
	RNA_def_struct_ui_text(srna, "Collection Objects", "Objects of a collection");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_SceneCollection_objects_active_index_get",
	                           "rna_SceneCollection_objects_active_index_set",
	                           "rna_SceneCollection_objects_active_index_range");
	RNA_def_property_ui_text(prop, "Active Object Index", "Active index in collection objects array");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	func = RNA_def_function(srna, "link", "rna_SceneCollection_object_link");
	RNA_def_function_ui_description(func, "Link an object to collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to add to collection");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	func = RNA_def_function(srna, "unlink", "rna_SceneCollection_object_unlink");
	RNA_def_function_ui_description(func, "Unlink object from collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to remove from collection");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_scene_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "SceneCollection", NULL);
	RNA_def_struct_ui_text(srna, "Scene Collection", "Collection");
	RNA_def_struct_ui_icon(srna, ICON_COLLAPSEMENU);
	RNA_def_struct_refine_func(srna, "rna_SceneCollection_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SceneCollection_name_set");
	RNA_def_property_ui_text(prop, "Name", "Collection name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_collection_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of collection");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "scene_collections", NULL);
	RNA_def_property_struct_type(prop, "SceneCollection");
	RNA_def_property_ui_text(prop, "SceneCollections", "");
	rna_def_scene_collections(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "objects", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_SceneCollection_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects directly added to this collection (not including sub-collection objects)");
	rna_def_collection_objects(brna, prop);

	/* Functions */
	func = RNA_def_function(srna, "move_above", "rna_SceneCollection_move_above");
	RNA_def_function_ui_description(func, "Move collection after another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Reference collection above which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_below", "rna_SceneCollection_move_below");
	RNA_def_function_ui_description(func, "Move collection before another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Reference collection below which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_into", "rna_SceneCollection_move_into");
	RNA_def_function_ui_description(func, "Move collection into another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Collection to insert into");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "duplicate", "rna_SceneCollection_duplicate");
	RNA_def_function_ui_description(func, "Create a copy of the collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "result", "SceneCollection", "", "Newly created collection");
	RNA_def_function_return(func, parm);
}

static void rna_def_layer_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "LayerCollection", NULL);
	RNA_def_struct_ui_text(srna, "Layer Collection", "Layer collection");
	RNA_def_struct_ui_icon(srna, ICON_COLLAPSEMENU);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_LayerCollection_name_get", "rna_LayerCollection_name_length", "rna_LayerCollection_name_set");
	RNA_def_property_ui_text(prop, "Name", "Collection name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "scene_collection");
	RNA_def_property_struct_type(prop, "SceneCollection");
	RNA_def_property_ui_text(prop, "Collection", "Collection this layer collection is wrapping");

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Layer Collections", "");

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_LayerCollection_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects directly or indirectly added to this collection (not including sub-collection objects)");

	/* Functions */
	func = RNA_def_function(srna, "move_above", "rna_LayerCollection_move_above");
	RNA_def_function_ui_description(func, "Move collection after another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Reference collection above which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_below", "rna_LayerCollection_move_below");
	RNA_def_function_ui_description(func, "Move collection before another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Reference collection below which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_into", "rna_LayerCollection_move_into");
	RNA_def_function_ui_description(func, "Move collection into another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Collection to insert into");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "create_group", "rna_LayerCollection_create_group");
	RNA_def_function_ui_description(func, "Enable or disable a collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "result", "Group", "", "Newly created Group");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "duplicate", "rna_LayerCollection_duplicate");
	RNA_def_function_ui_description(func, "Create a copy of the collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "result", "LayerCollection", "", "Newly created collection");
	RNA_def_function_return(func, parm);

	/* Flags */
	prop = RNA_def_property(srna, "selectable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_SELECTABLE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 1);
	RNA_def_property_ui_text(prop, "Selectable", "Restrict selection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");

	prop = RNA_def_property(srna, "visible_viewport", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_VIEWPORT);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_ui_text(prop, "Viewport Visibility", "");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");

	prop = RNA_def_property(srna, "visible_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_RENDER);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	RNA_def_property_ui_text(prop, "Render Visibility", "Control");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");

	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", COLLECTION_DISABLED);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Enabled", "Enable or disable collection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");
}

static void rna_def_layer_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "LayerCollections");
	srna = RNA_def_struct(brna, "LayerCollections", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Collections", "Collections of render layer");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "active_collection");
	RNA_def_property_int_funcs(prop, "rna_LayerCollections_active_collection_index_get",
	                           "rna_LayerCollections_active_collection_index_set",
	                           "rna_LayerCollections_active_collection_index_range");
	RNA_def_property_ui_text(prop, "Active Collection Index", "Active index in layer collection array");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_pointer_funcs(prop, "rna_LayerCollections_active_collection_get",
	                               "rna_LayerCollections_active_collection_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Layer Collection", "Active Layer Collection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	func = RNA_def_function(srna, "link", "rna_ViewLayer_collection_link");
	RNA_def_function_ui_description(func, "Link a collection to render layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "scene_collection", "SceneCollection", "", "Collection to add to render layer");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "result", "LayerCollection", "", "Newly created layer collection");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "unlink", "rna_ViewLayer_collection_unlink");
	RNA_def_function_ui_description(func, "Unlink a collection from render layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer_collection", "LayerCollection", "", "Layer collection to remove from render layer");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_layer_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "LayerObjects");
	srna = RNA_def_struct(brna, "LayerObjects", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Objects", "Collections of objects");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_LayerObjects_active_object_get", "rna_LayerObjects_active_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Object", "Active object for this layer");
	/* Could call: ED_object_base_activate(C, rl->basact);
	 * but would be a bad level call and it seems the notifier is enough */
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);

	prop = RNA_def_property(srna, "selected", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, "rna_LayerObjects_selected_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_ViewLayer_objects_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Objects", "All the selected objects of this layer");
}

static void rna_def_object_base(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ObjectBase", NULL);
	RNA_def_struct_sdna(srna, "Base");
	RNA_def_struct_ui_text(srna, "Object Base", "An object instance in a render layer");
	RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Object this base links to");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BASE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "Object base selection state");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_select_update");
}

void RNA_def_view_layer(BlenderRNA *brna)
{
	FunctionRNA *func;
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ViewLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "Render layer");
	RNA_def_struct_ui_icon(srna, ICON_RENDER_RESULT);
	RNA_def_struct_path_func(srna, "rna_ViewLayer_path");
	RNA_def_struct_idprops_func(srna, "rna_ViewLayer_idprops");

	rna_def_view_layer_common(srna, 1);

	func = RNA_def_function(srna, "update_render_passes", "rna_ViewLayer_update_render_passes");
	RNA_def_function_ui_description(func, "Requery the enabled render passes from the render engine");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Layer Collections", "");
	rna_def_layer_collections(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_ViewLayer_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects in this layer");
	rna_def_layer_objects(brna, prop);

	/* layer options */
	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_RENDER);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_FREESTYLE);
	RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	/* Freestyle */
	rna_def_freestyle_settings(brna);

	prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "freestyle_config");
	RNA_def_property_struct_type(prop, "FreestyleSettings");
	RNA_def_property_ui_text(prop, "Freestyle Settings", "");

	/* debug update routine */
	func = RNA_def_function(srna, "update", "rna_ViewLayer_update_tagged");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func,
	                                "Update data tagged to be updated from previous access to data or operators");

	/* Dependency Graph */
	prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Depsgraph");
	RNA_def_property_ui_text(prop, "Dependency Graph", "Dependencies in the scene data");
	RNA_def_property_pointer_funcs(prop, "rna_ViewLayer_depsgraph_get", NULL, NULL, NULL);

	/* Nested Data  */
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_scene_collection(brna);
	rna_def_layer_collection(brna);
	rna_def_object_base(brna);
	RNA_define_animate_sdna(true);
	/* *** Animated *** */
}

#endif
