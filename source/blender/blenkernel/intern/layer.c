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

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLT_translation.h"

#include "BKE_layer.h"
#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

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
static void scene_layer_engine_settings_update(SceneLayer *sl, Object *ob, const char *engine_name);
static void object_bases_Iterator_next(Iterator *iter, const int flag);

/* RenderLayer */

/**
 * Returns the SceneLayer to be used for rendering
 */
SceneLayer *BKE_scene_layer_active(struct Scene *scene)
{
	SceneLayer *sl = BLI_findlink(&scene->render_layers, scene->active_layer);
	BLI_assert(sl);
	return sl;
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
SceneLayer *BKE_scene_layer_find_from_collection(Scene *scene, LayerCollection *lc)
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

static void scene_layer_object_base_unref(SceneLayer* sl, Base *base)
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

static void layer_collection_base_flag_recalculate(LayerCollection *lc, const bool tree_is_visible, const bool tree_is_selectable)
{
	bool is_visible = tree_is_visible && ((lc->flag & COLLECTION_VISIBLE) != 0);
	/* an object can only be selected if it's visible */
	bool is_selectable = tree_is_selectable && is_visible && ((lc->flag & COLLECTION_SELECTABLE) != 0);

	for (LinkData *link = lc->object_bases.first; link; link = link->next) {
		Base *base = link->data;

		if (is_visible) {
			base->flag |= BASE_VISIBLED;
		}
		else {
			base->flag &= ~BASE_VISIBLED;
		}

		if (is_selectable) {
			base->flag |= BASE_SELECTABLED;
		}
		else {
			base->flag &= ~BASE_SELECTABLED;
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
void BKE_scene_layer_engine_settings_update(struct SceneLayer *sl, const char *engine_name)
{
	if ((sl->flag & SCENE_LAYER_ENGINE_DIRTY) == 0) {
		return;
	}

	/* do the complete settings update */
	for (Base *base = sl->object_bases.first; base; base = base->next) {
		if (((base->flag & BASE_DIRTY_ENGINE_SETTINGS) != 0) && \
		    (base->flag & BASE_VISIBLED) != 0)
		{
			scene_layer_engine_settings_update(sl, base->object, engine_name);
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
static int index_from_collection(ListBase *lb, LayerCollection *lc, int *i)
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
int BKE_layer_collection_findindex(SceneLayer *sl, LayerCollection *lc)
{
	int i = 0;
	return index_from_collection(&sl->layer_collections, lc, &i);
}

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
bool BKE_scene_layer_has_collection(struct SceneLayer *sl, struct SceneCollection *sc)
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
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name, offsetof(CollectionEngineSettingsCB_Type, name));

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
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name, offsetof(CollectionEngineSettingsCB_Type, name));
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

	for (CollectionEngineSettings *ces = lc->mode_settings.first; ces; ces = ces->next) {
		BKE_layer_collection_engine_settings_free(ces);
	}

	BLI_freelistN(&lc->engine_settings);
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

static void layer_collection_create_mode_settings_object(LayerCollection *lc)
{
	CollectionEngineSettings *ces;

	ces = MEM_callocN(sizeof(CollectionEngineSettings), "Object Mode Settings");
	BLI_addtail(&lc->mode_settings, ces);
	ces->type = COLLECTION_MODE_OBJECT;

	/* properties */
	BKE_collection_engine_property_add_int(ces, "foo", 2);
}

static void layer_collection_create_mode_settings_edit(LayerCollection *lc)
{
	CollectionEngineSettings *ces;

	ces = MEM_callocN(sizeof(CollectionEngineSettings), "Edit Mode Settings");
	BLI_addtail(&lc->mode_settings, ces);
	ces->type = COLLECTION_MODE_EDIT;

	/* properties */
	BKE_collection_engine_property_add_float(ces, "bar", 0.5);
}

static void layer_collection_create_mode_settings(LayerCollection *lc)
{
	layer_collection_create_mode_settings_object(lc);
	layer_collection_create_mode_settings_edit(lc);
}

/**
 * Return layer collection engine settings for specified engine
 */
CollectionEngineSettings *BKE_layer_collection_engine_get(LayerCollection *lc, const int type, const char *engine_name)
{
	if (type == COLLECTION_MODE_NONE) {
		return BLI_findstring(&lc->engine_settings, engine_name, offsetof(CollectionEngineSettings, name));
	}
	else {
		CollectionEngineSettings *ces;
		for (ces = lc->mode_settings.first; ces; ces = ces->next) {
			if (ces->type == type) {
				return ces;
			}
		}
	}
	BLI_assert(false);
	return NULL;
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

CollectionEngineProperty *BKE_collection_engine_property_get(CollectionEngineSettings *ces, const char *name)
{
	return BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
}

int BKE_collection_engine_property_value_get_int(CollectionEngineSettings *ces, const char *name)
{
	CollectionEnginePropertyInt *prop;
	prop = (CollectionEnginePropertyInt *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
	return prop->value;
}

float BKE_collection_engine_property_value_get_float(CollectionEngineSettings *ces, const char *name)
{
	CollectionEnginePropertyFloat *prop;
	prop = (CollectionEnginePropertyFloat *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
	return prop->value;
}

void BKE_collection_engine_property_value_set_int(CollectionEngineSettings *ces, const char *name, int value)
{
	CollectionEnginePropertyInt *prop;
	prop = (CollectionEnginePropertyInt *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
	prop->value = value;
	prop->data.flag |= COLLECTION_PROP_USE;
}

void BKE_collection_engine_property_value_set_float(CollectionEngineSettings *ces, const char *name, float value)
{
	CollectionEnginePropertyFloat *prop;
	prop = (CollectionEnginePropertyFloat *)BLI_findstring(&ces->properties, name, offsetof(CollectionEngineProperty, name));
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

static void collection_engine_settings_init(CollectionEngineSettings *ces, const char *engine_name)
{
	CollectionEngineSettingsCB_Type *ces_type;
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name, offsetof(CollectionEngineSettingsCB_Type, name));

	BLI_listbase_clear(&ces->properties);
	BLI_strncpy_utf8(ces->name, ces_type->name, sizeof(ces->name));

	/* call callback */
	ces_type->callback(NULL, ces);
}

static void collection_engine_settings_copy(CollectionEngineSettings *ces_dst, CollectionEngineSettings *ces_src)
{
	BLI_strncpy_utf8(ces_dst->name, ces_src->name, sizeof(ces_dst->name));
	BLI_freelistN(&ces_dst->properties);

	for (CollectionEngineProperty *prop = ces_src->properties.first; prop; prop = prop->next) {
		CollectionEngineProperty *prop_new = MEM_dupallocN(prop);
		BLI_addtail(&ces_dst->properties, prop_new);
	}
}

/**
 * Set a value from a CollectionProperty to another
 */
static void collection_engine_property_set (CollectionEngineProperty *prop_dst, CollectionEngineProperty *prop_src){
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
		    default:
			    BLI_assert(false);
			    break;
		}
	}
}

static void collection_engine_settings_merge(CollectionEngineSettings *ces_dst, CollectionEngineSettings *ces_src)
{
	CollectionEngineProperty *prop_src, *prop_dst;

	prop_dst = ces_dst->properties.first;
	for (prop_src = ces_src->properties.first; prop_src; prop_src = prop_src->next, prop_dst = prop_dst->next) {
		collection_engine_property_set(prop_dst, prop_src);
	}
}

static void layer_collection_engine_settings_update(
        LayerCollection *lc, CollectionEngineSettings *ces_parent,
        Base *base, CollectionEngineSettings *ces_ob)
{
	if ((lc->flag & COLLECTION_VISIBLE) == 0) {
		return;
	}

	CollectionEngineSettings ces = {NULL};
	collection_engine_settings_copy(&ces, ces_parent);

	CollectionEngineSettings *ces_lc = BKE_layer_collection_engine_get(lc, ces.type, ces.name);
	collection_engine_settings_merge(&ces, ces_lc);

	if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data)) != NULL) {
		collection_engine_settings_merge(ces_ob, &ces);
	}

	/* do it recursively */
	for (LayerCollection *lcn = lc->layer_collections.first; lcn; lcn = lcn->next) {
		layer_collection_engine_settings_update(lcn, &ces, base, ces_ob);
	}

	BKE_layer_collection_engine_settings_free(&ces);
}

/**
 * Update the collection settings pointer allocated in the object
 * This is to be flushed from the Depsgraph
 */
static void scene_layer_engine_settings_update(SceneLayer *sl, Object *ob, const char *engine_name)
{
	Base *base = BKE_scene_layer_base_find(sl, ob);
	CollectionEngineSettings ces_layer = {NULL}, *ces_ob;

	collection_engine_settings_init(&ces_layer, engine_name);

	if (ob->collection_settings) {
		BKE_layer_collection_engine_settings_free(ob->collection_settings);
		MEM_freeN(ob->collection_settings);
	}

	CollectionEngineSettingsCB_Type *ces_type;
	ces_type = BLI_findstring(&R_engines_settings_callbacks, engine_name, offsetof(CollectionEngineSettingsCB_Type, name));
	ces_ob = collection_engine_settings_create(ces_type);

	for (LayerCollection *lc = sl->layer_collections.first; lc; lc = lc->next) {
		layer_collection_engine_settings_update(lc, &ces_layer, base, ces_ob);
	}

	BKE_layer_collection_engine_settings_free(&ces_layer);
	ob->collection_settings = ces_ob;
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
