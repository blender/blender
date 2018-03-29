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

/** \file blender/blenkernel/intern/collection.c
 *  \ingroup bke
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLT_translation.h"
#include "BLI_string_utils.h"

#include "BKE_collection.h"
#include "BKE_group.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_group_types.h"
#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/* Prototypes. */
static SceneCollection *find_collection_parent(const struct SceneCollection *sc_child, struct SceneCollection *sc_parent);
static bool is_collection_in_tree(const struct SceneCollection *sc_reference, struct SceneCollection *sc_parent);

static SceneCollection *collection_master_from_id(const ID *owner_id)
{
	switch (GS(owner_id->name)) {
		case ID_SCE:
			return ((Scene *)owner_id)->collection;
		case ID_GR:
			return ((Group *)owner_id)->collection;
		default:
			BLI_assert(!"ID doesn't support collections");
			return NULL;
	}
}

/**
 * Add a new collection, but don't handle syncing with layer collections
 */
static SceneCollection *collection_add(ID *owner_id, SceneCollection *sc_parent, const int type, const char *name_custom)
{
	SceneCollection *sc_master = collection_master_from_id(owner_id);
	SceneCollection *sc = MEM_callocN(sizeof(SceneCollection), "New Collection");
	sc->type = type;
	const char *name = name_custom;

	if (!sc_parent) {
		sc_parent = sc_master;
	}

	if (!name) {
		if (sc_parent == sc_master) {
			name = BLI_sprintfN("Collection %d", BLI_listbase_count(&sc_master->scene_collections) + 1);
		}
		else {
			const int number = BLI_listbase_count(&sc_parent->scene_collections) + 1;
			const int digits = integer_digits_i(number);
			const int max_len = sizeof(sc_parent->name) - 1 /* NULL terminator */ - (1 + digits) /* " %d" */;
			name = BLI_sprintfN("%.*s %d", max_len, sc_parent->name, number);
		}
	}

	BLI_addtail(&sc_parent->scene_collections, sc);
	BKE_collection_rename(owner_id, sc, name);

	if (name != name_custom) {
		MEM_freeN((char *)name);
	}

	return sc;
}

/**
 * Add a collection to a collection ListBase and syncronize all render layers
 * The ListBase is NULL when the collection is to be added to the master collection
 */
SceneCollection *BKE_collection_add(ID *owner_id, SceneCollection *sc_parent, const int type, const char *name_custom)
{
	if (sc_parent == NULL) {
		sc_parent = BKE_collection_master(owner_id);
	}

	SceneCollection *scene_collection = collection_add(owner_id, sc_parent, type, name_custom);
	BKE_layer_sync_new_scene_collection(owner_id, sc_parent, scene_collection);
	return scene_collection;
}

/**
 * Free the collection items recursively
 */
static void collection_free(SceneCollection *sc, const bool do_id_user)
{
	if (do_id_user) {
		for (LinkData *link = sc->objects.first; link; link = link->next) {
			id_us_min(link->data);
		}
	}

	BLI_freelistN(&sc->objects);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		collection_free(nsc, do_id_user);
	}
	BLI_freelistN(&sc->scene_collections);
}

/**
 * Unlink the collection recursively
 * \return true if unlinked.
 */
static bool collection_remlink(SceneCollection *sc_parent, SceneCollection *sc_gone)
{
	for (SceneCollection *sc = sc_parent->scene_collections.first; sc; sc = sc->next) {
		if (sc == sc_gone) {
			BLI_remlink(&sc_parent->scene_collections, sc_gone);
			return true;
		}

		if (collection_remlink(sc, sc_gone)) {
			return true;
		}
	}
	return false;
}

/**
 * Recursively remove any instance of this SceneCollection
 */
static void layer_collection_remove(ViewLayer *view_layer, ListBase *lb, const SceneCollection *sc)
{
	LayerCollection *lc = lb->first;
	while (lc) {
		if (lc->scene_collection == sc) {
			BKE_layer_collection_free(view_layer, lc);
			BLI_remlink(lb, lc);

			LayerCollection *lc_next = lc->next;
			MEM_freeN(lc);
			lc = lc_next;

			/* only the "top-level" layer collections may have the
			 * same SceneCollection in a sibling tree.
			 */
			if (lb != &view_layer->layer_collections) {
				return;
			}
		}

		else {
			layer_collection_remove(view_layer, &lc->layer_collections, sc);
			lc = lc->next;
		}
	}
}

/**
 * Remove a collection from the scene, and syncronize all render layers
 *
 * If an object is in any other collection, link the object to the master collection.
 */
bool BKE_collection_remove(ID *owner_id, SceneCollection *sc)
{
	SceneCollection *sc_master = collection_master_from_id(owner_id);

	/* The master collection cannot be removed. */
	if (sc == sc_master) {
		return false;
	}

	/* We need to do bottom up removal, otherwise we get a crash when we remove a collection that
	 * has one of its nested collections linked to a view layer. */
	SceneCollection *scene_collection_nested = sc->scene_collections.first;
	while (scene_collection_nested != NULL) {
		SceneCollection *scene_collection_next = scene_collection_nested->next;
		BKE_collection_remove(owner_id, scene_collection_nested);
		scene_collection_nested = scene_collection_next;
	}

	/* Unlink from the respective collection tree. */
	if (!collection_remlink(sc_master, sc)) {
		BLI_assert(false);
	}

	/* If an object is no longer in any collection, we add it to the master collection. */
	ListBase collection_objects;
	BLI_duplicatelist(&collection_objects, &sc->objects);

	FOREACH_SCENE_COLLECTION_BEGIN(owner_id, scene_collection_iter)
	{
		if (scene_collection_iter == sc) {
			continue;
		}

		LinkData *link_next, *link = collection_objects.first;
		while (link) {
			link_next = link->next;

			if (BLI_findptr(&scene_collection_iter->objects, link->data, offsetof(LinkData, data))) {
				BLI_remlink(&collection_objects, link);
				MEM_freeN(link);
			}

			link = link_next;
		}
	}
	FOREACH_SCENE_COLLECTION_END;

	for (LinkData *link = collection_objects.first; link; link = link->next) {
		BKE_collection_object_add(owner_id, sc_master, link->data);
	}

	BLI_freelistN(&collection_objects);

	/* Clear the collection items. */
	collection_free(sc, true);

	/* check all layers that use this collection and clear them */
	for (ViewLayer *view_layer = BKE_view_layer_first_from_id(owner_id); view_layer; view_layer = view_layer->next) {
		layer_collection_remove(view_layer, &view_layer->layer_collections, sc);
		view_layer->active_collection = 0;
	}

	MEM_freeN(sc);
	return true;
}

/**
 * Copy SceneCollection tree but keep pointing to the same objects
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_collection_copy_data(SceneCollection *sc_dst, SceneCollection *sc_src, const int flag)
{
	BLI_duplicatelist(&sc_dst->objects, &sc_src->objects);
	if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
		for (LinkData *link = sc_dst->objects.first; link; link = link->next) {
			id_us_plus(link->data);
		}
	}

	BLI_duplicatelist(&sc_dst->scene_collections, &sc_src->scene_collections);
	for (SceneCollection *nsc_src = sc_src->scene_collections.first, *nsc_dst = sc_dst->scene_collections.first;
	     nsc_src;
	     nsc_src = nsc_src->next, nsc_dst = nsc_dst->next)
	{
		BKE_collection_copy_data(nsc_dst, nsc_src, flag);
	}
}

/**
 * Makes a shallow copy of a SceneCollection
 *
 * Add a new collection in the same level as the old one, copy any nested collections
 * but link the objects to the new collection (as oppose to copy them).
 */
SceneCollection *BKE_collection_duplicate(ID *owner_id, SceneCollection *scene_collection)
{
	SceneCollection *scene_collection_master = BKE_collection_master(owner_id);
	SceneCollection *scene_collection_parent = find_collection_parent(scene_collection, scene_collection_master);

	/* It's not allowed to copy the master collection. */
	if (scene_collection_master == scene_collection) {
		return NULL;
	}

	SceneCollection *scene_collection_new = collection_add(
	                                            owner_id,
	                                            scene_collection_parent,
	                                            scene_collection->type,
	                                            scene_collection->name);

	if (scene_collection_new != scene_collection->next) {
		BLI_remlink(&scene_collection_parent->scene_collections, scene_collection_new);
		BLI_insertlinkafter(&scene_collection_parent->scene_collections, scene_collection, scene_collection_new);
	}

	BKE_collection_copy_data(scene_collection_new, scene_collection, 0);
	BKE_layer_sync_new_scene_collection(owner_id, scene_collection_parent, scene_collection_new);

	/* Make sure every linked instance of the new collection has the same values (flags, overrides, ...) as the
	 * corresponding original collection. */
	BKE_layer_collection_sync_flags(owner_id, scene_collection_new, scene_collection);

	return scene_collection_new;
}

static SceneCollection *master_collection_from_id(const ID *owner_id)
{
	switch (GS(owner_id->name)) {
		case ID_SCE:
			return ((const Scene *)owner_id)->collection;
		case ID_GR:
			return ((const Group *)owner_id)->collection;
		default:
			BLI_assert(!"ID doesn't support scene collection");
			return NULL;
	}
}

/**
 * Returns the master collection of the scene or group
 */
SceneCollection *BKE_collection_master(const ID *owner_id)
{
	return master_collection_from_id(owner_id);
}

static void collection_rename(const ID *owner_id, SceneCollection *sc, const char *name)
{
	SceneCollection *sc_parent = find_collection_parent(sc, collection_master_from_id(owner_id));
	BLI_strncpy(sc->name, name, sizeof(sc->name));
	BLI_uniquename(&sc_parent->scene_collections, sc, DATA_("Collection"), '.', offsetof(SceneCollection, name), sizeof(sc->name));
}

void BKE_collection_rename(const ID *owner_id, SceneCollection *sc, const char *name)
{
	collection_rename(owner_id, sc, name);
}

/**
 * Make sure the collection name is still unique within its siblings.
 */
static void collection_name_check(const ID *owner_id, SceneCollection *sc)
{
	/* It's a bit of a hack, we simply try to make sure the collection name is valid. */
	collection_rename(owner_id, sc, sc->name);
}

/**
 * Free (or release) any data used by the master collection (does not free the master collection itself).
 * Used only to clear the entire scene or group data since it's not doing re-syncing of the LayerCollection tree
 */
void BKE_collection_master_free(ID *owner_id, const bool do_id_user)
{
	collection_free(BKE_collection_master(owner_id), do_id_user);
}

static void collection_object_add(const ID *owner_id, SceneCollection *sc, Object *ob)
{
	BLI_addtail(&sc->objects, BLI_genericNodeN(ob));

	if (GS(owner_id->name) == ID_SCE) {
		id_us_plus((ID *)ob);
	}
	else {
		BLI_assert(GS(owner_id->name) == ID_GR);
		if ((ob->flag & OB_FROMGROUP) == 0) {
			ob->flag |= OB_FROMGROUP;
		}
	}

	BKE_layer_sync_object_link(owner_id, sc, ob);
}

/**
 * Add object to collection
 */
bool BKE_collection_object_add(const ID *owner_id, SceneCollection *sc, Object *ob)
{
	if (BKE_collection_object_exists(sc, ob)) {
		/* don't add the same object twice */
		return false;
	}

	collection_object_add(owner_id, sc, ob);
	return true;
}

/**
 * Add object to all collections that reference objects is in
 * (used to copy objects)
 */
void BKE_collection_object_add_from(Scene *scene, Object *ob_src, Object *ob_dst)
{
	FOREACH_SCENE_COLLECTION_BEGIN(scene, sc)
	{
		if (BLI_findptr(&sc->objects, ob_src, offsetof(LinkData, data))) {
			collection_object_add(&scene->id, sc, ob_dst);
		}
	}
	FOREACH_SCENE_COLLECTION_END;

	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		Base *base_src = BKE_view_layer_base_find(view_layer, ob_src);
		if (base_src != NULL) {
			if (base_src->collection_properties == NULL) {
				continue;
			}
			Base *base_dst = BKE_view_layer_base_find(view_layer, ob_dst);
			IDP_MergeGroup(base_dst->collection_properties, base_src->collection_properties, true);
		}
	}
}

/**
 * Remove object from collection.
 * \param bmain: Can be NULL if free_us is false.
 */
bool BKE_collection_object_remove(Main *bmain, ID *owner_id, SceneCollection *sc, Object *ob, const bool free_us)
{
	LinkData *link = BLI_findptr(&sc->objects, ob, offsetof(LinkData, data));

	if (link == NULL) {
		return false;
	}

	BLI_remlink(&sc->objects, link);
	MEM_freeN(link);

	BKE_layer_sync_object_unlink(owner_id, sc, ob);

	if (GS(owner_id->name) == ID_SCE) {
		if (free_us) {
			BKE_libblock_free_us(bmain, ob);
		}
		else {
			id_us_min(&ob->id);
		}
	}
	else {
		BLI_assert(GS(owner_id->name) == ID_GR);
	}

	return true;
}

/**
 * Remove object from all collections of scene
 * \param scene_collection_skip: Don't remove base from this collection.
 */
static bool collections_object_remove_ex(Main *bmain, ID *owner_id, Object *ob, const bool free_us,
                                         SceneCollection *scene_collection_skip)
{
	bool removed = false;
	if (GS(owner_id->name) == ID_SCE) {
		BKE_scene_remove_rigidbody_object((Scene *)owner_id, ob);
	}
	else {
		BLI_assert(GS(owner_id->name) == ID_GR);
	}

	FOREACH_SCENE_COLLECTION_BEGIN(owner_id, sc)
	{
		if (sc != scene_collection_skip) {
			removed |= BKE_collection_object_remove(bmain, owner_id, sc, ob, free_us);
		}
	}
	FOREACH_SCENE_COLLECTION_END;
	return removed;
}

/**
 * Remove object from all collections of scene
 */
bool BKE_collections_object_remove(Main *bmain, ID *owner_id, Object *ob, const bool free_us)
{
	return collections_object_remove_ex(bmain, owner_id, ob, free_us, NULL);
}

/**
 * Move object from a collection into another
 *
 * If source collection is NULL move it from all the existing collections.
 */
void BKE_collection_object_move(ID *owner_id, SceneCollection *sc_dst, SceneCollection *sc_src, Object *ob)
{
	/* In both cases we first add the object, then remove it from the other collections.
	 * Otherwise we lose the original base and whether it was active and selected. */
	if (sc_src != NULL) {
		if (BKE_collection_object_add(owner_id, sc_dst, ob)) {
			BKE_collection_object_remove(NULL, owner_id, sc_src, ob, false);
		}
	}
	else {
		/* Adding will fail if object is already in collection.
		 * However we still need to remove it from the other collections. */
		BKE_collection_object_add(owner_id, sc_dst, ob);
		collections_object_remove_ex(NULL, owner_id, ob, false, sc_dst);
	}
}

/**
 * Whether the object is directly inside the collection.
 */
bool BKE_collection_object_exists(struct SceneCollection *scene_collection, struct Object *ob)
{
	if (BLI_findptr(&scene_collection->objects, ob, offsetof(LinkData, data))) {
		return true;
	}
	return false;
}

static SceneCollection *scene_collection_from_index_recursive(SceneCollection *scene_collection, const int index, int *index_current)
{
	if (index == (*index_current)) {
		return scene_collection;
	}

	(*index_current)++;

	for (SceneCollection *scene_collection_iter = scene_collection->scene_collections.first;
	     scene_collection_iter != NULL;
	     scene_collection_iter = scene_collection_iter->next)
	{
		SceneCollection *nested = scene_collection_from_index_recursive(scene_collection_iter, index, index_current);
		if (nested != NULL) {
			return nested;
		}
	}
	return NULL;
}

/**
 * Return Scene Collection for a given index.
 *
 * The index is calculated from top to bottom counting the children before the siblings.
 */
SceneCollection *BKE_collection_from_index(Scene *scene, const int index)
{
	int index_current = 0;
	SceneCollection *master_collection = BKE_collection_master(&scene->id);
	return scene_collection_from_index_recursive(master_collection, index, &index_current);
}

static void layer_collection_sync(LayerCollection *lc_dst, LayerCollection *lc_src)
{
	lc_dst->flag = lc_src->flag;

	/* Pending: sync overrides. */
	IDP_MergeGroup(lc_dst->properties, lc_src->properties, true);

	/* Continue recursively. */
	LayerCollection *lc_dst_nested, *lc_src_nested;
	lc_src_nested = lc_src->layer_collections.first;
	for (lc_dst_nested = lc_dst->layer_collections.first;
	     lc_dst_nested && lc_src_nested;
	     lc_dst_nested = lc_dst_nested->next, lc_src_nested = lc_src_nested->next)
	{
		layer_collection_sync(lc_dst_nested, lc_src_nested);
	}
}

/**
 * Select all the objects in this SceneCollection (and its nested collections) for this ViewLayer.
 * Return true if any object was selected.
 */
bool BKE_collection_objects_select(ViewLayer *view_layer, SceneCollection *scene_collection)
{
	LayerCollection *layer_collection = BKE_layer_collection_first_from_scene_collection(view_layer, scene_collection);
	if (layer_collection != NULL) {
		BKE_layer_collection_objects_select(layer_collection);
		return true;
	}
	else {
		/* Slower approach, we need to iterate over all the objects and for each one we see if there is a base. */
		bool changed = false;
		for (LinkData *link = scene_collection->objects.first; link; link = link->next) {
			Base *base = BKE_view_layer_base_find(view_layer, link->data);
			if (base != NULL) {
				if (((base->flag & BASE_SELECTED) == 0) && ((base->flag & BASE_SELECTABLED) != 0)) {
					base->flag |= BASE_SELECTED;
					changed = true;
				}
			}
		}
		return changed;
	}
}

/**
 * Leave only the master collection in, remove everything else.
 * @param group
 */
static void collection_group_cleanup(Group *group)
{
	/* Unlink all the LayerCollections. */
	while (group->view_layer->layer_collections.last != NULL) {
		BKE_collection_unlink(group->view_layer, group->view_layer->layer_collections.last);
	}

	/* Remove all the SceneCollections but the master. */
	collection_free(group->collection, false);
}

/**
 * Create a group from a collection
 *
 * Any ViewLayer that may have this the related SceneCollection linked is converted
 * to a Group Collection.
 */
Group *BKE_collection_group_create(Main *bmain, Scene *scene, LayerCollection *lc_src)
{
	SceneCollection *sc_dst, *sc_src = lc_src->scene_collection;
	LayerCollection *lc_dst;

	/* The master collection can't be converted. */
	if (sc_src == BKE_collection_master(&scene->id)) {
		return NULL;
	}

	/* If a sub-collection of sc_dst is directly linked into a ViewLayer we can't convert. */
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (LayerCollection *lc_child = view_layer->layer_collections.first; lc_child; lc_child = lc_child->next) {
			if (is_collection_in_tree(lc_child->scene_collection, sc_src)) {
				return NULL;
			}
		}
	}

	/* Create new group with the same data as the original collection. */
	Group *group = BKE_group_add(bmain, sc_src->name);
	collection_group_cleanup(group);

	sc_dst = BKE_collection_add(&group->id, NULL, COLLECTION_TYPE_GROUP_INTERNAL, sc_src->name);
	BKE_collection_copy_data(sc_dst, sc_src, 0);
	FOREACH_SCENE_COLLECTION_BEGIN(&group->id, sc_group)
	{
		sc_group->type = COLLECTION_TYPE_GROUP_INTERNAL;
	}
	FOREACH_SCENE_COLLECTION_END;

	lc_dst = BKE_collection_link(group->view_layer, sc_dst);
	layer_collection_sync(lc_dst, lc_src);

	return group;
}

/* ---------------------------------------------------------------------- */
/* Outliner drag and drop */

/**
 * Find and return the SceneCollection that has \a sc_child as one of its directly
 * nested SceneCollection.
 *
 * \param sc_parent Initial SceneCollection to look into recursively, usually the master collection
 */
static SceneCollection *find_collection_parent(const SceneCollection *sc_child, SceneCollection *sc_parent)
{
	for (SceneCollection *sc_nested = sc_parent->scene_collections.first; sc_nested; sc_nested = sc_nested->next) {
		if (sc_nested == sc_child) {
			return sc_parent;
		}

		SceneCollection *found = find_collection_parent(sc_child, sc_nested);
		if (found) {
			return found;
		}
	}

	return NULL;
}

/**
 * Check if \a sc_reference is nested to \a sc_parent SceneCollection
 */
static bool is_collection_in_tree(const SceneCollection *sc_reference, SceneCollection *sc_parent)
{
	return find_collection_parent(sc_reference, sc_parent) != NULL;
}

bool BKE_collection_move_above(const ID *owner_id, SceneCollection *sc_dst, SceneCollection *sc_src)
{
	/* Find the SceneCollection the sc_src belongs to */
	SceneCollection *sc_master = master_collection_from_id(owner_id);

	/* Master Layer can't be moved around*/
	if (ELEM(sc_master, sc_src, sc_dst)) {
		return false;
	}

	/* collection is already where we wanted it to be */
	if (sc_dst->prev == sc_src) {
		return false;
	}

	/* We can't move a collection fs the destiny collection
	 * is nested to the source collection */
	if (is_collection_in_tree(sc_dst, sc_src)) {
		return false;
	}

	SceneCollection *sc_src_parent = find_collection_parent(sc_src, sc_master);
	SceneCollection *sc_dst_parent = find_collection_parent(sc_dst, sc_master);
	BLI_assert(sc_src_parent);
	BLI_assert(sc_dst_parent);

	/* Remove sc_src from its parent */
	BLI_remlink(&sc_src_parent->scene_collections, sc_src);

	/* Re-insert it where it belongs */
	BLI_insertlinkbefore(&sc_dst_parent->scene_collections, sc_dst, sc_src);

	/* Update the tree */
	BKE_layer_collection_resync(owner_id, sc_src_parent);
	BKE_layer_collection_resync(owner_id, sc_dst_parent);

	/* Keep names unique. */
	collection_name_check(owner_id, sc_src);

	return true;
}

bool BKE_collection_move_below(const ID *owner_id, SceneCollection *sc_dst, SceneCollection *sc_src)
{
	/* Find the SceneCollection the sc_src belongs to */
	SceneCollection *sc_master = master_collection_from_id(owner_id);

	/* Master Layer can't be moved around*/
	if (ELEM(sc_master, sc_src, sc_dst)) {
		return false;
	}

	/* Collection is already where we wanted it to be */
	if (sc_dst->next == sc_src) {
		return false;
	}

	/* We can't move a collection if the destiny collection
	 * is nested to the source collection */
	if (is_collection_in_tree(sc_dst, sc_src)) {
		return false;
	}

	SceneCollection *sc_src_parent = find_collection_parent(sc_src, sc_master);
	SceneCollection *sc_dst_parent = find_collection_parent(sc_dst, sc_master);
	BLI_assert(sc_src_parent);
	BLI_assert(sc_dst_parent);

	/* Remove sc_src from its parent */
	BLI_remlink(&sc_src_parent->scene_collections, sc_src);

	/* Re-insert it where it belongs */
	BLI_insertlinkafter(&sc_dst_parent->scene_collections, sc_dst, sc_src);

	/* Update the tree */
	BKE_layer_collection_resync(owner_id, sc_src_parent);
	BKE_layer_collection_resync(owner_id, sc_dst_parent);

	/* Keep names unique. */
	collection_name_check(owner_id, sc_src);

	return true;
}

bool BKE_collection_move_into(const ID *owner_id, SceneCollection *sc_dst, SceneCollection *sc_src)
{
	/* Find the SceneCollection the sc_src belongs to */
	SceneCollection *sc_master = master_collection_from_id(owner_id);
	if (sc_src == sc_master) {
		return false;
	}

	/* We can't move a collection if the destiny collection
	 * is nested to the source collection */
	if (is_collection_in_tree(sc_dst, sc_src)) {
		return false;
	}

	SceneCollection *sc_src_parent = find_collection_parent(sc_src, sc_master);
	BLI_assert(sc_src_parent);

	/* collection is already where we wanted it to be */
	if (sc_dst->scene_collections.last == sc_src) {
		return false;
	}

	/* Remove sc_src from it */
	BLI_remlink(&sc_src_parent->scene_collections, sc_src);

	/* Insert sc_src into sc_dst */
	BLI_addtail(&sc_dst->scene_collections, sc_src);

	/* Update the tree */
	BKE_layer_collection_resync(owner_id, sc_src_parent);
	BKE_layer_collection_resync(owner_id, sc_dst);

	/* Keep names unique. */
	collection_name_check(owner_id, sc_src);

	return true;
}

/* ---------------------------------------------------------------------- */
/* Iteractors */
/* scene collection iteractor */

typedef struct SceneCollectionsIteratorData {
	ID *owner_id;
	void **array;
	int tot, cur;
} SceneCollectionsIteratorData;

static void scene_collection_callback(SceneCollection *sc, BKE_scene_collections_Cb callback, void *data)
{
	callback(sc, data);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		scene_collection_callback(nsc, callback, data);
	}
}

static void scene_collections_count(SceneCollection *UNUSED(sc), void *data)
{
	int *tot = data;
	(*tot)++;
}

static void scene_collections_build_array(SceneCollection *sc, void *data)
{
	SceneCollection ***array = data;
	**array = sc;
	(*array)++;
}

static void scene_collections_array(ID *owner_id, SceneCollection ***collections_array, int *tot)
{
	SceneCollection *sc;
	SceneCollection **array;

	*collections_array = NULL;
	*tot = 0;

	if (owner_id == NULL) {
		return;
	}

	sc = master_collection_from_id(owner_id);
	BLI_assert(sc != NULL);
	scene_collection_callback(sc, scene_collections_count, tot);

	if (*tot == 0)
		return;

	*collections_array = array = MEM_mallocN(sizeof(SceneCollection *) * (*tot), "SceneCollectionArray");
	scene_collection_callback(sc, scene_collections_build_array, &array);
}

/**
 * Only use this in non-performance critical situations
 * (it iterates over all scene collections twice)
 */
void BKE_scene_collections_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	ID *owner_id = data_in;
	SceneCollectionsIteratorData *data = MEM_callocN(sizeof(SceneCollectionsIteratorData), __func__);

	data->owner_id = owner_id;
	iter->data = data;
	iter->valid = true;

	scene_collections_array(owner_id, (SceneCollection ***)&data->array, &data->tot);
	BLI_assert(data->tot != 0);

	data->cur = 0;
	iter->current = data->array[data->cur];
}

void BKE_scene_collections_iterator_next(struct BLI_Iterator *iter)
{
	SceneCollectionsIteratorData *data = iter->data;

	if (++data->cur < data->tot) {
		iter->current = data->array[data->cur];
	}
	else {
		iter->valid = false;
	}
}

void BKE_scene_collections_iterator_end(struct BLI_Iterator *iter)
{
	SceneCollectionsIteratorData *data = iter->data;

	if (data) {
		if (data->array) {
			MEM_freeN(data->array);
		}
		MEM_freeN(data);
	}
	iter->valid = false;
}


/* scene objects iteractor */

typedef struct SceneObjectsIteratorData {
	GSet *visited;
	LinkData *link_next;
	BLI_Iterator scene_collection_iter;
} SceneObjectsIteratorData;

void BKE_scene_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
	Scene *scene = data_in;
	SceneObjectsIteratorData *data = MEM_callocN(sizeof(SceneObjectsIteratorData), __func__);
	iter->data = data;

	/* lookup list ot make sure each object is object called once */
	data->visited = BLI_gset_ptr_new(__func__);

	/* we wrap the scenecollection iterator here to go over the scene collections */
	BKE_scene_collections_iterator_begin(&data->scene_collection_iter, scene);

	SceneCollection *sc = data->scene_collection_iter.current;
	if (sc->objects.first != NULL) {
		iter->current = ((LinkData *)sc->objects.first)->data;
	}
	else {
		BKE_scene_objects_iterator_next(iter);
	}
}

/**
 * Gets the first unique object in the sequence
 */
static LinkData *object_base_unique(GSet *gs, LinkData *link)
{
	for (; link != NULL; link = link->next) {
		Object *ob = link->data;
		void **ob_key_p;
		if (!BLI_gset_ensure_p_ex(gs, ob, &ob_key_p)) {
			*ob_key_p = ob;
			return link;
		}
	}
	return NULL;
}

void BKE_scene_objects_iterator_next(BLI_Iterator *iter)
{
	SceneObjectsIteratorData *data = iter->data;
	LinkData *link = data->link_next ? object_base_unique(data->visited, data->link_next) : NULL;

	if (link) {
		data->link_next = link->next;
		iter->current = link->data;
	}
	else {
		/* if this is the last object of this ListBase look at the next SceneCollection */
		SceneCollection *sc;
		BKE_scene_collections_iterator_next(&data->scene_collection_iter);
		do {
			sc = data->scene_collection_iter.current;
			/* get the first unique object of this collection */
			LinkData *new_link = object_base_unique(data->visited, sc->objects.first);
			if (new_link) {
				data->link_next = new_link->next;
				iter->current = new_link->data;
				return;
			}
			BKE_scene_collections_iterator_next(&data->scene_collection_iter);
		} while (data->scene_collection_iter.valid);

		if (!data->scene_collection_iter.valid) {
			iter->valid = false;
		}
	}
}

void BKE_scene_objects_iterator_end(BLI_Iterator *iter)
{
	SceneObjectsIteratorData *data = iter->data;
	if (data) {
		BKE_scene_collections_iterator_end(&data->scene_collection_iter);
		BLI_gset_free(data->visited, NULL);
		MEM_freeN(data);
	}
}
