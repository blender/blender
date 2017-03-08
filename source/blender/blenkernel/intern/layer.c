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
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

/* prototype */
struct CollectionEngineSettingsCB_Type;
static void layer_collection_free(SceneLayer *sl, LayerCollection *lc);
static LayerCollection *layer_collection_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc);
static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc, const SceneCollection *sc);
static CollectionEngineSettings *collection_engine_settings_create(struct CollectionEngineSettingsCB_Type *ces_type);
static void layer_collection_engine_settings_free(LayerCollection *lc);
static void layer_collection_create_engine_settings(LayerCollection *lc);
static void layer_collection_create_mode_settings(LayerCollection *lc);
static void scene_layer_engine_settings_update(SceneLayer *sl, Object *ob);
static void object_bases_Iterator_next(Iterator *iter, const int flag);

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
 * Returns the SceneLayer to be used for drawing, outliner, and
 * other context related areas.
 */
SceneLayer *BKE_scene_layer_context_active(Scene *scene)
{
	/* waiting for workspace to get the layer from context*/
	TODO_LAYER_CONTEXT;
	return BKE_scene_layer_render_active(scene);
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

	SceneLayer *sl = MEM_callocN(sizeof(SceneLayer), "Scene Layer");
	sl->flag |= SCENE_LAYER_RENDER;

	BLI_addtail(&scene->render_layers, sl);

	/* unique name */
	BLI_strncpy_utf8(sl->name, name, sizeof(sl->name));
	BLI_uniquename(&scene->render_layers, sl, DATA_("SceneLayer"), '.', offsetof(SceneLayer, name), sizeof(sl->name));

	SceneCollection *sc = BKE_collection_master(scene);
	layer_collection_add(sl, &sl->layer_collections, sc);

	return sl;
}

bool BKE_scene_layer_remove(Main *bmain, Scene *scene, SceneLayer *sl)
{
	const int act = BLI_findindex(&scene->render_layers, sl);

	if (act == -1) {
		return false;
	}
	else if ( (scene->render_layers.first == scene->render_layers.last) &&
	          (scene->render_layers.first == sl))
	{
		/* ensure 1 layer is kept */
		return false;
	}

	BLI_remlink(&scene->render_layers, sl);

	BKE_scene_layer_free(sl);
	MEM_freeN(sl);

	scene->active_layer = 0;
	/* TODO WORKSPACE: set active_layer to 0 */

	for (Scene *sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			BKE_nodetree_remove_layer_n(sce->nodetree, scene, act);
		}
	}

	return true;
}

/**
 * Free (or release) any data used by this SceneLayer (does not free the SceneLayer itself).
 */
void BKE_scene_layer_free(SceneLayer *sl)
{
	sl->basact = NULL;
	BLI_freelistN(&sl->object_bases);

	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
		layer_collection_free(NULL, lc);
	}
	BLI_freelistN(&sl->layer_collections);
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

		BLI_remlink(&sl->object_bases, base);
		MEM_freeN(base);
	}
}

static void layer_collection_base_flag_recalculate(
        LayerCollection *lc, const bool tree_is_visible, const bool tree_is_selectable)
{
	bool is_visible = tree_is_visible && ((lc->flag & COLLECTION_VISIBLE) != 0);
	/* an object can only be selected if it's visible */
	bool is_selectable = tree_is_selectable && is_visible && ((lc->flag & COLLECTION_SELECTABLE) != 0);

	for (LinkData *link = lc->object_bases.first; link; link = link->next) {
		Base *base = link->data;

		if (is_visible) {
			base->flag |= BASE_VISIBLED;
		}

		if (is_selectable) {
			base->flag |= BASE_SELECTABLED;
		}
	}

	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		layer_collection_base_flag_recalculate(lcn, is_visible, is_selectable);
	}
}

/**
 * Re-evaluate the ObjectBase flags for SceneLayer
 */
void BKE_scene_layer_base_flag_recalculate(SceneLayer *sl)
{
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		base->flag &= ~(BASE_VISIBLED | BASE_SELECTABLED);
	}

	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
		layer_collection_base_flag_recalculate(lc, true, true);
	}

	/* if base is not selectabled, clear select */
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		if ((base->flag & BASE_SELECTABLED) == 0) {
			base->flag &= ~BASE_SELECTED;
		}
	}
}

/**
 * Tag Scene Layer to recalculation
 *
 * Temporary function, waiting for real depsgraph
 */
void BKE_scene_layer_engine_settings_recalculate(SceneLayer *sl)
{
	sl->flag |= SCENE_LAYER_ENGINE_DIRTY;
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		base->flag |= BASE_DIRTY_ENGINE_SETTINGS;
	}
}

/**
 * Tag Object in SceneLayer to recalculation
 *
 * Temporary function, waiting for real depsgraph
 */
void BKE_scene_layer_engine_settings_object_recalculate(SceneLayer *sl, Object *ob)
{
	Base *base = BLI_findptr(&sl->object_bases, ob, offsetof(Base, object));
	if (base) {
		sl->flag |= SCENE_LAYER_ENGINE_DIRTY;
		base->flag |= BASE_DIRTY_ENGINE_SETTINGS;
	}
}

/**
 * Tag all Objects in LayerCollection to recalculation
 *
 * Temporary function, waiting for real depsgraph
 */
void BKE_scene_layer_engine_settings_collection_recalculate(SceneLayer *sl, LayerCollection *lc)
{
	sl->flag |= SCENE_LAYER_ENGINE_DIRTY;

	for (LinkData *link = lc->object_bases.first; link; link = link->next) {
		Base *base = (Base *)link->data;
		base->flag |= BASE_DIRTY_ENGINE_SETTINGS;
	}

	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		BKE_scene_layer_engine_settings_collection_recalculate(sl, lcn);
	}
}

/**
 * Re-calculate the engine settings for all the objects in SceneLayer
 *
 * Temporary function, waiting for real depsgraph
 */
void BKE_scene_layer_engine_settings_update(struct SceneLayer *sl)
{
	if ((sl->flag & SCENE_LAYER_ENGINE_DIRTY) == 0) {
		return;
	}

	/* do the complete settings update */
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		if (((base->flag & BASE_DIRTY_ENGINE_SETTINGS) != 0) && \
		    (base->flag & BASE_VISIBLED) != 0)
		{
			scene_layer_engine_settings_update(sl, base->object);
			base->flag &= ~BASE_DIRTY_ENGINE_SETTINGS;
		}
	}

	sl->flag &= ~SCENE_LAYER_ENGINE_DIRTY;
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

		/* do not bump user count, leave it for SceneCollections */
		base->object = ob;
		BLI_addtail(&sl->object_bases, base);
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
	layer_collection_engine_settings_free(lc);

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
LayerCollection *BKE_layer_collection_active(SceneLayer *sl)
{
	int i = 0;
	return collection_from_index(&sl->layer_collections, sl->active_collection, &i);
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
			BLI_listbase_swaplinks(&sl->layer_collections, lc_src, lc_dst);
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
			BLI_listbase_swaplinks(&sl->layer_collections, lc_src, lc_dst);
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
				layer_collection_add(sl, &lc->layer_collections, sc_nested);
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
	LayerCollection *lc = layer_collection_add(sl, &sl->layer_collections, sc);
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
	BKE_scene_layer_base_flag_recalculate(sl);
	BKE_scene_layer_engine_settings_collection_recalculate(sl, lc);

	BLI_remlink(&sl->layer_collections, lc);
	MEM_freeN(lc);
	sl->active_collection = 0;
}

static void layer_collection_object_add(SceneLayer *sl, LayerCollection *lc, Object *ob)
{
	Base *base = object_base_add(sl, ob);

	/* only add an object once - prevent SceneCollection->objects and
	 * SceneCollection->filter_objects to add the same object */

	if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data))) {
		return;
	}

	BLI_addtail(&lc->object_bases, BLI_genericNodeN(base));

	BKE_scene_layer_base_flag_recalculate(sl);
	BKE_scene_layer_engine_settings_object_recalculate(sl, ob);
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
		layer_collection_add(sl, &lc->layer_collections, nsc);
	}
}

static LayerCollection *layer_collection_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc)
{
	LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");
	BLI_addtail(lb, lc);

	lc->scene_collection = sc;
	lc->flag = COLLECTION_VISIBLE + COLLECTION_SELECTABLE + COLLECTION_FOLDED;

	layer_collection_create_engine_settings(lc);
	layer_collection_create_mode_settings(lc);
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
				layer_collection_add(sl, &lc_parent->layer_collections, sc);
			}
		}
	}
}

/**
 * Add a corresponding ObjectBase to all the equivalent LayerCollection
 */
void BKE_layer_sync_object_link(Scene *scene, SceneCollection *sc, Object *ob)
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
void BKE_layer_sync_object_unlink(Scene *scene, SceneCollection *sc, Object *ob)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			LayerCollection *found = find_layer_collection_by_scene_collection(lc, sc);
			if (found) {
				layer_collection_object_remove(sl, found, ob);
			}
		}
		BKE_scene_layer_base_flag_recalculate(sl);
		BKE_scene_layer_engine_settings_object_recalculate(sl, ob);
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

ListBase R_engines_settings_callbacks = {NULL, NULL};

typedef struct CollectionEngineSettingsCB_Type {
	struct CollectionEngineSettingsCB_Type *next, *prev;

	char name[MAX_NAME]; /* engine name */

	CollectionEngineSettingsCB callback;

} CollectionEngineSettingsCB_Type;

static void create_engine_settings_layer_collection(LayerCollection *lc, CollectionEngineSettingsCB_Type *ces_type)
{
	if (BKE_layer_collection_engine_get(lc, COLLECTION_MODE_NONE, ces_type->name)) {
		return;
	}

	CollectionEngineSettings *ces = collection_engine_settings_create(ces_type);
	BLI_addtail(&lc->engine_settings, ces);

	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		create_engine_settings_layer_collection(lcn, ces_type);
	}
}

static void create_engines_settings_scene(Scene *scene, CollectionEngineSettingsCB_Type *ces_type)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
			create_engine_settings_layer_collection(lc, ces_type);
		}
	}
}

void BKE_layer_collection_engine_settings_callback_register(
        Main *bmain, const char *engine_name, CollectionEngineSettingsCB func)
{
	CollectionEngineSettingsCB_Type	*ces_type;

	/* cleanup in case it existed */
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name,
	                          offsetof(CollectionEngineSettingsCB_Type, name));

	if (ces_type) {
		BLI_remlink(&R_engines_settings_callbacks, ces_type);
		MEM_freeN(ces_type);
	}

	ces_type = MEM_callocN(sizeof(CollectionEngineSettingsCB_Type), "collection_engine_type");
	BLI_strncpy_utf8(ces_type->name, engine_name, sizeof(ces_type->name));
	ces_type->callback = func;
	BLI_addtail(&R_engines_settings_callbacks, ces_type);

	if (bmain) {
		/* populate all of the collections of the scene with those settings */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			create_engines_settings_scene(scene, ces_type);
		}
	}
}

void BKE_layer_collection_engine_settings_callback_free(void)
{
	BLI_freelistN(&R_engines_settings_callbacks);
}

static CollectionEngineSettings *collection_engine_settings_create(CollectionEngineSettingsCB_Type *ces_type)
{
	/* create callback data */
	CollectionEngineSettings *ces = MEM_callocN(sizeof(CollectionEngineSettings), "Collection Engine Settings");
	BLI_strncpy_utf8(ces->name, ces_type->name, sizeof(ces->name));

	/* call callback */
	ces_type->callback(NULL, ces);

	return ces;
}

/**
 * Initialize a CollectionEngineSettings
 *
 * Usually we would pass LayerCollection->engine_settings
 * But depsgraph uses this for Object->collection_settings
 */
CollectionEngineSettings *BKE_layer_collection_engine_settings_create(const char *engine_name)
{
	CollectionEngineSettingsCB_Type *ces_type;
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name,
	                          offsetof(CollectionEngineSettingsCB_Type, name));
	BLI_assert(ces_type);

	CollectionEngineSettings *ces = collection_engine_settings_create(ces_type);
	return ces;
}

/**
 * Free the CollectionEngineSettings
 */
void BKE_layer_collection_engine_settings_free(CollectionEngineSettings *ces)
{
	BLI_freelistN(&ces->properties);
}

static void layer_collection_engine_settings_free(LayerCollection *lc)
{
	for (CollectionEngineSettings *ces = lc->engine_settings.first; ces; ces = ces->next) {
		BKE_layer_collection_engine_settings_free(ces);
	}
	BLI_freelistN(&lc->engine_settings);

	for (CollectionEngineSettings *ces = lc->mode_settings.first; ces; ces = ces->next) {
		BKE_layer_collection_engine_settings_free(ces);
	}
	BLI_freelistN(&lc->mode_settings);
}

/**
 * Initialize the render settings for a single LayerCollection
 */
static void layer_collection_create_engine_settings(LayerCollection *lc)
{
	CollectionEngineSettingsCB_Type *ces_type;
	for (ces_type = R_engines_settings_callbacks.first; ces_type; ces_type = ces_type->next) {
		create_engine_settings_layer_collection(lc, ces_type);
	}
}

static void layer_collection_create_mode_settings_object(ListBase *lb)
{
	CollectionEngineSettings *ces;

	ces = MEM_callocN(sizeof(CollectionEngineSettings), "Object Mode Settings");
	BLI_strncpy_utf8(ces->name, "Object Mode", sizeof(ces->name));
	ces->type = COLLECTION_MODE_OBJECT;

	/* properties */
	OBJECT_collection_settings_create(ces);

	BLI_addtail(lb, ces);
}

static void layer_collection_create_mode_settings_edit(ListBase *lb)
{
	CollectionEngineSettings *ces;

	ces = MEM_callocN(sizeof(CollectionEngineSettings), "Edit Mode Settings");
	BLI_strncpy_utf8(ces->name, "Edit Mode", sizeof(ces->name));
	ces->type = COLLECTION_MODE_EDIT;

	/* properties */
	EDIT_MESH_collection_settings_create(ces);

	BLI_addtail(lb, ces);
}

static void collection_create_mode_settings(ListBase *lb)
{
	layer_collection_create_mode_settings_object(lb);
	layer_collection_create_mode_settings_edit(lb);
}

static void layer_collection_create_mode_settings(LayerCollection *lc)
{
	collection_create_mode_settings(&lc->mode_settings);
}

/**
 * Return collection enginne settings for either Object s of LayerCollection s
 */
static CollectionEngineSettings *collection_engine_get(
        ListBase *lb_render, ListBase *lb_mode, const int type, const char *engine_name)
{
	if (type == COLLECTION_MODE_NONE) {
		return BLI_findstring(lb_render, engine_name, offsetof(CollectionEngineSettings, name));
	}
	else {
		CollectionEngineSettings *ces;
		for (ces = lb_mode->first; ces; ces = ces->next) {
			if (ces->type == type) {
				return ces;
			}
		}
	}
	BLI_assert(false);
	return NULL;
}

/**
 * Return collection engine settings from Object for specified engine of mode
 */
CollectionEngineSettings *BKE_object_collection_engine_get(Object *ob, const int type, const char *engine_name)
{
	return collection_engine_get(&ob->collection_settings, &ob->collection_settings, type, engine_name);
}
/**
 * Return layer collection engine settings for specified engine
 */
CollectionEngineSettings *BKE_layer_collection_engine_get(LayerCollection *lc, const int type, const char *engine_name)
{
	return collection_engine_get(&lc->engine_settings, &lc->mode_settings, type, engine_name);
}

/* ---------------------------------------------------------------------- */
/* Engine Settings Properties */

void BKE_collection_engine_property_add_float(CollectionEngineSettings *ces, const char *name, float value)
{
	CollectionEnginePropertyFloat *prop;
	prop = MEM_callocN(sizeof(CollectionEnginePropertyFloat), "collection engine settings float");
	prop->data.type = COLLECTION_PROP_TYPE_FLOAT;
	BLI_strncpy_utf8(prop->data.name, name, sizeof(prop->data.name));
	prop->value = value;
	BLI_addtail(&ces->properties, prop);
}

void BKE_collection_engine_property_add_int(CollectionEngineSettings *ces, const char *name, int value)
{
	CollectionEnginePropertyInt *prop;
	prop = MEM_callocN(sizeof(CollectionEnginePropertyInt), "collection engine settings int");
	prop->data.type = COLLECTION_PROP_TYPE_INT;
	BLI_strncpy_utf8(prop->data.name, name, sizeof(prop->data.name));
	prop->value = value;
	BLI_addtail(&ces->properties, prop);
}

void BKE_collection_engine_property_add_bool(CollectionEngineSettings *ces, const char *name, bool value)
{
	CollectionEnginePropertyBool *prop;
	prop = MEM_callocN(sizeof(CollectionEnginePropertyBool), "collection engine settings bool");
	prop->data.type = COLLECTION_PROP_TYPE_BOOL;
	BLI_strncpy_utf8(prop->data.name, name, sizeof(prop->data.name));
	prop->value = value;
	BLI_addtail(&ces->properties, prop);
}

CollectionEngineProperty *BKE_collection_engine_property_get(CollectionEngineSettings *ces, const char *name)
{
	return BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
}

int BKE_collection_engine_property_value_get_int(CollectionEngineSettings *ces, const char *name)
{
	CollectionEnginePropertyInt *prop;
	prop = (CollectionEnginePropertyInt *)BLI_findstring(&ces->properties, name,
	                                                     offsetof(CollectionEngineProperty, name));
	return prop->value;
}

float BKE_collection_engine_property_value_get_float(CollectionEngineSettings *ces, const char *name)
{
	CollectionEnginePropertyFloat *prop;
	prop = (CollectionEnginePropertyFloat *)BLI_findstring(&ces->properties, name,
	                                                       offsetof(CollectionEngineProperty, name));
	return prop->value;
}

bool BKE_collection_engine_property_value_get_bool(CollectionEngineSettings *ces, const char *name)
{
	CollectionEnginePropertyBool *prop;
	prop = (CollectionEnginePropertyBool *)BLI_findstring(&ces->properties, name,
	                                                      offsetof(CollectionEngineProperty, name));
	return prop->value;
}

void BKE_collection_engine_property_value_set_int(CollectionEngineSettings *ces, const char *name, int value)
{
	CollectionEnginePropertyInt *prop;
	prop = (CollectionEnginePropertyInt *)BLI_findstring(&ces->properties, name,
	                                                     offsetof(CollectionEngineProperty, name));
	prop->value = value;
	prop->data.flag |= COLLECTION_PROP_USE;
}

void BKE_collection_engine_property_value_set_float(CollectionEngineSettings *ces, const char *name, float value)
{
	CollectionEnginePropertyFloat *prop;
	prop = (CollectionEnginePropertyFloat *)BLI_findstring(&ces->properties, name,
	                                                       offsetof(CollectionEngineProperty, name));
	prop->value = value;
	prop->data.flag |= COLLECTION_PROP_USE;
}

void BKE_collection_engine_property_value_set_bool(CollectionEngineSettings *ces, const char *name, bool value)
{
	CollectionEnginePropertyBool *prop;
	prop = (CollectionEnginePropertyBool *)BLI_findstring(&ces->properties, name,
	                                                      offsetof(CollectionEngineProperty, name));
	prop->value = value;
	prop->data.flag |= COLLECTION_PROP_USE;
}

bool BKE_collection_engine_property_use_get(CollectionEngineSettings *ces, const char *name)
{
	CollectionEngineProperty *prop;
	prop = (CollectionEngineProperty *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
	return ((prop->flag & COLLECTION_PROP_USE) != 0);
}

void BKE_collection_engine_property_use_set(CollectionEngineSettings *ces, const char *name, bool value)
{
	CollectionEngineProperty *prop;
	prop = (CollectionEngineProperty *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));

	if (value) {
		prop->flag |= COLLECTION_PROP_USE;
	}
	else {
		prop->flag &= ~COLLECTION_PROP_USE;
	}
}

/* Engine Settings recalculate  */

static void collection_engine_settings_init(ListBase *lb)
{
	CollectionEngineSettingsCB_Type *ces_type;
	for (ces_type = R_engines_settings_callbacks.first; ces_type; ces_type = ces_type->next) {
		CollectionEngineSettings *ces = collection_engine_settings_create(ces_type);
		BLI_strncpy_utf8(ces->name, ces_type->name, sizeof(ces->name));
		BLI_addtail(lb, ces);

		/* call callback */
		ces_type->callback(NULL, ces);
	}

	/* edit modes */
	collection_create_mode_settings(lb);
}

static void collection_engine_settings_copy(ListBase *lb_dst, ListBase *lb_src)
{
	for (CollectionEngineSettings *ces_src = lb_src->first; ces_src; ces_src = ces_src->next) {
		CollectionEngineSettings *ces_dst = MEM_callocN(sizeof(*ces_dst), "CollectionEngineSettings copy");

		BLI_strncpy_utf8(ces_dst->name, ces_src->name, sizeof(ces_dst->name));
		ces_dst->type = ces_src->type;
		BLI_addtail(lb_dst, ces_dst);

		for (CollectionEngineProperty *prop = ces_src->properties.first; prop; prop = prop->next) {
			CollectionEngineProperty *prop_new = MEM_dupallocN(prop);
			BLI_addtail(&ces_dst->properties, prop_new);
		}
	}
}

/**
 * Set a value from a CollectionProperty to another
 */
static void collection_engine_property_set (CollectionEngineProperty *prop_dst, CollectionEngineProperty *prop_src)
{
	if ((prop_src->flag & COLLECTION_PROP_USE) != 0) {
		/* mark the property as used, so the engine knows if the value was ever set*/
		prop_dst->flag |= COLLECTION_PROP_USE;
		switch (prop_src->type) {
			case COLLECTION_PROP_TYPE_FLOAT:
				((CollectionEnginePropertyFloat *)prop_dst)->value = ((CollectionEnginePropertyFloat *)prop_src)->value;
				break;
			case COLLECTION_PROP_TYPE_INT:
				((CollectionEnginePropertyInt *)prop_dst)->value = ((CollectionEnginePropertyInt *)prop_src)->value;
				break;
			case COLLECTION_PROP_TYPE_BOOL:
				((CollectionEnginePropertyBool *)prop_dst)->value = ((CollectionEnginePropertyBool *)prop_src)->value;
				break;
			default:
				BLI_assert(false);
				break;
		}
	}
}

static void collection_engine_settings_merge(ListBase *lb_dst, ListBase *lb_src)
{
	for (CollectionEngineSettings *ces_src = lb_src->first; ces_src; ces_src = ces_src->next) {
		CollectionEngineSettings *ces_dst = collection_engine_get(lb_dst, lb_dst, ces_src->type, ces_src->name);
		BLI_assert(ces_dst);

		CollectionEngineProperty *prop_dst, *prop_src;
		for (prop_dst = ces_dst->properties.first; prop_dst; prop_dst = prop_dst->next) {
			prop_src = BLI_findstring(&ces_src->properties, prop_dst->name, offsetof(CollectionEngineProperty, name));
			BLI_assert(prop_src);
			collection_engine_property_set(prop_dst, prop_src);
		}
	}
}

static void layer_collection_engine_settings_update(
        LayerCollection *lc, ListBase *lb_parent,
        Base *base, ListBase *lb_object)
{
	if ((lc->flag & COLLECTION_VISIBLE) == 0) {
		return;
	}

	ListBase lb_collection = {NULL};
	collection_engine_settings_copy(&lb_collection, lb_parent);

	collection_engine_settings_merge(&lb_collection, &lc->engine_settings);
	collection_engine_settings_merge(&lb_collection, &lc->mode_settings);

	if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data)) != NULL) {
		collection_engine_settings_merge(lb_object, &lb_collection);
	}

	/* do it recursively */
	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		layer_collection_engine_settings_update(lcn, &lb_collection, base, lb_object);
	}

	BKE_layer_collection_engine_settings_list_free(&lb_collection);
}

/**
 * Empty all the CollectionEngineSettings in the list
 */
void BKE_layer_collection_engine_settings_list_free(struct ListBase *lb)
{
	for (CollectionEngineSettings *ces = lb->first; ces; ces = ces->next) {
		BKE_layer_collection_engine_settings_free(ces);
	}
	BLI_freelistN(lb);
}

/**
 * Update the collection settings pointer allocated in the object
 * This is to be flushed from the Depsgraph
 */
static void scene_layer_engine_settings_update(SceneLayer *sl, Object *ob)
{
	Base *base = BKE_scene_layer_base_find(sl, ob);
	ListBase ces_layer = {NULL};

	collection_engine_settings_init(&ces_layer);

	/* start fresh */
	BKE_layer_collection_engine_settings_list_free(&ob->collection_settings);
	collection_engine_settings_init(&ob->collection_settings);

	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
		layer_collection_engine_settings_update(lc, &ces_layer, base, &ob->collection_settings);
	}

	BKE_layer_collection_engine_settings_list_free(&ces_layer);
}

/* ---------------------------------------------------------------------- */
/* Iterators */

static void object_bases_Iterator_begin(Iterator *iter, void *data_in, const int flag)
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
		object_bases_Iterator_next(iter, flag);
	}
	else {
		iter->current = base;
	}
}

static void object_bases_Iterator_next(Iterator *iter, const int flag)
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

static void objects_Iterator_begin(Iterator *iter, void *data_in, const int flag)
{
	object_bases_Iterator_begin(iter, data_in, flag);

	if (iter->valid) {
		iter->current = ((Base *)iter->current)->object;
	}
}

static void objects_Iterator_next(Iterator *iter, const int flag)
{
	object_bases_Iterator_next(iter, flag);

	if (iter->valid) {
		iter->current = ((Base *)iter->current)->object;
	}
}

void BKE_selected_objects_Iterator_begin(Iterator *iter, void *data_in)
{
	objects_Iterator_begin(iter, data_in, BASE_SELECTED);
}

void BKE_selected_objects_Iterator_next(Iterator *iter)
{
	objects_Iterator_next(iter, BASE_SELECTED);
}

void BKE_selected_objects_Iterator_end(Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_visible_objects_Iterator_begin(Iterator *iter, void *data_in)
{
	objects_Iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_visible_objects_Iterator_next(Iterator *iter)
{
	objects_Iterator_next(iter, BASE_VISIBLED);
}

void BKE_visible_objects_Iterator_end(Iterator *UNUSED(iter))
{
	/* do nothing */
}

void BKE_visible_bases_Iterator_begin(Iterator *iter, void *data_in)
{
	object_bases_Iterator_begin(iter, data_in, BASE_VISIBLED);
}

void BKE_visible_bases_Iterator_next(Iterator *iter)
{
	object_bases_Iterator_next(iter, BASE_VISIBLED);
}

void BKE_visible_bases_Iterator_end(Iterator *UNUSED(iter))
{
	/* do nothing */
}


/* ---------------------------------------------------------------------- */
/* Doversion routine */

/**
 * Merge CollectionEngineSettings
 *
 * \param ces_ref CollectionEngineSettings to use as reference
 * \param ces CollectionEngineSettings to merge into
 */
static void scene_layer_doversion_merge_setings(const CollectionEngineSettings *ces_ref, CollectionEngineSettings *ces)
{
	CollectionEngineProperty *cep = ces->properties.first, *cep_ref;

	for (cep_ref = ces_ref->properties.first; cep_ref; cep_ref = cep_ref->next) {
		cep = BLI_findstring(&ces->properties, cep_ref->name, offsetof(CollectionEngineProperty, name));

		if (cep == NULL) {
			cep = MEM_dupallocN(cep_ref);
			BLI_addtail(&ces->properties, cep);
		}
		else if (cep->type != cep_ref->type) {
			CollectionEngineProperty *prev = cep->prev, *next = cep->next;
			MEM_freeN(cep);
			cep = MEM_dupallocN(cep_ref);

			cep->prev = prev;
			cep->next = next;
		}
		else {
			/* keep the property as it is */
		}
	}
}

/**
 * Merge ListBases of LayerCollections
 *
 * \param lb_ref ListBase of CollectionEngineSettings to use as reference
 * \param lb ListBase of CollectionEngineSettings
 */
static void scene_layer_doversion_merge_layer_collection(const ListBase *lb_ref, ListBase *lb)
{
	CollectionEngineSettings *ces = lb->first, *ces_ref;

	for (ces_ref = lb_ref->first; ces_ref; ces_ref = ces_ref->next) {
		ces = BLI_findstring(lb, ces_ref->name, offsetof(CollectionEngineSettings, name));

		if (ces == NULL) {
			ces = MEM_dupallocN(ces_ref);
			BLI_duplicatelist(&ces->properties, &ces_ref->properties);
			BLI_addtail(lb, ces);
		}
		else {
			scene_layer_doversion_merge_setings(ces_ref, ces);
		}
	}
}

/**
 * Create or remove CollectionEngineSettings and CollectionEngineProperty
 * based on reference LayerCollection
 *
 * \param lc_ref reference LayerCollection to merge missing settings from
 * \param lb ListBase of LayerCollection
 */
static void scene_layer_doversion_update_collections(const LayerCollection *lc_ref, ListBase *lb)
{
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {

		scene_layer_doversion_merge_layer_collection(&lc_ref->engine_settings, &lc->engine_settings);
		scene_layer_doversion_merge_layer_collection(&lc_ref->mode_settings, &lc->mode_settings);

		/* continue recursively */
		scene_layer_doversion_update_collections(lc_ref, &lc->layer_collections);
	}
}

/**
 * Updates all the CollectionEngineSettings of all
 * LayerCollection elements in Scene
 *
 * \param lc_ref reference LayerCollection to merge missing settings from
 */
static void scene_layer_doversion_update(const LayerCollection *lc_ref, Scene *scene)
{
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		scene_layer_doversion_update_collections(lc_ref, &sl->layer_collections);
	}
}

/**
 * Return true at the first indicative that the listbases don't match
 *
 * It's fine if the individual properties values are different, as long
 * as we have the same properties across them
 *
 * \param lb_ces ListBase of CollectionEngineSettings
 * \param lb_ces_ref ListBase of CollectionEngineSettings
 */
static bool scene_layer_doversion_is_outdated_engines(ListBase *lb_ces, ListBase *lb_ces_ref)
{
	if (BLI_listbase_count(lb_ces) != BLI_listbase_count(lb_ces_ref)) {
		return true;
	}

	CollectionEngineSettings *ces, *ces_ref;
	for (ces = lb_ces->first, ces_ref = lb_ces_ref->first; ces; ces = ces->next, ces_ref = ces_ref->next) {
		if (BLI_listbase_count(&ces->properties) != BLI_listbase_count(&ces_ref->properties)) {
			return true;
		}

		CollectionEngineProperty *cep, *cep_ref;
		for (cep = ces->properties.first, cep_ref = ces_ref->properties.first;
		     cep != NULL;
		     cep = cep->next, cep_ref = cep_ref->next)
		{
			if (cep->type != cep_ref->type) {
				return true;
			}

			if (STREQ(cep->name, cep_ref->name) == false) {
				return true;
			}
		}
	}

	return false;
}

/**
 * Get the first available LayerCollection
 */
static LayerCollection *scene_layer_doversion_collection_get(Main *bmain)
{
	for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
		for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
			for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
				return lc;
			}
		}
	}
	return NULL;
}

/**
 * See if a new LayerCollection have the same CollectionEngineSettings
 * and properties of the saved LayerCollection
 */
static bool scene_layer_doversion_is_outdated(Main *bmain)
{
	LayerCollection *lc, lc_ref = {NULL};
	bool is_outdated = false;

	lc = scene_layer_doversion_collection_get(bmain);

	if (lc == NULL) {
		return false;
	}

	layer_collection_create_engine_settings(&lc_ref);
	layer_collection_create_mode_settings(&lc_ref);

	if (scene_layer_doversion_is_outdated_engines(&lc->engine_settings, &lc_ref.engine_settings)) {
		is_outdated = true;
	}

	if (scene_layer_doversion_is_outdated_engines(&lc->mode_settings, &lc_ref.mode_settings)) {
		is_outdated = true;
	}

	layer_collection_engine_settings_free(&lc_ref);
	return is_outdated;
}

/**
 * Handle doversion of files during the viewport development
 *
 * This is intended to prevent subversion bumping every time a new property
 * is added to an engine, but it may be relevant in the future as a generic doversion
 */
void BKE_scene_layer_doversion_update(Main *bmain)
{
	/* if file not outdated, don't bother with the slow merging */
	if (scene_layer_doversion_is_outdated(bmain) == false) {
		return;
	}

	/* create a reference LayerCollection to merge missing settings from */
	LayerCollection lc_ref = {NULL};
	layer_collection_create_engine_settings(&lc_ref);
	layer_collection_create_mode_settings(&lc_ref);

	/* bring all the missing properties for the LayerCollections */
	for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
		scene_layer_doversion_update(&lc_ref, scene);
	}

	layer_collection_engine_settings_free(&lc_ref);
}
