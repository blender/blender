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

#include "BLI_array.h"
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
#include "BKE_object.h"

#include "DNA_group_types.h"
#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

/* prototype */
struct EngineSettingsCB_Type;
static void layer_collections_sync_flags(ListBase *layer_collections_dst, const ListBase *layer_collections_src);
static void layer_collection_free(ViewLayer *view_layer, LayerCollection *lc);
static void layer_collection_objects_populate(ViewLayer *view_layer, LayerCollection *lc, ListBase *objects);
static LayerCollection *layer_collection_add(ViewLayer *view_layer, LayerCollection *parent, SceneCollection *sc);
static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc, const SceneCollection *sc);
static void object_bases_iterator_next(BLI_Iterator *iter, const int flag);

/* RenderLayer */

/* Returns the default view layer to view in workspaces if there is
 * none linked to the workspace yet. */
ViewLayer *BKE_view_layer_default_view(const Scene *scene)
{
	/* TODO: it makes more sense to have the Viewport layer as the default,
	 * but this breaks view layer tests so change it later. */
#if 0
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		if (!(view_layer->flag & VIEW_LAYER_RENDER)) {
			return view_layer;
		}
	}

	BLI_assert(scene->view_layers.first);
	return scene->view_layers.first;
#else
	return BKE_view_layer_default_render(scene);
#endif
}

/* Returns the default view layer to render if we need to render just one. */
ViewLayer *BKE_view_layer_default_render(const Scene *scene)
{
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		if (view_layer->flag & VIEW_LAYER_RENDER) {
			return view_layer;
		}
	}

	BLI_assert(scene->view_layers.first);
	return scene->view_layers.first;
}

/**
 * Returns the ViewLayer to be used for drawing, outliner, and other context related areas.
 */
ViewLayer *BKE_view_layer_from_workspace_get(const struct Scene *scene, const struct WorkSpace *workspace)
{
	return BKE_workspace_view_layer_get(workspace, scene);
}

/**
 * This is a placeholder to know which areas of the code need to be addressed for the Workspace changes.
 * Never use this, you should either use BKE_view_layer_from_workspace_get or get ViewLayer explicitly.
 */
ViewLayer *BKE_view_layer_context_active_PLACEHOLDER(const Scene *scene)
{
	BLI_assert(scene->view_layers.first);
	return scene->view_layers.first;
}

static ViewLayer *view_layer_add(const char *name, SceneCollection *master_scene_collection)
{
	if (!name) {
		name = DATA_("View Layer");
	}

	ViewLayer *view_layer = MEM_callocN(sizeof(ViewLayer), "View Layer");
	view_layer->flag = VIEW_LAYER_RENDER | VIEW_LAYER_FREESTYLE;

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

	BLI_freelistN(&view_layer->object_bases);

	for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
		layer_collection_free(NULL, lc);
	}
	BLI_freelistN(&view_layer->layer_collections);

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

	MEM_SAFE_FREE(view_layer->object_bases_array);

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
	if (view_layer_dst->id_properties != NULL) {
		view_layer_dst->id_properties = IDP_CopyProperty_ex(view_layer_dst->id_properties, flag);
	}
	BKE_freestyle_config_copy(&view_layer_dst->freestyle_config, &view_layer_src->freestyle_config, flag);

	view_layer_dst->stats = NULL;

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

	view_layer_dst->object_bases_array = NULL;
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
	LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");

	lc->scene_collection = sc;
	lc->flag = COLLECTION_SELECTABLE | COLLECTION_VIEWPORT | COLLECTION_RENDER;

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
 * Return the first matching LayerCollection in the ViewLayer for the SceneCollection.
 */
LayerCollection *BKE_layer_collection_first_from_scene_collection(ViewLayer *view_layer, const SceneCollection *scene_collection)
{
	for (LayerCollection *layer_collection = view_layer->layer_collections.first;
	     layer_collection != NULL;
	     layer_collection = layer_collection->next)
	{
		LayerCollection *found = find_layer_collection_by_scene_collection(layer_collection, scene_collection);

		if (found != NULL) {
			return found;
		}
	}
	return NULL;
}

/**
 * See if view layer has the scene collection linked directly, or indirectly (nested)
 */
bool BKE_view_layer_has_collection(ViewLayer *view_layer, const SceneCollection *scene_collection)
{
	return BKE_layer_collection_first_from_scene_collection(view_layer, scene_collection) != NULL;
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

/** \} */

/* Iterators */

/* -------------------------------------------------------------------- */
/** \name Private Iterator Helpers
 * \{ */

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

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_objects_iterator
 * See: #FOREACH_SELECTED_OBJECT_BEGIN
 * \{ */

void BKE_view_layer_selected_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	objects_iterator_begin(iter, data_in, BASE_SELECTED);
}

void BKE_view_layer_selected_objects_iterator_next(BLI_Iterator *iter)
{
	objects_iterator_next(iter, BASE_SELECTED);
}

void BKE_view_layer_selected_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_objects_iterator
 * \{ */

void BKE_view_layer_visible_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	objects_iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_view_layer_visible_objects_iterator_next(BLI_Iterator *iter)
{
	objects_iterator_next(iter, BASE_VISIBLED);
}

void BKE_view_layer_visible_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_bases_iterator
 * \{ */

void BKE_view_layer_selected_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	object_bases_iterator_begin(iter, data_in, BASE_SELECTED);
}

void BKE_view_layer_selected_bases_iterator_next(BLI_Iterator *iter)
{
	object_bases_iterator_next(iter, BASE_SELECTED);
}

void BKE_view_layer_selected_bases_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_visible_bases_iterator
 * \{ */

void BKE_view_layer_visible_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	object_bases_iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_view_layer_visible_bases_iterator_next(BLI_Iterator *iter)
{
	object_bases_iterator_next(iter, BASE_VISIBLED);
}

void BKE_view_layer_visible_bases_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_renderable_objects_iterator
 * \{ */

void BKE_view_layer_renderable_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
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
	BKE_view_layer_renderable_objects_iterator_next(iter);
}

void BKE_view_layer_renderable_objects_iterator_next(BLI_Iterator *iter)
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
		ViewLayer *view_layer = BKE_view_layer_default_render(data->iter.set);
		data->base_temp.next = view_layer->object_bases.first;
		data->iter.base = &data->base_temp;
		return;
	}

	iter->valid = false;
}

void BKE_view_layer_renderable_objects_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* Do nothing - iter->data was static allocated, we can't free it. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_bases_in_mode_iterator
 * \{ */

void BKE_view_layer_bases_in_mode_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	struct ObjectsInModeIteratorData *data = data_in;
	Base *base = data->base_active;

	/* when there are no objects */
	if (base == NULL) {
		iter->valid = false;
		return;
	}
	iter->data = data_in;
	iter->current = base;
}

void BKE_view_layer_bases_in_mode_iterator_next(BLI_Iterator *iter)
{
	struct ObjectsInModeIteratorData *data = iter->data;
	Base *base = iter->current;

	if (base == data->base_active) {
		/* first step */
		base = data->view_layer->object_bases.first;
		if (base == data->base_active) {
			base = base->next;
		}
	}
	else {
		base = base->next;
	}

	while (base) {
		if ((base->flag & BASE_SELECTED) != 0 &&
		    (base->object->type == data->base_active->object->type) &&
		    (base != data->base_active) &&
		    (base->object->mode & data->object_mode))
		{
			iter->current = base;
			return;
		}
		base = base->next;
	}
	iter->valid = false;
}

void BKE_view_layer_bases_in_mode_iterator_end(BLI_Iterator *UNUSED(iter))
{
	/* do nothing */
}

/** \} */

/* Evaluation  */

static void layer_eval_layer_collection_pre(Depsgraph *depsgraph, ID *UNUSED(owner_id), ViewLayer *view_layer)
{
	DEG_debug_print_eval(depsgraph, __func__, view_layer->name, view_layer);
	//Scene *scene = (GS(owner_id->name) == ID_SCE) ? (Scene *)owner_id : NULL;

	for (Base *base = view_layer->object_bases.first; base != NULL; base = base->next) {
		base->flag &= ~(BASE_VISIBLED | BASE_SELECTABLED);
	}

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
static bool layer_collection_visible_get(Depsgraph *depsgraph, LayerCollection *layer_collection)
{
	if (layer_collection->flag_evaluated & COLLECTION_DISABLED) {
		return false;
	}

	if (DEG_get_mode(depsgraph) == DAG_EVAL_VIEWPORT) {
		return (layer_collection->flag_evaluated & COLLECTION_VIEWPORT) != 0;
	}
	else {
		return (layer_collection->flag_evaluated & COLLECTION_RENDER) != 0;
	}
}

static void layer_eval_layer_collection(Depsgraph *depsgraph,
                                        LayerCollection *layer_collection,
                                        LayerCollection *parent_layer_collection)
{
	DEG_debug_print_eval_parent_typed(
	        depsgraph,
	        __func__,
	        layer_collection->scene_collection->name,
	        layer_collection->scene_collection,
	        collection_type_lookup[layer_collection->scene_collection->type],
	        "parent",
	        (parent_layer_collection != NULL) ? parent_layer_collection->scene_collection->name : "NONE",
	        (parent_layer_collection != NULL) ? parent_layer_collection->scene_collection : NULL,
	        (parent_layer_collection != NULL) ? collection_type_lookup[parent_layer_collection->scene_collection->type] : "");
	BLI_assert(layer_collection != parent_layer_collection);

	/* visibility */
	layer_collection->flag_evaluated = layer_collection->flag;

	if (parent_layer_collection != NULL) {
		if (layer_collection_visible_get(depsgraph, parent_layer_collection) == false) {
			layer_collection->flag_evaluated |= COLLECTION_DISABLED;
		}

		if ((parent_layer_collection->flag_evaluated & COLLECTION_DISABLED) ||
		    (parent_layer_collection->flag_evaluated & COLLECTION_SELECTABLE) == 0)
		{
			layer_collection->flag_evaluated &= ~COLLECTION_SELECTABLE;
		}
	}

	const bool is_visible = layer_collection_visible_get(depsgraph, layer_collection);
	const bool is_selectable = is_visible && ((layer_collection->flag_evaluated & COLLECTION_SELECTABLE) != 0);

	for (LinkData *link = layer_collection->object_bases.first; link != NULL; link = link->next) {
		Base *base = link->data;

		if (is_visible) {
			base->flag |= BASE_VISIBLED;
		}

		if (is_selectable) {
			base->flag |= BASE_SELECTABLED;
		}
	}
}

static void layer_eval_layer_collection_post(Depsgraph *depsgraph, ViewLayer *view_layer)
{
	DEG_debug_print_eval(depsgraph, __func__, view_layer->name, view_layer);
	/* Create array of bases, for fast index-based lookup. */
	const int num_object_bases = BLI_listbase_count(&view_layer->object_bases);
	MEM_SAFE_FREE(view_layer->object_bases_array);
	view_layer->object_bases_array = MEM_malloc_arrayN(
	        num_object_bases, sizeof(Base *), "view_layer->object_bases_array");
	int base_index = 0;
	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		/* if base is not selectabled, clear select. */
		if ((base->flag & BASE_SELECTABLED) == 0) {
			base->flag &= ~BASE_SELECTED;
		}
		/* Store base in the array. */
		view_layer->object_bases_array[base_index++] = base;
	}
}

static void layer_eval_collections_recurse(Depsgraph *depsgraph,
                                           ListBase *layer_collections,
                                           LayerCollection *parent_layer_collection)
{
	for (LayerCollection *layer_collection = layer_collections->first;
	     layer_collection != NULL;
	     layer_collection = layer_collection->next)
	{
		layer_eval_layer_collection(depsgraph,
		                            layer_collection,
		                            parent_layer_collection);
		layer_eval_collections_recurse(depsgraph,
		                               &layer_collection->layer_collections,
		                               layer_collection);
	}
}

void BKE_layer_eval_view_layer(struct Depsgraph *depsgraph,
                               struct ID *owner_id,
                               ViewLayer *view_layer)
{
	layer_eval_layer_collection_pre(depsgraph, owner_id, view_layer);
	layer_eval_collections_recurse(depsgraph,
	                               &view_layer->layer_collections,
	                               NULL);
	layer_eval_layer_collection_post(depsgraph, view_layer);
}

void BKE_layer_eval_view_layer_indexed(struct Depsgraph *depsgraph,
                                       struct ID *owner_id,
                                       int view_layer_index)
{
	BLI_assert(GS(owner_id->name) == ID_SCE);
	BLI_assert(view_layer_index >= 0);
	Scene *scene = (Scene *)owner_id;
	ViewLayer *view_layer = BLI_findlink(&scene->view_layers, view_layer_index);
	BLI_assert(view_layer != NULL);
	BKE_layer_eval_view_layer(depsgraph, owner_id, view_layer);
}
