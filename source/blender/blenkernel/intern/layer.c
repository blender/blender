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
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/layer.c
 *  \ingroup bke
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"

#include "DNA_group_types.h"
#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

/* prototype */
struct EngineSettingsCB_Type;
static void layer_collections_sync_flags(ListBase *layer_collections_dst, const ListBase *layer_collections_src);
static void layer_collection_free(ViewLayer *view_layer, LayerCollection *lc);
static void layer_collection_objects_populate(ViewLayer *view_layer, LayerCollection *lc, ListBase *objects);
static LayerCollection *layer_collection_add(ViewLayer *view_layer, LayerCollection *parent, SceneCollection *sc);
static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc, const SceneCollection *sc);
static IDProperty *collection_engine_settings_create(struct EngineSettingsCB_Type *ces_type, const bool populate);
static IDProperty *collection_engine_get(IDProperty *root, const int type, const char *engine_name);
static void collection_engine_settings_init(IDProperty *root, const bool populate);
static void layer_engine_settings_init(IDProperty *root, const bool populate);
static void object_bases_iterator_next(BLI_Iterator *iter, const int flag);

/* RenderLayer */

/**
 * Returns the ViewLayer to be used for rendering
 * Most of the time BKE_view_layer_from_workspace_get should be used instead
 */
ViewLayer *BKE_view_layer_from_scene_get(const Scene *scene)
{
	ViewLayer *view_layer = BLI_findlink(&scene->view_layers, scene->active_view_layer);
	BLI_assert(view_layer);
	return view_layer;
}

/**
 * Returns the ViewLayer to be used for drawing, outliner, and other context related areas.
 */
ViewLayer *BKE_view_layer_from_workspace_get(const struct Scene *scene, const struct WorkSpace *workspace)
{
	if (BKE_workspace_use_scene_settings_get(workspace)) {
		return BKE_view_layer_from_scene_get(scene);
	}
	else {
		return BKE_workspace_view_layer_get(workspace, scene);
	}
}

/**
 * This is a placeholder to know which areas of the code need to be addressed for the Workspace changes.
 * Never use this, you should either use BKE_view_layer_from_workspace_get or get ViewLayer explicitly.
 */
ViewLayer *BKE_view_layer_context_active_PLACEHOLDER(const Scene *scene)
{
	return BKE_view_layer_from_scene_get(scene);
}

static ViewLayer *view_layer_add(const char *name, SceneCollection *master_scene_collection)
{
	if (!name) {
		name = DATA_("View Layer");
	}

	IDPropertyTemplate val = {0};
	ViewLayer *view_layer = MEM_callocN(sizeof(ViewLayer), "View Layer");
	view_layer->flag = VIEW_LAYER_RENDER | VIEW_LAYER_FREESTYLE;

	view_layer->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	layer_engine_settings_init(view_layer->properties, false);
	BLI_strncpy_utf8(view_layer->name, name, sizeof(view_layer->name));

	/* Link the master collection by default. */
	layer_collection_add(view_layer, NULL, master_scene_collection);

	/* Pure rendering pipeline settings. */
	view_layer->layflag = 0x7FFF;   /* solid ztra halo edge strand */
	view_layer->passflag = SCE_PASS_COMBINED | SCE_PASS_Z;
	view_layer->pass_alpha_threshold = 0.5f;
	BKE_freestyle_config_init(&view_layer->freestyle_config);

	return view_layer;
}

/**
 * Add a new view layer
 * by default, a view layer has the master collection
 */
ViewLayer *BKE_view_layer_add(Scene *scene, const char *name)
{
	SceneCollection *sc = BKE_collection_master(&scene->id);
	ViewLayer *view_layer = view_layer_add(name, sc);

	BLI_addtail(&scene->view_layers, view_layer);

	/* unique name */
	BLI_uniquename(
	        &scene->view_layers, view_layer, DATA_("ViewLayer"), '.',
	        offsetof(ViewLayer, name), sizeof(view_layer->name));

	return view_layer;
}

/**
 * Add a ViewLayer for a Group
 * It should be added only once
 */
ViewLayer *BKE_view_layer_group_add(Group *group)
{
	BLI_assert(group->view_layer == NULL);
	SceneCollection *sc = BKE_collection_master(&group->id);
	ViewLayer *view_layer = view_layer_add(group->id.name + 2, sc);
	return view_layer;
}

void BKE_view_layer_free(ViewLayer *view_layer)
{
	BKE_view_layer_free_ex(view_layer, true);
}

/**
 * Free (or release) any data used by this ViewLayer.
 */
void BKE_view_layer_free_ex(ViewLayer *view_layer, const bool do_id_user)
{
	view_layer->basact = NULL;

	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if (base->collection_properties) {
			IDP_FreeProperty(base->collection_properties);
			MEM_freeN(base->collection_properties);
		}
	}
	BLI_freelistN(&view_layer->object_bases);

	for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
		layer_collection_free(NULL, lc);
	}
	BLI_freelistN(&view_layer->layer_collections);

	if (view_layer->properties) {
		IDP_FreeProperty(view_layer->properties);
		MEM_freeN(view_layer->properties);
	}

	if (view_layer->properties_evaluated) {
		IDP_FreeProperty(view_layer->properties_evaluated);
		MEM_freeN(view_layer->properties_evaluated);
	}

	for (ViewLayerEngineData *sled = view_layer->drawdata.first; sled; sled = sled->next) {
		if (sled->storage) {
			if (sled->free) {
				sled->free(sled->storage);
			}
			MEM_freeN(sled->storage);
		}
	}
	BLI_freelistN(&view_layer->drawdata);

	MEM_SAFE_FREE(view_layer->stats);

	BKE_freestyle_config_free(&view_layer->freestyle_config, do_id_user);

	if (view_layer->id_properties) {
		IDP_FreeProperty(view_layer->id_properties);
		MEM_freeN(view_layer->id_properties);
	}

	MEM_freeN(view_layer);
}

/**
 * Tag all the selected objects of a renderlayer
 */
void BKE_view_layer_selected_objects_tag(ViewLayer *view_layer, const int tag)
{
	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if ((base->flag & BASE_SELECTED) != 0) {
			base->object->flag |= tag;
		}
		else {
			base->object->flag &= ~tag;
		}
	}
}

/**
 * Return the first ViewLayer for a given id
 */
ViewLayer *BKE_view_layer_first_from_id(const ID *owner_id)
{
	switch (GS(owner_id->name)) {
		case ID_SCE:
			return ((Scene *)owner_id)->view_layers.first;
		case ID_GR:
			return ((Group *)owner_id)->view_layer;
		default:
			BLI_assert(!"ID doesn't support view layers");
			return NULL;
	}
}

static bool find_scene_collection_in_scene_collections(ListBase *lb, const LayerCollection *lc)
{
	for (LayerCollection *lcn = lb->first; lcn; lcn = lcn->next) {
		if (lcn == lc) {
			return true;
		}
		if (find_scene_collection_in_scene_collections(&lcn->layer_collections, lc)) {
			return true;
		}
	}
	return false;
}

/**
 * Fallback for when a Scene has no camera to use
 *
 * \param view_layer: in general you want to use the same ViewLayer that is used
 * for depsgraph. If rendering you pass the scene active layer, when viewing in the viewport
 * you want to get ViewLayer from context.
 */
Object *BKE_view_layer_camera_find(ViewLayer *view_layer)
{
	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if (base->object->type == OB_CAMERA) {
			return base->object;
		}
	}

	return NULL;
}

/**
 * Find the ViewLayer a LayerCollection belongs to
 */
ViewLayer *BKE_view_layer_find_from_collection(const ID *owner_id, LayerCollection *lc)
{
	switch (GS(owner_id->name)) {
		case ID_GR:
			return ((Group *)owner_id)->view_layer;
		case ID_SCE:
		{
			Scene *scene = (Scene *)owner_id;
			for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
				if (find_scene_collection_in_scene_collections(&view_layer->layer_collections, lc)) {
					return view_layer;
				}
			}
			return NULL;
		}
		default:
			BLI_assert(!"ID doesn't support scene layers");
			return NULL;
	}
}

/* Base */

Base *BKE_view_layer_base_find(ViewLayer *view_layer, Object *ob)
{
	return BLI_findptr(&view_layer->object_bases, ob, offsetof(Base, object));
}

void BKE_view_layer_base_deselect_all(ViewLayer *view_layer)
{
	Base *base;

	for (base = view_layer->object_bases.first; base; base = base->next) {
		base->flag &= ~BASE_SELECTED;
	}
}

void BKE_view_layer_base_select(struct ViewLayer *view_layer, Base *selbase)
{
	view_layer->basact = selbase;
	if ((selbase->flag & BASE_SELECTABLED) != 0) {
		selbase->flag |= BASE_SELECTED;
	}
}

/****************************************************************************/
/* Copying functions for datablocks that use ViewLayer/SceneCollection */

/* Find the equivalent SceneCollection in the new tree */
static SceneCollection *scene_collection_from_new_tree(
        SceneCollection *sc_reference, SceneCollection *sc_dst, SceneCollection *sc_src)
{
	if (sc_src == sc_reference) {
		return sc_dst;
	}

	for (SceneCollection *nsc_src = sc_src->scene_collections.first, *nsc_dst = sc_dst->scene_collections.first;
	     nsc_src;
	     nsc_src = nsc_src->next, nsc_dst = nsc_dst->next)
	{
		SceneCollection *found = scene_collection_from_new_tree(sc_reference, nsc_dst, nsc_src);
		if (found != NULL) {
			return found;
		}
	}
	return NULL;
}

static void layer_collection_sync_flags(
        LayerCollection *layer_collection_dst,
        const LayerCollection *layer_collection_src)
{
	layer_collection_dst->flag = layer_collection_src->flag;

	if (layer_collection_dst->properties != NULL) {
		IDP_FreeProperty(layer_collection_dst->properties);
		MEM_SAFE_FREE(layer_collection_dst->properties);
	}

	if (layer_collection_src->properties != NULL) {
		layer_collection_dst->properties = IDP_CopyProperty(layer_collection_src->properties);
	}

	layer_collections_sync_flags(&layer_collection_dst->layer_collections,
	                             &layer_collection_src->layer_collections);
}

static void layer_collections_sync_flags(ListBase *layer_collections_dst, const ListBase *layer_collections_src)
{
	BLI_assert(BLI_listbase_count(layer_collections_dst) == BLI_listbase_count(layer_collections_src));
	LayerCollection *layer_collection_dst = (LayerCollection *)layer_collections_dst->first;
	const LayerCollection *layer_collection_src = (const LayerCollection *)layer_collections_src->first;
	while (layer_collection_dst != NULL) {
		layer_collection_sync_flags(layer_collection_dst, layer_collection_src);
		layer_collection_dst = layer_collection_dst->next;
		layer_collection_src = layer_collection_src->next;
	}
}

static bool layer_collection_sync_if_match(
        ListBase *lb,
        const SceneCollection *scene_collection_dst,
        const SceneCollection *scene_collection_src)
{
	for (LayerCollection *layer_collection = lb->first;
	     layer_collection;
	     layer_collection = layer_collection->next)
	{
		if (layer_collection->scene_collection == scene_collection_src) {
			LayerCollection *layer_collection_dst =
			        BLI_findptr(
			            lb,
			            scene_collection_dst,
			            offsetof(LayerCollection, scene_collection));

			if (layer_collection_dst != NULL) {
				layer_collection_sync_flags(layer_collection_dst, layer_collection);
			}
			return true;
		}
		else {
			if (layer_collection_sync_if_match(
			        &layer_collection->layer_collections,
			        scene_collection_dst,
			        scene_collection_src))
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * Sync sibling collections across all view layers
 *
 * Make sure every linked instance of \a scene_collection_dst has the same values
 * (flags, overrides, ...) as the corresponding scene_collection_src.
 *
 * \note expect scene_collection_dst to be scene_collection_src->next, and it also
 * expects both collections to have the same ammount of sub-collections.
 */
void BKE_layer_collection_sync_flags(
        ID *owner_id,
        SceneCollection *scene_collection_dst,
        SceneCollection *scene_collection_src)
{
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		for (LayerCollection *layer_collection = view_layer->layer_collections.first;
		     layer_collection;
		     layer_collection = layer_collection->next)
		{
			layer_collection_sync_if_match(
			            &layer_collection->layer_collections,
			            scene_collection_dst,
			            scene_collection_src);
		}
	}
}

/* recreate the LayerCollection tree */
static void layer_collections_recreate(
        ViewLayer *view_layer_dst, ListBase *lb_src, SceneCollection *mc_dst, SceneCollection *mc_src)
{
	for (LayerCollection *lc_src = lb_src->first; lc_src; lc_src = lc_src->next) {
		SceneCollection *sc_dst = scene_collection_from_new_tree(lc_src->scene_collection, mc_dst, mc_src);
		BLI_assert(sc_dst);

		/* instead of synchronizing both trees we simply re-create it */
		BKE_collection_link(view_layer_dst, sc_dst);
	}
}

/**
 * Only copy internal data of ViewLayer from source to already allocated/initialized destination.
 *
 * \param mc_src Master Collection the source ViewLayer links in.
 * \param mc_dst Master Collection the destination ViewLayer links in.
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_view_layer_copy_data(
        ViewLayer *view_layer_dst, ViewLayer *view_layer_src, SceneCollection *mc_dst, SceneCollection *mc_src,
        const int flag)
{
	IDPropertyTemplate val = {0};

	if (view_layer_dst->id_properties != NULL) {
		view_layer_dst->id_properties = IDP_CopyProperty_ex(view_layer_dst->id_properties, flag);
	}
	BKE_freestyle_config_copy(&view_layer_dst->freestyle_config, &view_layer_src->freestyle_config, flag);

	view_layer_dst->stats = NULL;
	view_layer_dst->properties_evaluated = NULL;
	view_layer_dst->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	IDP_MergeGroup_ex(view_layer_dst->properties, view_layer_src->properties, true, flag);

	/* we start fresh with no overrides and no visibility flags set
	 * instead of syncing both trees we simply unlink and relink the scene collection */
	BLI_listbase_clear(&view_layer_dst->layer_collections);
	BLI_listbase_clear(&view_layer_dst->object_bases);
	BLI_listbase_clear(&view_layer_dst->drawdata);

	layer_collections_recreate(view_layer_dst, &view_layer_src->layer_collections, mc_dst, mc_src);

	/* Now we handle the syncing for visibility, selectability, ... */
	layer_collections_sync_flags(&view_layer_dst->layer_collections, &view_layer_src->layer_collections);

	Object *active_ob = OBACT(view_layer_src);
	for (Base *base_src = view_layer_src->object_bases.first, *base_dst = view_layer_dst->object_bases.first;
	     base_src;
	     base_src = base_src->next, base_dst = base_dst->next)
	{
		base_dst->flag = base_src->flag;
		base_dst->flag_legacy = base_src->flag_legacy;

		if (base_dst->object == active_ob) {
			view_layer_dst->basact = base_dst;
		}
	}
}

/**
 * Find and return the ListBase of LayerCollection that has \a lc_child as one of its directly
 * nested LayerCollection.
 *
 * \param lb_parent Initial ListBase of LayerCollection to look into recursively
 * usually the view layer's collection list
 */
static ListBase *find_layer_collection_parent_list_base(ListBase *lb_parent, const LayerCollection *lc_child)
{
	for (LayerCollection *lc_nested = lb_parent->first; lc_nested; lc_nested = lc_nested->next) {
		if (lc_nested == lc_child) {
			return lb_parent;
		}

		ListBase *found = find_layer_collection_parent_list_base(&lc_nested->layer_collections, lc_child);
		if (found != NULL) {
			return found;
		}
	}

	return NULL;
}

/**
 * Makes a shallow copy of a LayerCollection
 *
 * Add a new collection in the same level as the old one (linking if necessary),
 * and copy all the collection data across them.
 */
struct LayerCollection *BKE_layer_collection_duplicate(struct ID *owner_id, struct LayerCollection *layer_collection)
{
	SceneCollection *scene_collection, *scene_collection_new;

	scene_collection = layer_collection->scene_collection;
	scene_collection_new = BKE_collection_duplicate(owner_id, scene_collection);

	LayerCollection *layer_collection_new = NULL;

	/* If the original layer_collection was directly linked to the view layer
	   we need to link the new scene collection here as well. */
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		if (BLI_findindex(&view_layer->layer_collections, layer_collection) != -1) {
			layer_collection_new = BKE_collection_link(view_layer, scene_collection_new);
			layer_collection_sync_flags(layer_collection_new, layer_collection);

			if (layer_collection_new != layer_collection->next) {
				BLI_remlink(&view_layer->layer_collections, layer_collection_new);
				BLI_insertlinkafter(&view_layer->layer_collections, layer_collection, layer_collection_new);
			}
			break;
		}
	}

	/* Otherwise just try to find the corresponding layer collection to return it back. */
	if (layer_collection_new == NULL) {
		for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
			ListBase *layer_collections_parent;
			layer_collections_parent = find_layer_collection_parent_list_base(
			                               &view_layer->layer_collections,
			                               layer_collection);
			if (layer_collections_parent != NULL) {
				layer_collection_new = BLI_findptr(
				        layer_collections_parent,
				        scene_collection_new,
				        offsetof(LayerCollection, scene_collection));
				break;
			}
		}
	}
	return layer_collection_new;
}

static void view_layer_object_base_unref(ViewLayer *view_layer, Base *base)
{
	base->refcount--;

	/* It only exists in the RenderLayer */
	if (base->refcount == 0) {
		if (view_layer->basact == base) {
			view_layer->basact = NULL;
		}

		if (base->collection_properties) {
			IDP_FreeProperty(base->collection_properties);
			MEM_freeN(base->collection_properties);
		}

		BLI_remlink(&view_layer->object_bases, base);
		MEM_freeN(base);
	}
}

/**
 * Return the base if existent, or create it if necessary
 * Always bump the refcount
 */
static Base *object_base_add(ViewLayer *view_layer, Object *ob)
{
	Base *base;
	base = BKE_view_layer_base_find(view_layer, ob);

	if (base == NULL) {
		base = MEM_callocN(sizeof(Base), "Object Base");

		/* Do not bump user count, leave it for SceneCollections. */
		base->object = ob;
		BLI_addtail(&view_layer->object_bases, base);

		IDPropertyTemplate val = {0};
		base->collection_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	}

	base->refcount++;
	return base;
}

/* LayerCollection */

static void layer_collection_objects_unpopulate(ViewLayer *view_layer, LayerCollection *lc)
{
	if (view_layer) {
		for (LinkData *link = lc->object_bases.first; link; link = link->next) {
			view_layer_object_base_unref(view_layer, link->data);
		}
	}

	BLI_freelistN(&lc->object_bases);
}

/**
 * When freeing the entire ViewLayer at once we don't bother with unref
 * otherwise ViewLayer is passed to keep the syncing of the LayerCollection tree
 */
static void layer_collection_free(ViewLayer *view_layer, LayerCollection *lc)
{
	layer_collection_objects_unpopulate(view_layer, lc);
	BLI_freelistN(&lc->overrides);

	if (lc->properties) {
		IDP_FreeProperty(lc->properties);
		MEM_freeN(lc->properties);
	}

	if (lc->properties_evaluated) {
		IDP_FreeProperty(lc->properties_evaluated);
		MEM_freeN(lc->properties_evaluated);
	}

	for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
		layer_collection_free(view_layer, nlc);
	}

	BLI_freelistN(&lc->layer_collections);
}

/**
 * Free (or release) LayerCollection from ViewLayer
 * (does not free the LayerCollection itself).
 */
void BKE_layer_collection_free(ViewLayer *view_layer, LayerCollection *lc)
{
	layer_collection_free(view_layer, lc);
}

/* LayerCollection */

/**
 * Recursively get the collection for a given index
 */
static LayerCollection *collection_from_index(ListBase *lb, const int number, int *i)
{
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		if (*i == number) {
			return lc;
		}

		(*i)++;

		LayerCollection *lc_nested = collection_from_index(&lc->layer_collections, number, i);
		if (lc_nested) {
			return lc_nested;
		}
	}
	return NULL;
}

/**
 * Get the collection for a given index
 */
LayerCollection *BKE_layer_collection_from_index(ViewLayer *view_layer, const int index)
{
	int i = 0;
	return collection_from_index(&view_layer->layer_collections, index, &i);
}

/**
 * Get the active collection
 */
LayerCollection *BKE_layer_collection_get_active(ViewLayer *view_layer)
{
	int i = 0;
	return collection_from_index(&view_layer->layer_collections, view_layer->active_collection, &i);
}


/**
 * Return layer collection to add new object(s).
 * Create one if none exists.
 */
LayerCollection *BKE_layer_collection_get_active_ensure(Scene *scene, ViewLayer *view_layer)
{
	LayerCollection *lc = BKE_layer_collection_get_active(view_layer);

	if (lc == NULL) {
		BLI_assert(BLI_listbase_is_empty(&view_layer->layer_collections));
		/* When there is no collection linked to this ViewLayer, create one. */
		SceneCollection *sc = BKE_collection_add(&scene->id, NULL, COLLECTION_TYPE_NONE, NULL);
		lc = BKE_collection_link(view_layer, sc);
		/* New collection has to be the active one. */
		BLI_assert(lc == BKE_layer_collection_get_active(view_layer));
	}

	return lc;
}

/**
 * Recursively get the count of collections
 */
static int collection_count(ListBase *lb)
{
	int i = 0;
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		i += collection_count(&lc->layer_collections) + 1;
	}
	return i;
}

/**
 * Get the total number of collections
 * (including all the nested collections)
 */
int BKE_layer_collection_count(ViewLayer *view_layer)
{
	return collection_count(&view_layer->layer_collections);
}

/**
 * Recursively get the index for a given collection
 */
static int index_from_collection(ListBase *lb, const LayerCollection *lc, int *i)
{
	for (LayerCollection *lcol = lb->first; lcol; lcol = lcol->next) {
		if (lcol == lc) {
			return *i;
		}

		(*i)++;

		int i_nested = index_from_collection(&lcol->layer_collections, lc, i);
		if (i_nested != -1) {
			return i_nested;
		}
	}
	return -1;
}

/**
 * Return -1 if not found
 */
int BKE_layer_collection_findindex(ViewLayer *view_layer, const LayerCollection *lc)
{
	int i = 0;
	return index_from_collection(&view_layer->layer_collections, lc, &i);
}

/**
 * Lookup the listbase that contains \a lc.
 */
static ListBase *layer_collection_listbase_find(ListBase *lb, LayerCollection *lc)
{
	for (LayerCollection *lc_iter = lb->first; lc_iter; lc_iter = lc_iter->next) {
		if (lc_iter == lc) {
			return lb;
		}

		ListBase *lb_child_result;
		if ((lb_child_result = layer_collection_listbase_find(&lc_iter->layer_collections, lc))) {
			return lb_child_result;
		}
	}

	return NULL;
}

#if 0
/**
 * Lookup the listbase that contains \a sc.
 */
static ListBase *scene_collection_listbase_find(ListBase *lb, SceneCollection *sc)
{
	for (SceneCollection *sc_iter = lb->first; sc_iter; sc_iter = sc_iter->next) {
		if (sc_iter == sc) {
			return lb;
		}

		ListBase *lb_child_result;
		if ((lb_child_result = scene_collection_listbase_find(&sc_iter->scene_collections, sc))) {
			return lb_child_result;
		}
	}

	return NULL;
}
#endif

/* ---------------------------------------------------------------------- */
/* Outliner drag and drop */

/**
 * Nest a LayerCollection into another one
 * Both collections must be from the same ViewLayer, return true if succeded.
 *
 * The LayerCollection will effectively be moved into the
 * new (nested) position. So all the settings, overrides, ... go with it, and
 * if the collection was directly linked to the ViewLayer it's then unlinked.
 *
 * For the other ViewLayers we simply resync the tree, without changing directly
 * linked collections (even if they link to the same SceneCollection)
 *
 * \param lc_src LayerCollection to nest into \a lc_dst
 * \param lc_dst LayerCollection to have \a lc_src inserted into
 */

static void layer_collection_swap(
        ViewLayer *view_layer, ListBase *lb_a, ListBase *lb_b,
        LayerCollection *lc_a, LayerCollection *lc_b)
{
	if (lb_a == NULL) {
		lb_a = layer_collection_listbase_find(&view_layer->layer_collections, lc_a);
	}

	if (lb_b == NULL) {
		lb_b = layer_collection_listbase_find(&view_layer->layer_collections, lc_b);
	}

	BLI_assert(lb_a);
	BLI_assert(lb_b);

	BLI_listbases_swaplinks(lb_a, lb_b, lc_a, lc_b);
}

/**
 * Move \a lc_src into \a lc_dst. Both have to be stored in \a view_layer.
 * If \a lc_src is directly linked to the ViewLayer it's unlinked
 */
bool BKE_layer_collection_move_into(const ID *owner_id, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	ViewLayer *view_layer = BKE_view_layer_find_from_collection(owner_id, lc_src);
	bool is_directly_linked = false;

	if ((!view_layer) || (view_layer != BKE_view_layer_find_from_collection(owner_id, lc_dst))) {
		return false;
	}

	/* We can't nest the collection into itself */
	if (lc_src->scene_collection == lc_dst->scene_collection) {
		return false;
	}

	/* Collection is already where we wanted it to be */
	if (lc_dst->layer_collections.last == lc_src) {
		return false;
	}

	/* Collection is already where we want it to be in the scene tree
	 * but we want to swap it in the layer tree still */
	if (lc_dst->scene_collection->scene_collections.last == lc_src->scene_collection) {
		LayerCollection *lc_swap = lc_dst->layer_collections.last;
		layer_collection_swap(view_layer, &lc_dst->layer_collections, NULL, lc_dst->layer_collections.last, lc_src);

		if (BLI_findindex(&view_layer->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(view_layer, lc_swap);
		}
		return true;
	}
	else {
		LayerCollection *lc_temp;
		is_directly_linked = BLI_findindex(&view_layer->layer_collections, lc_src) != -1;

		if (!is_directly_linked) {
			/* lc_src will be invalid after BKE_collection_move_into!
			 * so we swap it with lc_temp to preserve its settings */
			lc_temp = BKE_collection_link(view_layer, lc_src->scene_collection);
			layer_collection_swap(view_layer, &view_layer->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_into(owner_id, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked) {
				/* Swap back and remove */
				layer_collection_swap(view_layer, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(view_layer, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = BLI_findptr(
	        &lc_dst->layer_collections, lc_src->scene_collection, offsetof(LayerCollection, scene_collection));
	BLI_assert(lc_new);
	layer_collection_swap(view_layer, &lc_dst->layer_collections, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&view_layer->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(view_layer, lc_new);
	}

	return true;
}

/**
 * Move \a lc_src above \a lc_dst. Both have to be stored in \a view_layer.
 * If \a lc_src is directly linked to the ViewLayer it's unlinked
 */
bool BKE_layer_collection_move_above(const ID *owner_id, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	ViewLayer *view_layer = BKE_view_layer_find_from_collection(owner_id, lc_src);
	const bool is_directly_linked_src = BLI_findindex(&view_layer->layer_collections, lc_src) != -1;
	const bool is_directly_linked_dst = BLI_findindex(&view_layer->layer_collections, lc_dst) != -1;

	if ((!view_layer) || (view_layer != BKE_view_layer_find_from_collection(owner_id, lc_dst))) {
		return false;
	}

	/* Collection is already where we wanted it to be */
	if (lc_dst->prev == lc_src) {
		return false;
	}

	/* Collection is already where we want it to be in the scene tree
	 * but we want to swap it in the layer tree still */
	if (lc_dst->prev && lc_dst->prev->scene_collection == lc_src->scene_collection) {
		LayerCollection *lc_swap = lc_dst->prev;
		layer_collection_swap(view_layer, NULL, NULL, lc_dst->prev, lc_src);

		if (BLI_findindex(&view_layer->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(view_layer, lc_swap);
		}
		return true;
	}
	/* We don't allow to move above/below a directly linked collection
	 * unless the source collection is also directly linked */
	else if (is_directly_linked_dst) {
		/* Both directly linked to the ViewLayer, just need to swap */
		if (is_directly_linked_src) {
			BLI_remlink(&view_layer->layer_collections, lc_src);
			BLI_insertlinkbefore(&view_layer->layer_collections, lc_dst, lc_src);
			return true;
		}
		else {
			return false;
		}
	}
	else {
		LayerCollection *lc_temp;

		if (!is_directly_linked_src) {
			/* lc_src will be invalid after BKE_collection_move_into!
			 * so we swap it with lc_temp to preserve its settings */
			lc_temp = BKE_collection_link(view_layer, lc_src->scene_collection);
			layer_collection_swap(view_layer, &view_layer->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_above(owner_id, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked_src) {
				/* Swap back and remove */
				layer_collection_swap(view_layer, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(view_layer, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = lc_dst->prev;
	BLI_assert(lc_new);
	layer_collection_swap(view_layer, NULL, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&view_layer->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(view_layer, lc_new);
	}

	return true;
}

/**
 * Move \a lc_src below \a lc_dst. Both have to be stored in \a view_layer.
 * If \a lc_src is directly linked to the ViewLayer it's unlinked
 */
bool BKE_layer_collection_move_below(const ID *owner_id, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	ViewLayer *view_layer = BKE_view_layer_find_from_collection(owner_id, lc_src);
	const bool is_directly_linked_src = BLI_findindex(&view_layer->layer_collections, lc_src) != -1;
	const bool is_directly_linked_dst = BLI_findindex(&view_layer->layer_collections, lc_dst) != -1;

	if ((!view_layer) || (view_layer != BKE_view_layer_find_from_collection(owner_id, lc_dst))) {
		return false;
	}

	/* Collection is already where we wanted it to be */
	if (lc_dst->next == lc_src) {
		return false;
	}

	/* Collection is already where we want it to be in the scene tree
	 * but we want to swap it in the layer tree still */
	if (lc_dst->next && lc_dst->next->scene_collection == lc_src->scene_collection) {
		LayerCollection *lc_swap = lc_dst->next;
		layer_collection_swap(view_layer, NULL, NULL, lc_dst->next, lc_src);

		if (BLI_findindex(&view_layer->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(view_layer, lc_swap);
		}
		return true;
	}
	/* We don't allow to move above/below a directly linked collection
	 * unless the source collection is also directly linked */
	else if (is_directly_linked_dst) {
		/* Both directly linked to the ViewLayer, just need to swap */
		if (is_directly_linked_src) {
			BLI_remlink(&view_layer->layer_collections, lc_src);
			BLI_insertlinkafter(&view_layer->layer_collections, lc_dst, lc_src);
			return true;
		}
		else {
			return false;
		}
	}
	else {
		LayerCollection *lc_temp;

		if (!is_directly_linked_src) {
			/* lc_src will be invalid after BKE_collection_move_into!
			 * so we swap it with lc_temp to preserve its settings */
			lc_temp = BKE_collection_link(view_layer, lc_src->scene_collection);
			layer_collection_swap(view_layer, &view_layer->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_below(owner_id, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked_src) {
				/* Swap back and remove */
				layer_collection_swap(view_layer, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(view_layer, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = lc_dst->next;
	BLI_assert(lc_new);
	layer_collection_swap(view_layer, NULL, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&view_layer->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(view_layer, lc_new);
	}

	return true;
}

static bool layer_collection_resync(ViewLayer *view_layer, LayerCollection *lc, const SceneCollection *sc)
{
	if (lc->scene_collection == sc) {
		ListBase collections = {NULL};
		BLI_movelisttolist(&collections, &lc->layer_collections);

		for (SceneCollection *sc_nested = sc->scene_collections.first; sc_nested; sc_nested = sc_nested->next) {
			LayerCollection *lc_nested = BLI_findptr(&collections, sc_nested, offsetof(LayerCollection, scene_collection));
			if (lc_nested) {
				BLI_remlink(&collections, lc_nested);
				BLI_addtail(&lc->layer_collections, lc_nested);
			}
			else {
				layer_collection_add(view_layer, lc, sc_nested);
			}
		}

		for (LayerCollection *lc_nested = collections.first; lc_nested; lc_nested = lc_nested->next) {
			layer_collection_free(view_layer, lc_nested);
		}
		BLI_freelistN(&collections);

		BLI_assert(BLI_listbase_count(&lc->layer_collections) ==
		           BLI_listbase_count(&sc->scene_collections));

		return true;
	}

	for (LayerCollection *lc_nested = lc->layer_collections.first; lc_nested; lc_nested = lc_nested->next) {
		if (layer_collection_resync(view_layer, lc_nested, sc)) {
			return true;
		}
	}

	return false;
}

/**
 * Update the scene layers so that any LayerCollection that points
 * to \a sc is re-synced again
 */
void BKE_layer_collection_resync(const ID *owner_id, const SceneCollection *sc)
{
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
			layer_collection_resync(view_layer, lc, sc);
		}
	}
}

/* ---------------------------------------------------------------------- */

/**
 * Select all the objects of this layer collection
 *
 * It also select the objects that are in nested collections.
 * \note Recursive
 */
void BKE_layer_collection_objects_select(struct LayerCollection *layer_collection)
{
	if ((layer_collection->flag & COLLECTION_DISABLED) ||
	    ((layer_collection->flag & COLLECTION_SELECTABLE) == 0))
	{
		return;
	}

	for (LinkData *link = layer_collection->object_bases.first; link; link = link->next) {
		Base *base = link->data;
		if (base->flag & BASE_SELECTABLED) {
			base->flag |= BASE_SELECTED;
		}
	}

	for (LayerCollection *iter = layer_collection->layer_collections.first; iter; iter = iter->next) {
		BKE_layer_collection_objects_select(iter);
	}
}

/* ---------------------------------------------------------------------- */

/**
 * Link a collection to a renderlayer
 * The collection needs to be created separately
 */
LayerCollection *BKE_collection_link(ViewLayer *view_layer, SceneCollection *sc)
{
	LayerCollection *lc = layer_collection_add(view_layer, NULL, sc);
	view_layer->active_collection = BKE_layer_collection_findindex(view_layer, lc);
	return lc;
}

/**
 * Unlink a collection base from a renderlayer
 * The corresponding collection is not removed from the master collection
 */
void BKE_collection_unlink(ViewLayer *view_layer, LayerCollection *lc)
{
	BKE_layer_collection_free(view_layer, lc);
	BLI_remlink(&view_layer->layer_collections, lc);
	MEM_freeN(lc);
	view_layer->active_collection = 0;
}

/**
 * Recursively enable nested collections
 */
static void layer_collection_enable(ViewLayer *view_layer, LayerCollection *lc)
{
	layer_collection_objects_populate(view_layer, lc, &lc->scene_collection->objects);

	for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
		layer_collection_enable(view_layer, nlc);
	}
}

/**
 * Enable collection
 * Add its objects bases to ViewLayer
 *
 * Only around for doversion.
 */
void BKE_collection_enable(ViewLayer *view_layer, LayerCollection *lc)
{
	if ((lc->flag & COLLECTION_DISABLED) == 0) {
		return;
	}

	lc->flag &= ~COLLECTION_DISABLED;
	layer_collection_enable(view_layer, lc);
}

static void layer_collection_object_add(ViewLayer *view_layer, LayerCollection *lc, Object *ob)
{
	Base *base = object_base_add(view_layer, ob);

	/* Only add an object once. */
	if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data))) {
		return;
	}

	bool is_visible = ((lc->flag & COLLECTION_VIEWPORT) != 0) && ((lc->flag & COLLECTION_DISABLED) == 0);
	bool is_selectable = is_visible && ((lc->flag & COLLECTION_SELECTABLE) != 0);

	if (is_visible) {
		base->flag |= BASE_VISIBLED;
	}

	if (is_selectable) {
		base->flag |= BASE_SELECTABLED;
	}

	BLI_addtail(&lc->object_bases, BLI_genericNodeN(base));
}

static void layer_collection_object_remove(ViewLayer *view_layer, LayerCollection *lc, Object *ob)
{
	Base *base;
	base = BKE_view_layer_base_find(view_layer, ob);

	LinkData *link = BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data));
	BLI_remlink(&lc->object_bases, link);
	MEM_freeN(link);

	view_layer_object_base_unref(view_layer, base);
}

static void layer_collection_objects_populate(ViewLayer *view_layer, LayerCollection *lc, ListBase *objects)
{
	for (LinkData *link = objects->first; link; link = link->next) {
		layer_collection_object_add(view_layer, lc, link->data);
	}
}

static void layer_collection_populate(ViewLayer *view_layer, LayerCollection *lc, SceneCollection *sc)
{
	layer_collection_objects_populate(view_layer, lc, &sc->objects);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		layer_collection_add(view_layer, lc, nsc);
	}
}

static LayerCollection *layer_collection_add(ViewLayer *view_layer, LayerCollection *parent, SceneCollection *sc)
{
	IDPropertyTemplate val = {0};
	LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");

	lc->scene_collection = sc;
	lc->flag = COLLECTION_SELECTABLE | COLLECTION_VIEWPORT | COLLECTION_RENDER;

	lc->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	collection_engine_settings_init(lc->properties, false);

	if (parent != NULL) {
		BLI_addtail(&parent->layer_collections, lc);
	}
	else {
		BLI_addtail(&view_layer->layer_collections, lc);
	}

	layer_collection_populate(view_layer, lc, sc);

	return lc;
}

/* ---------------------------------------------------------------------- */

/**
 * See if render layer has the scene collection linked directly, or indirectly (nested)
 */
bool BKE_view_layer_has_collection(ViewLayer *view_layer, const SceneCollection *sc)
{
	for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
		if (find_layer_collection_by_scene_collection(lc, sc) != NULL) {
			return true;
		}
	}
	return false;
}

/**
 * See if the object is in any of the scene layers of the scene
 */
bool BKE_scene_has_object(Scene *scene, Object *ob)
{
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		Base *base = BKE_view_layer_base_find(view_layer, ob);
		if (base) {
			return true;
		}
	}
	return false;
}


/* ---------------------------------------------------------------------- */
/* Syncing */

static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc, const SceneCollection *sc)
{
	if (lc->scene_collection == sc) {
		return lc;
	}

	for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
		LayerCollection *found = find_layer_collection_by_scene_collection(nlc, sc);
		if (found) {
			return found;
		}
	}
	return NULL;
}

/**
 * Add a new LayerCollection for all the ViewLayers that have sc_parent
 */
void BKE_layer_sync_new_scene_collection(ID *owner_id, const SceneCollection *sc_parent, SceneCollection *sc)
{
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *lc_parent = find_layer_collection_by_scene_collection(lc, sc_parent);
			if (lc_parent) {
				layer_collection_add(view_layer, lc_parent, sc);
			}
		}
	}
}

/**
 * Add a corresponding ObjectBase to all the equivalent LayerCollection
 */
void BKE_layer_sync_object_link(const ID *owner_id, SceneCollection *sc, Object *ob)
{
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *found = find_layer_collection_by_scene_collection(lc, sc);
			if (found) {
				layer_collection_object_add(view_layer, found, ob);
			}
		}
	}
}

/**
 * Remove the equivalent object base to all layers that have this collection
 */
void BKE_layer_sync_object_unlink(const ID *owner_id, SceneCollection *sc, Object *ob)
{
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *found = find_layer_collection_by_scene_collection(lc, sc);
			if (found) {
				layer_collection_object_remove(view_layer, found, ob);
			}
		}
	}
}

/* ---------------------------------------------------------------------- */
/* Override */

/**
 * Add a new datablock override
 */
void BKE_override_view_layer_datablock_add(ViewLayer *view_layer, int id_type, const char *data_path, const ID *owner_id)
{
	UNUSED_VARS(view_layer, id_type, data_path, owner_id);
	TODO_LAYER_OVERRIDE;
}

/**
 * Add a new int override
 */
void BKE_override_view_layer_int_add(ViewLayer *view_layer, int id_type, const char *data_path, const int value)
{
	UNUSED_VARS(view_layer, id_type, data_path, value);
	TODO_LAYER_OVERRIDE;
}

/**
 * Add a new boolean override
 */
void BKE_override_layer_collection_boolean_add(struct LayerCollection *layer_collection, int id_type, const char *data_path, const bool value)
{
	UNUSED_VARS(layer_collection, id_type, data_path, value);
	TODO_LAYER_OVERRIDE;
}

/* ---------------------------------------------------------------------- */
/* Engine Settings */

ListBase R_layer_collection_engines_settings_callbacks = {NULL, NULL};
ListBase R_view_layer_engines_settings_callbacks = {NULL, NULL};

typedef struct EngineSettingsCB_Type {
	struct EngineSettingsCB_Type *next, *prev;

	char name[MAX_NAME]; /* engine name */

	EngineSettingsCB callback;

} EngineSettingsCB_Type;

static void create_engine_settings_scene(IDProperty *root, EngineSettingsCB_Type *es_type)
{
	if (collection_engine_get(root, COLLECTION_MODE_NONE, es_type->name)) {
		return;
	}

	IDProperty *props = collection_engine_settings_create(es_type, true);
	IDP_AddToGroup(root, props);
}

static void create_layer_collection_engine_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
{
	create_engine_settings_scene(scene->collection_properties, es_type);
}

static void create_view_layer_engine_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
{
	create_engine_settings_scene(scene->layer_properties, es_type);
}

static void create_layer_collection_engine_settings_collection(LayerCollection *lc, EngineSettingsCB_Type *es_type)
{
	if (BKE_layer_collection_engine_collection_get(lc, COLLECTION_MODE_NONE, es_type->name)) {
		return;
	}

	IDProperty *props = collection_engine_settings_create(es_type, false);
	IDP_AddToGroup(lc->properties, props);

	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		create_layer_collection_engine_settings_collection(lcn, es_type);
	}
}

static void create_layer_collection_engines_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
{
	/* Populate the scene with the new settings. */
	create_layer_collection_engine_settings_scene(scene, es_type);

	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
			create_layer_collection_engine_settings_collection(lc, es_type);
		}
	}
}

static void create_view_layer_engines_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
{
	/* Populate the scene with the new settings. */
	create_view_layer_engine_settings_scene(scene, es_type);
}

static void create_view_layer_engines_settings_layer(ViewLayer *view_layer, EngineSettingsCB_Type *es_type)
{
	if (BKE_view_layer_engine_layer_get(view_layer, COLLECTION_MODE_NONE, es_type->name)) {
		return;
	}

	IDProperty *props = collection_engine_settings_create(es_type, false);
	IDP_AddToGroup(view_layer->properties, props);
}

static EngineSettingsCB_Type *engine_settings_callback_register(const char *engine_name, EngineSettingsCB func, ListBase *lb)
{
	EngineSettingsCB_Type *es_type;

	/* Cleanup in case it existed. */
	es_type = BLI_findstring(lb, engine_name, offsetof(EngineSettingsCB_Type, name));

	if (es_type) {
		BLI_remlink(lb, es_type);
		MEM_freeN(es_type);
	}

	es_type = MEM_callocN(sizeof(EngineSettingsCB_Type), __func__);
	BLI_strncpy_utf8(es_type->name, engine_name, sizeof(es_type->name));
	es_type->callback = func;
	BLI_addtail(lb, es_type);

	return es_type;
}

void BKE_layer_collection_engine_settings_callback_register(
        Main *bmain, const char *engine_name, EngineSettingsCB func)
{
	EngineSettingsCB_Type *es_type =
	        engine_settings_callback_register(engine_name, func, &R_layer_collection_engines_settings_callbacks);

	if (bmain) {
		/* Populate all of the collections of the scene with those settings. */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			create_layer_collection_engines_settings_scene(scene, es_type);
		}
	}
}

void BKE_view_layer_engine_settings_callback_register(
        Main *bmain, const char *engine_name, EngineSettingsCB func)
{
	EngineSettingsCB_Type *es_type =
	        engine_settings_callback_register(engine_name, func, &R_view_layer_engines_settings_callbacks);

	if (bmain) {
		/* Populate all of the collections of the scene with those settings. */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			create_view_layer_engines_settings_scene(scene, es_type);

			for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
				create_view_layer_engines_settings_layer(view_layer, es_type);
			}
		}
	}
}

void BKE_layer_collection_engine_settings_callback_free(void)
{
	BLI_freelistN(&R_layer_collection_engines_settings_callbacks);
}

void BKE_view_layer_engine_settings_callback_free(void)
{
	BLI_freelistN(&R_view_layer_engines_settings_callbacks);
}

/**
 * Create a root IDProperty for this engine
 *
 * \param populate whether we want to pre-fill the collection with the default properties
 */
static IDProperty *collection_engine_settings_create(EngineSettingsCB_Type *es_type, const bool populate)
{
	IDProperty *props;
	IDPropertyTemplate val = {0};

	props = IDP_New(IDP_GROUP, &val, es_type->name);
	props->subtype = IDP_GROUP_SUB_ENGINE_RENDER;

	/* properties */
	if (populate) {
		es_type->callback(NULL, props);
	}

	return props;
}

static void layer_collection_create_mode_settings_object(IDProperty *root, const bool populate)
{
	IDProperty *props;
	IDPropertyTemplate val = {0};

	props = IDP_New(IDP_GROUP, &val, "ObjectMode");
	props->subtype = IDP_GROUP_SUB_MODE_OBJECT;

	/* properties */
	if (populate) {
		OBJECT_collection_settings_create(props);
	}

	IDP_AddToGroup(root, props);
}

static void layer_collection_create_mode_settings_edit(IDProperty *root, const bool populate)
{
	IDProperty *props;
	IDPropertyTemplate val = {0};

	props = IDP_New(IDP_GROUP, &val, "EditMode");
	props->subtype = IDP_GROUP_SUB_MODE_EDIT;

	/* properties */
	if (populate) {
		EDIT_MESH_collection_settings_create(props);
	}

	IDP_AddToGroup(root, props);
}

static void layer_collection_create_mode_settings_paint_weight(IDProperty *root, const bool populate)
{
	IDProperty *props;
	IDPropertyTemplate val = {0};

	props = IDP_New(IDP_GROUP, &val, "WeightPaintMode");
	props->subtype = IDP_GROUP_SUB_MODE_PAINT_WEIGHT;

	/* properties */
	if (populate) {
		PAINT_WEIGHT_collection_settings_create(props);
	}

	IDP_AddToGroup(root, props);
}

static void layer_collection_create_mode_settings_paint_vertex(IDProperty *root, const bool populate)
{
	IDProperty *props;
	IDPropertyTemplate val = {0};

	props = IDP_New(IDP_GROUP, &val, "VertexPaintMode");
	props->subtype = IDP_GROUP_SUB_MODE_PAINT_VERTEX;

	/* properties */
	if (populate) {
		PAINT_VERTEX_collection_settings_create(props);
	}

	IDP_AddToGroup(root, props);
}

static void layer_collection_create_render_settings(IDProperty *root, const bool populate)
{
	EngineSettingsCB_Type *es_type;
	for (es_type = R_layer_collection_engines_settings_callbacks.first; es_type; es_type = es_type->next) {
		IDProperty *props = collection_engine_settings_create(es_type, populate);
		IDP_AddToGroup(root, props);
	}
}

static void view_layer_create_render_settings(IDProperty *root, const bool populate)
{
	EngineSettingsCB_Type *es_type;
	for (es_type = R_view_layer_engines_settings_callbacks.first; es_type; es_type = es_type->next) {
		IDProperty *props = collection_engine_settings_create(es_type, populate);
		IDP_AddToGroup(root, props);
	}
}

static void collection_create_mode_settings(IDProperty *root, const bool populate)
{
	/* XXX TODO: put all those engines in the R_engines_settings_callbacks
	 * and have IDP_AddToGroup outside the callbacks */
	layer_collection_create_mode_settings_object(root, populate);
	layer_collection_create_mode_settings_edit(root, populate);
	layer_collection_create_mode_settings_paint_weight(root, populate);
	layer_collection_create_mode_settings_paint_vertex(root, populate);
}

static void layer_create_mode_settings(IDProperty *root, const bool populate)
{
	TODO_LAYER; /* XXX like collection_create_mode_settings */
	UNUSED_VARS(root, populate);
}

static int idproperty_group_subtype(const int mode_type)
{
	int idgroup_type;

	switch (mode_type) {
		case COLLECTION_MODE_OBJECT:
			idgroup_type = IDP_GROUP_SUB_MODE_OBJECT;
			break;
		case COLLECTION_MODE_EDIT:
			idgroup_type = IDP_GROUP_SUB_MODE_EDIT;
			break;
		case COLLECTION_MODE_PAINT_WEIGHT:
			idgroup_type = IDP_GROUP_SUB_MODE_PAINT_WEIGHT;
			break;
		case COLLECTION_MODE_PAINT_VERTEX:
			idgroup_type = IDP_GROUP_SUB_MODE_PAINT_VERTEX;
			break;
		default:
		case COLLECTION_MODE_NONE:
			return IDP_GROUP_SUB_ENGINE_RENDER;
			break;
	}

	return idgroup_type;
}

/**
 * Return collection enginne settings for either Object s of LayerCollection s
 */
static IDProperty *collection_engine_get(
        IDProperty *root, const int type, const char *engine_name)
{
	const int subtype = idproperty_group_subtype(type);

	if (subtype == IDP_GROUP_SUB_ENGINE_RENDER) {
		return IDP_GetPropertyFromGroup(root, engine_name);
	}
	else {
		IDProperty *prop;
		for (prop = root->data.group.first; prop; prop = prop->next) {
			if (prop->subtype == subtype) {
				return prop;
			}
		}
	}

	BLI_assert(false);
	return NULL;
}

/**
 * Return collection engine settings from Object for specified engine of mode
 */
IDProperty *BKE_layer_collection_engine_evaluated_get(Object *ob, const int type, const char *engine_name)
{
	return collection_engine_get(ob->base_collection_properties, type, engine_name);
}
/**
 * Return layer collection engine settings for specified engine
 */
IDProperty *BKE_layer_collection_engine_collection_get(LayerCollection *lc, const int type, const char *engine_name)
{
	return collection_engine_get(lc->properties, type, engine_name);
}

/**
 * Return layer collection engine settings for specified engine in the scene
 */
IDProperty *BKE_layer_collection_engine_scene_get(Scene *scene, const int type, const char *engine_name)
{
	return collection_engine_get(scene->collection_properties, type, engine_name);
}

/**
 * Return scene layer engine settings for specified engine in the scene
 */
IDProperty *BKE_view_layer_engine_scene_get(Scene *scene, const int type, const char *engine_name)
{
	return collection_engine_get(scene->layer_properties, type, engine_name);
}

/**
 * Return scene layer engine settings for specified engine
 */
IDProperty *BKE_view_layer_engine_layer_get(ViewLayer *view_layer, const int type, const char *engine_name)
{
	return collection_engine_get(view_layer->properties, type, engine_name);
}

/**
 * Return scene layer evaluated engine settings for specified engine
 */
IDProperty *BKE_view_layer_engine_evaluated_get(ViewLayer *view_layer, const int type, const char *engine_name)
{
	return collection_engine_get(view_layer->properties_evaluated, type, engine_name);
}

/* ---------------------------------------------------------------------- */
/* Engine Settings Properties */

void BKE_collection_engine_property_add_float(IDProperty *props, const char *name, float value)
{
	IDPropertyTemplate val = {0};
	val.f = value;
	IDP_AddToGroup(props, IDP_New(IDP_FLOAT, &val, name));
}

void BKE_collection_engine_property_add_float_array(
        IDProperty *props, const char *name, const float *values, const int array_length)
{
	IDPropertyTemplate val = {0};
	val.array.len = array_length;
	val.array.type = IDP_FLOAT;

	IDProperty *idprop = IDP_New(IDP_ARRAY, &val, name);
	memcpy(IDP_Array(idprop), values, sizeof(float) * idprop->len);
	IDP_AddToGroup(props, idprop);
}

void BKE_collection_engine_property_add_int(IDProperty *props, const char *name, int value)
{
	IDPropertyTemplate val = {0};
	val.i = value;
	IDP_AddToGroup(props, IDP_New(IDP_INT, &val, name));
}

void BKE_collection_engine_property_add_bool(IDProperty *props, const char *name, bool value)
{
	IDPropertyTemplate val = {0};
	val.i = value;
	IDP_AddToGroup(props, IDP_New(IDP_INT, &val, name));
}

int BKE_collection_engine_property_value_get_int(IDProperty *props, const char *name)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	return idprop ? IDP_Int(idprop) : 0;
}

float BKE_collection_engine_property_value_get_float(IDProperty *props, const char *name)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	return idprop ? IDP_Float(idprop) : 0.0f;
}

const float *BKE_collection_engine_property_value_get_float_array(IDProperty *props, const char *name)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	return idprop ? IDP_Array(idprop) : NULL;
}

bool BKE_collection_engine_property_value_get_bool(IDProperty *props, const char *name)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	return idprop ? IDP_Int(idprop) : 0;
}

void BKE_collection_engine_property_value_set_int(IDProperty *props, const char *name, int value)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	IDP_Int(idprop) = value;
}

void BKE_collection_engine_property_value_set_float(IDProperty *props, const char *name, float value)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	IDP_Float(idprop) = value;
}

void BKE_collection_engine_property_value_set_float_array(IDProperty *props, const char *name, const float *values)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	memcpy(IDP_Array(idprop), values, sizeof(float) * idprop->len);
}

void BKE_collection_engine_property_value_set_bool(IDProperty *props, const char *name, bool value)
{
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, name);
	IDP_Int(idprop) = value;
}

/* Engine Settings recalculate  */

/* get all the default settings defined in scene and merge them here */
static void collection_engine_settings_init(IDProperty *root, const bool populate)
{
	/* render engines */
	layer_collection_create_render_settings(root, populate);

	/* mode engines */
	collection_create_mode_settings(root, populate);
}

/* get all the default settings defined in scene and merge them here */
static void layer_engine_settings_init(IDProperty *root, const bool populate)
{
	/* render engines */
	view_layer_create_render_settings(root, populate);

	/* mode engines */
	layer_create_mode_settings(root, populate);
}

/**
 * Initialize the layer collection render setings
 * It's used mainly for scenes
 */
void BKE_layer_collection_engine_settings_create(IDProperty *root)
{
	collection_engine_settings_init(root, true);
}

/**
 * Initialize the render setings
 * It's used mainly for scenes
 */
void BKE_view_layer_engine_settings_create(IDProperty *root)
{
	layer_engine_settings_init(root, true);
}

/**
 * Reference of IDProperty group scene collection settings
 * Used when reading blendfiles, to see if there is any missing settings.
 */
static struct {
	struct {
		IDProperty *collection_properties;
		IDProperty *render_settings;
	} scene;
	IDProperty *view_layer;
	IDProperty *layer_collection;
} root_reference = {
	.scene = {NULL, NULL},
	.view_layer = NULL,
	.layer_collection = NULL,
};

/**
 * Free the reference scene collection settings IDProperty group.
 */
static void engine_settings_validate_init(void)
{
	IDPropertyTemplate val = {0};

	/* LayerCollection engine settings. */
	if (root_reference.scene.collection_properties == NULL) {
		root_reference.scene.collection_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		collection_engine_settings_init(root_reference.scene.collection_properties, true);
	}

	if (root_reference.layer_collection == NULL) {
		root_reference.layer_collection = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		collection_engine_settings_init(root_reference.layer_collection, false);
	}

	/* Render engine setting. */
	if (root_reference.scene.render_settings == NULL) {
		root_reference.scene.render_settings = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		layer_engine_settings_init(root_reference.scene.render_settings, true);
	}

	if (root_reference.view_layer == NULL) {
		root_reference.view_layer = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		layer_engine_settings_init(root_reference.view_layer, false);
	}
}

/**
 * Free the reference scene collection settings IDProperty group.
 */
static void layer_collection_engine_settings_validate_free(void)
{
	IDProperty *idprops[] = {
	    root_reference.scene.render_settings,
	    root_reference.scene.collection_properties,
	    root_reference.view_layer,
	    root_reference.layer_collection,
	    NULL,
	};

	IDProperty **idprop = &idprops[0];
	while (*idprop) {
		if (*idprop) {
			IDP_FreeProperty(*idprop);
			MEM_freeN(*idprop);
			*idprop = NULL;
			idprop++;
		}
	}
}

/**
 * Make sure Scene has all required collection settings.
 */
void BKE_layer_collection_engine_settings_validate_scene(Scene *scene)
{
	if (root_reference.scene.collection_properties == NULL) {
		engine_settings_validate_init();
	}

	if (scene->collection_properties == NULL) {
		IDPropertyTemplate val = {0};
		scene->collection_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		BKE_layer_collection_engine_settings_create(scene->collection_properties);
	}
	else {
		IDP_MergeGroup(scene->collection_properties, root_reference.scene.collection_properties, false);
	}
}

/**
 * Maker sure LayerCollection has all required collection settings.
 */
void BKE_layer_collection_engine_settings_validate_collection(LayerCollection *lc)
{
	if (root_reference.layer_collection == NULL) {
		engine_settings_validate_init();
	}

	BLI_assert(lc->properties != NULL);
	IDP_MergeGroup(lc->properties, root_reference.layer_collection, false);
}

/**
 * Make sure Scene has all required collection settings.
 */
void BKE_view_layer_engine_settings_validate_scene(Scene *scene)
{
	if (root_reference.scene.render_settings == NULL) {
		engine_settings_validate_init();
	}

	if (scene->layer_properties == NULL) {
		IDPropertyTemplate val = {0};
		scene->layer_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		BKE_view_layer_engine_settings_create(scene->layer_properties);
	}
	else {
		IDP_MergeGroup(scene->layer_properties, root_reference.scene.render_settings, false);
	}
}

/**
 * Make sure Scene has all required collection settings.
 */
void BKE_view_layer_engine_settings_validate_layer(ViewLayer *view_layer)
{
	if (root_reference.view_layer == NULL) {
		engine_settings_validate_init();
	}

	IDP_MergeGroup(view_layer->properties, root_reference.view_layer, false);
}

/* ---------------------------------------------------------------------- */
/* Iterators */

static void object_bases_iterator_begin(BLI_Iterator *iter, void *data_in, const int flag)
{
	ViewLayer *view_layer = data_in;
	Base *base = view_layer->object_bases.first;

	/* when there are no objects */
	if (base ==  NULL) {
		iter->valid = false;
		return;
	}

	iter->data = base;

	if ((base->flag & flag) == 0) {
		object_bases_iterator_next(iter, flag);
	}
	else {
		iter->current = base;
	}
}

static void object_bases_iterator_next(BLI_Iterator *iter, const int flag)
{
	Base *base = ((Base *)iter->data)->next;

	while (base) {
		if ((base->flag & flag) != 0) {
			iter->current = base;
			iter->data = base;
			return;
		}
		base = base->next;
	}

	iter->valid = false;
}

static void objects_iterator_begin(BLI_Iterator *iter, void *data_in, const int flag)
{
	object_bases_iterator_begin(iter, data_in, flag);

	if (iter->valid) {
		iter->current = ((Base *)iter->current)->object;
	}
}

static void objects_iterator_next(BLI_Iterator *iter, const int flag)
{
	object_bases_iterator_next(iter, flag);

	if (iter->valid) {
		iter->current = ((Base *)iter->current)->object;
	}
}

void BKE_selected_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	objects_iterator_begin(iter, data_in, BASE_SELECTED);
}

void BKE_selected_objects_iterator_next(BLI_Iterator *iter)
{
	objects_iterator_next(iter, BASE_SELECTED);
}

void BKE_selected_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_visible_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	objects_iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_visible_objects_iterator_next(BLI_Iterator *iter)
{
	objects_iterator_next(iter, BASE_VISIBLED);
}

void BKE_visible_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_selected_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	object_bases_iterator_begin(iter, data_in, BASE_SELECTED);
}

void BKE_selected_bases_iterator_next(BLI_Iterator *iter)
{
	object_bases_iterator_next(iter, BASE_SELECTED);
}

void BKE_selected_bases_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_visible_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	object_bases_iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_visible_bases_iterator_next(BLI_Iterator *iter)
{
	object_bases_iterator_next(iter, BASE_VISIBLED);
}

void BKE_visible_bases_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_renderable_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	struct ObjectsRenderableIteratorData *data = data_in;

	/* Tag objects to prevent going over the same object twice. */
	for (Scene *scene = data->scene; scene; scene = scene->set) {
		for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
			for (Base *base = view_layer->object_bases.first; base; base = base->next) {
				 base->object->id.flag |= LIB_TAG_DOIT;
			}
		}
	}

	ViewLayer *view_layer = data->scene->view_layers.first;
	data->iter.view_layer = view_layer;

	data->base_temp.next = view_layer->object_bases.first;
	data->iter.base = &data->base_temp;

	data->iter.set = NULL;

	iter->data = data_in;
	BKE_renderable_objects_iterator_next(iter);
}

void BKE_renderable_objects_iterator_next(BLI_Iterator *iter)
{
	/* Set it early in case we need to exit and we are running from within a loop. */
	iter->skip = true;

	struct ObjectsRenderableIteratorData *data = iter->data;
	Base *base = data->iter.base->next;

	/* There is still a base in the current scene layer. */
	if (base != NULL) {
		Object *ob = base->object;

		/* We need to set the iter.base even if the rest fail otherwise
		 * we keep checking the exactly same base over and over again. */
		data->iter.base = base;

		if (ob->id.flag & LIB_TAG_DOIT) {
			ob->id.flag &= ~LIB_TAG_DOIT;

			if ((base->flag & BASE_VISIBLED) != 0) {
				iter->skip = false;
				iter->current = ob;
			}
		}
		return;
	}

	/* Time to go to the next scene layer. */
	if (data->iter.set == NULL) {
		while ((data->iter.view_layer = data->iter.view_layer->next)) {
			ViewLayer *view_layer = data->iter.view_layer;
			if (view_layer->flag & VIEW_LAYER_RENDER) {
				data->base_temp.next = view_layer->object_bases.first;
				data->iter.base = &data->base_temp;
				return;
			}
		}

		/* Setup the "set" for the next iteration. */
		data->scene_temp.set = data->scene;
		data->iter.set = &data->scene_temp;
		return;
	}

	/* Look for an object in the next set. */
	while ((data->iter.set = data->iter.set->set)) {
		ViewLayer *view_layer = BKE_view_layer_from_scene_get(data->iter.set);
		data->base_temp.next = view_layer->object_bases.first;
		data->iter.base = &data->base_temp;
		return;
	}

	iter->valid = false;
}

void BKE_renderable_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* Do nothing - iter->data was static allocated, we can't free it. */
}

/* Evaluation  */

/**
 * Reset props
 *
 * If props_ref is pasted, copy props from it
 */
static void idproperty_reset(IDProperty **props, IDProperty *props_ref)
{
	IDPropertyTemplate val = {0};

	if (*props) {
		IDP_FreeProperty(*props);
		MEM_freeN(*props);
	}
	*props = IDP_New(IDP_GROUP, &val, ROOT_PROP);

	if (props_ref) {
		IDP_MergeGroup(*props, props_ref, true);
	}
}

void BKE_layer_eval_layer_collection_pre(const struct EvaluationContext *UNUSED(eval_ctx),
                                         ID *owner_id, ViewLayer *view_layer)
{
	DEG_debug_print_eval(__func__, view_layer->name, view_layer);
	Scene *scene = (GS(owner_id->name) == ID_SCE) ? (Scene *)owner_id : NULL;

	for (Base *base = view_layer->object_bases.first; base != NULL; base = base->next) {
		base->flag &= ~(BASE_VISIBLED | BASE_SELECTABLED);
		idproperty_reset(&base->collection_properties, scene ? scene->collection_properties : NULL);
	}

	/* Sync properties from scene to scene layer. */
	idproperty_reset(&view_layer->properties_evaluated, scene ? scene->layer_properties : NULL);
	IDP_MergeGroup(view_layer->properties_evaluated, view_layer->properties, true);

	/* TODO(sergey): Is it always required? */
	view_layer->flag |= VIEW_LAYER_ENGINE_DIRTY;
}

static const char *collection_type_lookup[] =
{
    "None", /* COLLECTION_TYPE_NONE */
    "Group Internal", /* COLLECTION_TYPE_GROUP_INTERNAL */
};

/**
 * \note We can't use layer_collection->flag because of 3 level nesting (where parent is visible, but not grand-parent)
 * So layer_collection->flag_evaluated is expected to be up to date with layer_collection->flag.
 */
static bool layer_collection_visible_get(const EvaluationContext *eval_ctx, LayerCollection *layer_collection)
{
	if (layer_collection->flag_evaluated & COLLECTION_DISABLED) {
		return false;
	}

	if (eval_ctx->mode == DAG_EVAL_VIEWPORT) {
		return (layer_collection->flag_evaluated & COLLECTION_VIEWPORT) != 0;
	}
	else {
		return (layer_collection->flag_evaluated & COLLECTION_RENDER) != 0;
	}
}

void BKE_layer_eval_layer_collection(const EvaluationContext *eval_ctx,
                                     LayerCollection *layer_collection,
                                     LayerCollection *parent_layer_collection)
{
	if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
		/* TODO)sergey): Try to make it more generic and handled by depsgraph messaging. */
		printf("%s on %s (%p) [%s], parent %s (%p) [%s]\n",
		       __func__,
		       layer_collection->scene_collection->name,
		       layer_collection->scene_collection,
		       collection_type_lookup[layer_collection->scene_collection->type],
		       (parent_layer_collection != NULL) ? parent_layer_collection->scene_collection->name : "NONE",
		       (parent_layer_collection != NULL) ? parent_layer_collection->scene_collection : NULL,
		       (parent_layer_collection != NULL) ? collection_type_lookup[parent_layer_collection->scene_collection->type] : "");
	}
	BLI_assert(layer_collection != parent_layer_collection);

	/* visibility */
	layer_collection->flag_evaluated = layer_collection->flag;

	if (parent_layer_collection != NULL) {
		if (layer_collection_visible_get(eval_ctx, parent_layer_collection) == false) {
			layer_collection->flag_evaluated |= COLLECTION_DISABLED;
		}

		if ((parent_layer_collection->flag_evaluated & COLLECTION_DISABLED) ||
		    (parent_layer_collection->flag_evaluated & COLLECTION_SELECTABLE) == 0)
		{
			layer_collection->flag_evaluated &= ~COLLECTION_SELECTABLE;
		}
	}

	const bool is_visible = layer_collection_visible_get(eval_ctx, layer_collection);
	const bool is_selectable = is_visible && ((layer_collection->flag_evaluated & COLLECTION_SELECTABLE) != 0);

	/* overrides */
	if (is_visible) {
		if (parent_layer_collection == NULL) {
			idproperty_reset(&layer_collection->properties_evaluated, layer_collection->properties);
		}
		else {
			idproperty_reset(&layer_collection->properties_evaluated, parent_layer_collection->properties_evaluated);
			IDP_MergeGroup(layer_collection->properties_evaluated, layer_collection->properties, true);
		}
	}

	for (LinkData *link = layer_collection->object_bases.first; link != NULL; link = link->next) {
		Base *base = link->data;

		if (is_visible) {
			IDP_MergeGroup(base->collection_properties, layer_collection->properties_evaluated, true);
			base->flag |= BASE_VISIBLED;
		}

		if (is_selectable) {
			base->flag |= BASE_SELECTABLED;
		}
	}
}

void BKE_layer_eval_layer_collection_post(const struct EvaluationContext *UNUSED(eval_ctx),
                                          ViewLayer *view_layer)
{
	DEG_debug_print_eval(__func__, view_layer->name, view_layer);
	/* if base is not selectabled, clear select */
	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if ((base->flag & BASE_SELECTABLED) == 0) {
			base->flag &= ~BASE_SELECTED;
		}
	}
}

/**
 * Free any static allocated memory.
 */
void BKE_layer_exit(void)
{
	layer_collection_engine_settings_validate_free();
}
