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
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_workspace.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

#define DEBUG_PRINT if (G.debug & G_DEBUG_DEPSGRAPH) printf

/* prototype */
struct EngineSettingsCB_Type;
static void layer_collection_free(SceneLayer *sl, LayerCollection *lc);
static LayerCollection *layer_collection_add(SceneLayer *sl, LayerCollection *parent, SceneCollection *sc);
static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc, const SceneCollection *sc);
static IDProperty *collection_engine_settings_create(struct EngineSettingsCB_Type *ces_type, const bool populate);
static IDProperty *collection_engine_get(IDProperty *root, const int type, const char *engine_name);
static void collection_engine_settings_init(IDProperty *root, const bool populate);
static void layer_engine_settings_init(IDProperty *root, const bool populate);
static void object_bases_iterator_next(BLI_Iterator *iter, const int flag);

/* RenderLayer */

/**
 * Returns the SceneLayer to be used for rendering
 * Most of the time BKE_scene_layer_context_active should be used instead
 */
SceneLayer *BKE_scene_layer_render_active(const Scene *scene)
{
	SceneLayer *sl = BLI_findlink(&scene->render_layers, scene->active_layer);
	BLI_assert(sl);
	return sl;
}

/**
 * Returns the SceneLayer to be used for drawing, outliner, and other context related areas.
 */
SceneLayer *BKE_scene_layer_context_active_ex(const Main *bmain, const Scene *UNUSED(scene))
{
	/* XXX We should really pass the workspace as argument, but would require
	 * some bigger changes since it's often not available where we call this.
	 * Just working around this by getting active window from WM for now */
	for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
		/* Called on startup, so 'winactive' may not be set, in that case fall back to first window. */
		wmWindow *win = wm->winactive ? wm->winactive : wm->windows.first;
		const WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
		return BKE_workspace_render_layer_get(workspace);
	}

	return NULL;
}
SceneLayer *BKE_scene_layer_context_active(const Scene *scene)
{
	return BKE_scene_layer_context_active_ex(G.main, scene);
}

/**
 * Add a new renderlayer
 * by default, a renderlayer has the master collection
 */
SceneLayer *BKE_scene_layer_add(Scene *scene, const char *name)
{
	if (!name) {
		name = DATA_("Render Layer");
	}

	IDPropertyTemplate val = {0};
	SceneLayer *sl = MEM_callocN(sizeof(SceneLayer), "Scene Layer");
	sl->flag |= SCENE_LAYER_RENDER;

	sl->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	layer_engine_settings_init(sl->properties, false);

	BLI_addtail(&scene->render_layers, sl);

	/* unique name */
	BLI_strncpy_utf8(sl->name, name, sizeof(sl->name));
	BLI_uniquename(&scene->render_layers, sl, DATA_("SceneLayer"), '.', offsetof(SceneLayer, name), sizeof(sl->name));

	SceneCollection *sc = BKE_collection_master(scene);
	layer_collection_add(sl, NULL, sc);

	return sl;
}

/**
 * Free (or release) any data used by this SceneLayer.
 */
void BKE_scene_layer_free(SceneLayer *sl)
{
	sl->basact = NULL;

	for (Base *base = sl->object_bases.first; base; base = base->next) {
		if (base->collection_properties) {
			IDP_FreeProperty(base->collection_properties);
			MEM_freeN(base->collection_properties);
		}
	}
	BLI_freelistN(&sl->object_bases);

	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
		layer_collection_free(NULL, lc);
	}
	BLI_freelistN(&sl->layer_collections);

	if (sl->properties) {
		IDP_FreeProperty(sl->properties);
		MEM_freeN(sl->properties);
	}

	if (sl->properties_evaluated) {
		IDP_FreeProperty(sl->properties_evaluated);
		MEM_freeN(sl->properties_evaluated);
	}

	for (SceneLayerEngineData *sled = sl->drawdata.first; sled; sled = sled->next) {
		if (sled->storage) {
			if (sled->free) {
				sled->free(sled->storage);
			}
			MEM_freeN(sled->storage);
		}
	}
	BLI_freelistN(&sl->drawdata);

	MEM_SAFE_FREE(sl->stats);

	MEM_freeN(sl);
}

/**
 * Set the render engine of a renderlayer
 */
void BKE_scene_layer_engine_set(SceneLayer *sl, const char *engine)
{
	BLI_strncpy_utf8(sl->engine, engine, sizeof(sl->engine));
}

/**
 * Tag all the selected objects of a renderlayer
 */
void BKE_scene_layer_selected_objects_tag(SceneLayer *sl, const int tag)
{
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		if ((base->flag & BASE_SELECTED) != 0) {
			base->object->flag |= tag;
		}
		else {
			base->object->flag &= ~tag;
		}
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
 * Find the SceneLayer a LayerCollection belongs to
 */
SceneLayer *BKE_scene_layer_find_from_collection(const Scene *scene, LayerCollection *lc)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		if (find_scene_collection_in_scene_collections(&sl->layer_collections, lc)) {
			return sl;
		}
	}
	return NULL;
}

/* Base */

Base *BKE_scene_layer_base_find(SceneLayer *sl, Object *ob)
{
	return BLI_findptr(&sl->object_bases, ob, offsetof(Base, object));
}

void BKE_scene_layer_base_deselect_all(SceneLayer *sl)
{
	Base *base;

	for (base = sl->object_bases.first; base; base = base->next) {
		base->flag &= ~BASE_SELECTED;
	}
}

void BKE_scene_layer_base_select(struct SceneLayer *sl, Base *selbase)
{
	sl->basact = selbase;
	if ((selbase->flag & BASE_SELECTABLED) != 0) {
		selbase->flag |= BASE_SELECTED;
	}
}

static void scene_layer_object_base_unref(SceneLayer *sl, Base *base)
{
	base->refcount--;

	/* It only exists in the RenderLayer */
	if (base->refcount == 0) {
		if (sl->basact == base) {
			sl->basact = NULL;
		}

		if (base->collection_properties) {
			IDP_FreeProperty(base->collection_properties);
			MEM_freeN(base->collection_properties);
		}

		BLI_remlink(&sl->object_bases, base);
		MEM_freeN(base);
	}
}

/**
 * Return the base if existent, or create it if necessary
 * Always bump the refcount
 */
static Base *object_base_add(SceneLayer *sl, Object *ob)
{
	Base *base;
	base = BKE_scene_layer_base_find(sl, ob);

	if (base == NULL) {
		base = MEM_callocN(sizeof(Base), "Object Base");

		/* Do not bump user count, leave it for SceneCollections. */
		base->object = ob;
		BLI_addtail(&sl->object_bases, base);

		IDPropertyTemplate val = {0};
		base->collection_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	}

	base->refcount++;
	return base;
}

/* LayerCollection */

/**
 * When freeing the entire SceneLayer at once we don't bother with unref
 * otherwise SceneLayer is passed to keep the syncing of the LayerCollection tree
 */
static void layer_collection_free(SceneLayer *sl, LayerCollection *lc)
{
	if (sl) {
		for (LinkData *link = lc->object_bases.first; link; link = link->next) {
			scene_layer_object_base_unref(sl, link->data);
		}
	}

	BLI_freelistN(&lc->object_bases);
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
		layer_collection_free(sl, nlc);
	}

	BLI_freelistN(&lc->layer_collections);
}

/**
 * Free (or release) LayerCollection from SceneLayer
 * (does not free the LayerCollection itself).
 */
void BKE_layer_collection_free(SceneLayer *sl, LayerCollection *lc)
{
	layer_collection_free(sl, lc);
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
 * Get the active collection
 */
LayerCollection *BKE_layer_collection_get_active(SceneLayer *sl)
{
	int i = 0;
	return collection_from_index(&sl->layer_collections, sl->active_collection, &i);
}


/**
 * Return layer collection to add new object(s).
 * Create one if none exists.
 */
LayerCollection *BKE_layer_collection_get_active_ensure(Scene *scene, SceneLayer *sl)
{
	LayerCollection *lc = BKE_layer_collection_get_active(sl);

	if (lc == NULL) {
		BLI_assert(BLI_listbase_is_empty(&sl->layer_collections));
		/* When there is no collection linked to this SceneLayer, create one. */
		SceneCollection *sc = BKE_collection_add(scene, NULL, NULL);
		lc = BKE_collection_link(sl, sc);
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
int BKE_layer_collection_count(SceneLayer *sl)
{
	return collection_count(&sl->layer_collections);
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
int BKE_layer_collection_findindex(SceneLayer *sl, const LayerCollection *lc)
{
	int i = 0;
	return index_from_collection(&sl->layer_collections, lc, &i);
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
 * Both collections must be from the same SceneLayer, return true if succeded.
 *
 * The LayerCollection will effectively be moved into the
 * new (nested) position. So all the settings, overrides, ... go with it, and
 * if the collection was directly linked to the SceneLayer it's then unlinked.
 *
 * For the other SceneLayers we simply resync the tree, without changing directly
 * linked collections (even if they link to the same SceneCollection)
 *
 * \param lc_src LayerCollection to nest into \a lc_dst
 * \param lc_dst LayerCollection to have \a lc_src inserted into
 */

static void layer_collection_swap(
        SceneLayer *sl, ListBase *lb_a, ListBase *lb_b,
        LayerCollection *lc_a, LayerCollection *lc_b)
{
	if (lb_a == NULL) {
		lb_a = layer_collection_listbase_find(&sl->layer_collections, lc_a);
	}

	if (lb_b == NULL) {
		lb_b = layer_collection_listbase_find(&sl->layer_collections, lc_b);
	}

	BLI_assert(lb_a);
	BLI_assert(lb_b);

	BLI_listbases_swaplinks(lb_a, lb_b, lc_a, lc_b);
}

/**
 * Move \a lc_src into \a lc_dst. Both have to be stored in \a sl.
 * If \a lc_src is directly linked to the SceneLayer it's unlinked
 */
bool BKE_layer_collection_move_into(const Scene *scene, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	SceneLayer *sl = BKE_scene_layer_find_from_collection(scene, lc_src);
	bool is_directly_linked = false;

	if ((!sl) || (sl != BKE_scene_layer_find_from_collection(scene, lc_dst))) {
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
		layer_collection_swap(sl, &lc_dst->layer_collections, NULL, lc_dst->layer_collections.last, lc_src);

		if (BLI_findindex(&sl->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(sl, lc_swap);
		}
		return true;
	}
	else {
		LayerCollection *lc_temp;
		is_directly_linked = BLI_findindex(&sl->layer_collections, lc_src) != -1;

		if (!is_directly_linked) {
			/* lc_src will be invalid after BKE_collection_move_into!
			 * so we swap it with lc_temp to preserve its settings */
			lc_temp = BKE_collection_link(sl, lc_src->scene_collection);
			layer_collection_swap(sl, &sl->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_into(scene, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked) {
				/* Swap back and remove */
				layer_collection_swap(sl, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(sl, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = BLI_findptr(&lc_dst->layer_collections, lc_src->scene_collection, offsetof(LayerCollection, scene_collection));
	BLI_assert(lc_new);
	layer_collection_swap(sl, &lc_dst->layer_collections, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&sl->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(sl, lc_new);
	}

	return true;
}

/**
 * Move \a lc_src above \a lc_dst. Both have to be stored in \a sl.
 * If \a lc_src is directly linked to the SceneLayer it's unlinked
 */
bool BKE_layer_collection_move_above(const Scene *scene, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	SceneLayer *sl = BKE_scene_layer_find_from_collection(scene, lc_src);
	const bool is_directly_linked_src = BLI_findindex(&sl->layer_collections, lc_src) != -1;
	const bool is_directly_linked_dst = BLI_findindex(&sl->layer_collections, lc_dst) != -1;

	if ((!sl) || (sl != BKE_scene_layer_find_from_collection(scene, lc_dst))) {
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
		layer_collection_swap(sl, NULL, NULL, lc_dst->prev, lc_src);

		if (BLI_findindex(&sl->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(sl, lc_swap);
		}
		return true;
	}
	/* We don't allow to move above/below a directly linked collection
	 * unless the source collection is also directly linked */
	else if (is_directly_linked_dst) {
		/* Both directly linked to the SceneLayer, just need to swap */
		if (is_directly_linked_src) {
			BLI_remlink(&sl->layer_collections, lc_src);
			BLI_insertlinkbefore(&sl->layer_collections, lc_dst, lc_src);
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
			lc_temp = BKE_collection_link(sl, lc_src->scene_collection);
			layer_collection_swap(sl, &sl->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_above(scene, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked_src) {
				/* Swap back and remove */
				layer_collection_swap(sl, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(sl, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = lc_dst->prev;
	BLI_assert(lc_new);
	layer_collection_swap(sl, NULL, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&sl->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(sl, lc_new);
	}

	return true;
}

/**
 * Move \a lc_src below \a lc_dst. Both have to be stored in \a sl.
 * If \a lc_src is directly linked to the SceneLayer it's unlinked
 */
bool BKE_layer_collection_move_below(const Scene *scene, LayerCollection *lc_dst, LayerCollection *lc_src)
{
	SceneLayer *sl = BKE_scene_layer_find_from_collection(scene, lc_src);
	const bool is_directly_linked_src = BLI_findindex(&sl->layer_collections, lc_src) != -1;
	const bool is_directly_linked_dst = BLI_findindex(&sl->layer_collections, lc_dst) != -1;

	if ((!sl) || (sl != BKE_scene_layer_find_from_collection(scene, lc_dst))) {
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
		layer_collection_swap(sl, NULL, NULL, lc_dst->next, lc_src);

		if (BLI_findindex(&sl->layer_collections, lc_swap) != -1) {
			BKE_collection_unlink(sl, lc_swap);
		}
		return true;
	}
	/* We don't allow to move above/below a directly linked collection
	 * unless the source collection is also directly linked */
	else if (is_directly_linked_dst) {
		/* Both directly linked to the SceneLayer, just need to swap */
		if (is_directly_linked_src) {
			BLI_remlink(&sl->layer_collections, lc_src);
			BLI_insertlinkafter(&sl->layer_collections, lc_dst, lc_src);
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
			lc_temp = BKE_collection_link(sl, lc_src->scene_collection);
			layer_collection_swap(sl, &sl->layer_collections, NULL, lc_temp, lc_src);
		}

		if (!BKE_collection_move_below(scene, lc_dst->scene_collection, lc_src->scene_collection)) {
			if (!is_directly_linked_src) {
				/* Swap back and remove */
				layer_collection_swap(sl, NULL, NULL, lc_temp, lc_src);
				BKE_collection_unlink(sl, lc_temp);
			}
			return false;
		}
	}

	LayerCollection *lc_new = lc_dst->next;
	BLI_assert(lc_new);
	layer_collection_swap(sl, NULL, NULL, lc_new, lc_src);

	/* If it's directly linked, unlink it after the swap */
	if (BLI_findindex(&sl->layer_collections, lc_new) != -1) {
		BKE_collection_unlink(sl, lc_new);
	}

	return true;
}

static bool layer_collection_resync(SceneLayer *sl, LayerCollection *lc, const SceneCollection *sc)
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
				layer_collection_add(sl, lc, sc_nested);
			}
		}

		for (LayerCollection *lc_nested = collections.first; lc_nested; lc_nested = lc_nested->next) {
			layer_collection_free(sl, lc_nested);
		}
		BLI_freelistN(&collections);

		BLI_assert(BLI_listbase_count(&lc->layer_collections) ==
		           BLI_listbase_count(&sc->scene_collections));

		return true;
	}

	for (LayerCollection *lc_nested = lc->layer_collections.first; lc_nested; lc_nested = lc_nested->next) {
		if (layer_collection_resync(sl, lc_nested, sc)) {
			return true;
		}
	}

	return false;
}

/**
 * Update the scene layers so that any LayerCollection that points
 * to \a sc is re-synced again
 */
void BKE_layer_collection_resync(const Scene *scene, const SceneCollection *sc)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			layer_collection_resync(sl, lc, sc);
		}
	}
}

/* ---------------------------------------------------------------------- */

/**
 * Link a collection to a renderlayer
 * The collection needs to be created separately
 */
LayerCollection *BKE_collection_link(SceneLayer *sl, SceneCollection *sc)
{
	LayerCollection *lc = layer_collection_add(sl, NULL, sc);
	sl->active_collection = BKE_layer_collection_findindex(sl, lc);
	return lc;
}

/**
 * Unlink a collection base from a renderlayer
 * The corresponding collection is not removed from the master collection
 */
void BKE_collection_unlink(SceneLayer *sl, LayerCollection *lc)
{
	BKE_layer_collection_free(sl, lc);
	BLI_remlink(&sl->layer_collections, lc);
	MEM_freeN(lc);
	sl->active_collection = 0;
}

static void layer_collection_object_add(SceneLayer *sl, LayerCollection *lc, Object *ob)
{
	Base *base = object_base_add(sl, ob);

	/* Only add an object once - prevent SceneCollection->objects and
	 * SceneCollection->filter_objects to add the same object. */

	if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data))) {
		return;
	}

	bool is_visible = (lc->flag & COLLECTION_VISIBLE) != 0;
	bool is_selectable = is_visible && ((lc->flag & COLLECTION_SELECTABLE) != 0);

	if (is_visible) {
		base->flag |= BASE_VISIBLED;
	}

	if (is_selectable) {
		base->flag |= BASE_SELECTABLED;
	}

	BLI_addtail(&lc->object_bases, BLI_genericNodeN(base));
}

static void layer_collection_object_remove(SceneLayer *sl, LayerCollection *lc, Object *ob)
{
	Base *base;
	base = BKE_scene_layer_base_find(sl, ob);

	LinkData *link = BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data));
	BLI_remlink(&lc->object_bases, link);
	MEM_freeN(link);

	scene_layer_object_base_unref(sl, base);
}

static void layer_collection_objects_populate(SceneLayer *sl, LayerCollection *lc, ListBase *objects)
{
	for (LinkData *link = objects->first; link; link = link->next) {
		layer_collection_object_add(sl, lc, link->data);
	}
}

static void layer_collection_populate(SceneLayer *sl, LayerCollection *lc, SceneCollection *sc)
{
	layer_collection_objects_populate(sl, lc, &sc->objects);
	layer_collection_objects_populate(sl, lc, &sc->filter_objects);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		layer_collection_add(sl, lc, nsc);
	}
}

static LayerCollection *layer_collection_add(SceneLayer *sl, LayerCollection *parent, SceneCollection *sc)
{
	IDPropertyTemplate val = {0};
	LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");

	lc->scene_collection = sc;
	lc->flag = COLLECTION_VISIBLE | COLLECTION_SELECTABLE;

	lc->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
	collection_engine_settings_init(lc->properties, false);

	if (parent != NULL) {
		BLI_addtail(&parent->layer_collections, lc);
	}
	else {
		BLI_addtail(&sl->layer_collections, lc);
	}

	layer_collection_populate(sl, lc, sc);

	return lc;
}

/* ---------------------------------------------------------------------- */

/**
 * See if render layer has the scene collection linked directly, or indirectly (nested)
 */
bool BKE_scene_layer_has_collection(SceneLayer *sl, const SceneCollection *sc)
{
	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
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
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		Base *base = BKE_scene_layer_base_find(sl, ob);
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
 * Add a new LayerCollection for all the SceneLayers that have sc_parent
 */
void BKE_layer_sync_new_scene_collection(Scene *scene, const SceneCollection *sc_parent, SceneCollection *sc)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *lc_parent = find_layer_collection_by_scene_collection(lc, sc_parent);
			if (lc_parent) {
				layer_collection_add(sl, lc_parent, sc);
			}
		}
	}
}

/**
 * Add a corresponding ObjectBase to all the equivalent LayerCollection
 */
void BKE_layer_sync_object_link(const Scene *scene, SceneCollection *sc, Object *ob)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *found = find_layer_collection_by_scene_collection(lc, sc);
			if (found) {
				layer_collection_object_add(sl, found, ob);
			}
		}
	}
}

/**
 * Remove the equivalent object base to all layers that have this collection
 * also remove all reference to ob in the filter_objects
 */
void BKE_layer_sync_object_unlink(const Scene *scene, SceneCollection *sc, Object *ob)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *found = find_layer_collection_by_scene_collection(lc, sc);
			if (found) {
				layer_collection_object_remove(sl, found, ob);
			}
		}
	}
}

/* ---------------------------------------------------------------------- */
/* Override */

/**
 * Add a new datablock override
 */
void BKE_collection_override_datablock_add(LayerCollection *UNUSED(lc), const char *UNUSED(data_path), ID *UNUSED(id))
{
	TODO_LAYER_OVERRIDE;
}

/* ---------------------------------------------------------------------- */
/* Engine Settings */

ListBase R_layer_collection_engines_settings_callbacks = {NULL, NULL};
ListBase R_scene_layer_engines_settings_callbacks = {NULL, NULL};

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

static void create_scene_layer_engine_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
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

	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			create_layer_collection_engine_settings_collection(lc, es_type);
		}
	}
}

static void create_scene_layer_engines_settings_scene(Scene *scene, EngineSettingsCB_Type *es_type)
{
	/* Populate the scene with the new settings. */
	create_scene_layer_engine_settings_scene(scene, es_type);
}

static void create_scene_layer_engines_settings_layer(SceneLayer *sl, EngineSettingsCB_Type *es_type)
{
	if (BKE_scene_layer_engine_layer_get(sl, COLLECTION_MODE_NONE, es_type->name)) {
		return;
	}

	IDProperty *props = collection_engine_settings_create(es_type, false);
	IDP_AddToGroup(sl->properties, props);
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

void BKE_scene_layer_engine_settings_callback_register(
        Main *bmain, const char *engine_name, EngineSettingsCB func)
{
	EngineSettingsCB_Type *es_type =
	        engine_settings_callback_register(engine_name, func, &R_scene_layer_engines_settings_callbacks);

	if (bmain) {
		/* Populate all of the collections of the scene with those settings. */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			create_scene_layer_engines_settings_scene(scene, es_type);

			for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
				create_scene_layer_engines_settings_layer(sl, es_type);
			}
		}
	}
}

void BKE_layer_collection_engine_settings_callback_free(void)
{
	BLI_freelistN(&R_layer_collection_engines_settings_callbacks);
}

void BKE_scene_layer_engine_settings_callback_free(void)
{
	BLI_freelistN(&R_scene_layer_engines_settings_callbacks);
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

static void scene_layer_create_render_settings(IDProperty *root, const bool populate)
{
	EngineSettingsCB_Type *es_type;
	for (es_type = R_scene_layer_engines_settings_callbacks.first; es_type; es_type = es_type->next) {
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
IDProperty *BKE_scene_layer_engine_scene_get(Scene *scene, const int type, const char *engine_name)
{
	return collection_engine_get(scene->layer_properties, type, engine_name);
}

/**
 * Return scene layer engine settings for specified engine
 */
IDProperty *BKE_scene_layer_engine_layer_get(SceneLayer *sl, const int type, const char *engine_name)
{
	return collection_engine_get(sl->properties, type, engine_name);
}

/**
 * Return scene layer evaluated engine settings for specified engine
 */
IDProperty *BKE_scene_layer_engine_evaluated_get(SceneLayer *sl, const int type, const char *engine_name)
{
	return collection_engine_get(sl->properties_evaluated, type, engine_name);
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

	IDProperty *idprop= IDP_New(IDP_ARRAY, &val, name);
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
	scene_layer_create_render_settings(root, populate);

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
void BKE_scene_layer_engine_settings_create(IDProperty *root)
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
	IDProperty *scene_layer;
	IDProperty *layer_collection;
} root_reference = {
	.scene = {NULL, NULL},
	.scene_layer = NULL,
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

	if (root_reference.scene_layer == NULL) {
		root_reference.scene_layer = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		layer_engine_settings_init(root_reference.scene_layer, false);
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
	    root_reference.scene_layer,
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
void BKE_scene_layer_engine_settings_validate_scene(Scene *scene)
{
	if (root_reference.scene.render_settings == NULL) {
		engine_settings_validate_init();
	}

	if (scene->layer_properties == NULL) {
		IDPropertyTemplate val = {0};
		scene->layer_properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		BKE_scene_layer_engine_settings_create(scene->layer_properties);
	}
	else {
		IDP_MergeGroup(scene->layer_properties, root_reference.scene.render_settings, false);
	}
}

/**
 * Make sure Scene has all required collection settings.
 */
void BKE_scene_layer_engine_settings_validate_layer(SceneLayer *sl)
{
	if (root_reference.scene_layer == NULL) {
		engine_settings_validate_init();
	}

	IDP_MergeGroup(sl->properties, root_reference.scene_layer, false);
}

/* ---------------------------------------------------------------------- */
/* Iterators */

static void object_bases_iterator_begin(BLI_Iterator *iter, void *data_in, const int flag)
{
	SceneLayer *sl = data_in;
	Base *base = sl->object_bases.first;

	/* when there are no objects */
	if (base ==  NULL) {
		iter->valid = false;
		return;
	}

	iter->valid = true;
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

	iter->current = NULL;
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

void BKE_layer_eval_layer_collection_pre(struct EvaluationContext *UNUSED(eval_ctx),
                                         Scene *scene, SceneLayer *scene_layer)
{
	DEBUG_PRINT("%s on %s\n", __func__, scene_layer->name);
	for (Base *base = scene_layer->object_bases.first; base != NULL; base = base->next) {
		base->flag &= ~(BASE_VISIBLED | BASE_SELECTABLED);
		idproperty_reset(&base->collection_properties, scene->collection_properties);
	}

	/* Sync properties from scene to scene layer. */
	idproperty_reset(&scene_layer->properties_evaluated, scene->layer_properties);
	IDP_MergeGroup(scene_layer->properties_evaluated, scene_layer->properties, true);

	/* TODO(sergey): Is it always required? */
	scene_layer->flag |= SCENE_LAYER_ENGINE_DIRTY;
}

void BKE_layer_eval_layer_collection(struct EvaluationContext *UNUSED(eval_ctx),
                                     LayerCollection *layer_collection,
                                     LayerCollection *parent_layer_collection)
{
	DEBUG_PRINT("%s on %s, parent %s\n",
	            __func__,
	            layer_collection->scene_collection->name,
	            (parent_layer_collection != NULL) ? parent_layer_collection->scene_collection->name : "NONE");

	/* visibility */
	layer_collection->flag_evaluated = layer_collection->flag;
	bool is_visible = (layer_collection->flag & COLLECTION_VISIBLE) != 0;
	bool is_selectable = is_visible && ((layer_collection->flag & COLLECTION_SELECTABLE) != 0);

	if (parent_layer_collection != NULL) {
		is_visible &= (parent_layer_collection->flag_evaluated & COLLECTION_VISIBLE) != 0;
		is_selectable &= (parent_layer_collection->flag_evaluated & COLLECTION_SELECTABLE) != 0;
		layer_collection->flag_evaluated &= parent_layer_collection->flag_evaluated;
	}

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

void BKE_layer_eval_layer_collection_post(struct EvaluationContext *UNUSED(eval_ctx),
                                          SceneLayer *scene_layer)
{
	DEBUG_PRINT("%s on %s\n", __func__, scene_layer->name);
	/* if base is not selectabled, clear select */
	for (Base *base = scene_layer->object_bases.first; base; base = base->next) {
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
