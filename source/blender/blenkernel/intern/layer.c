/*
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
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_freestyle.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"

#include "DNA_ID.h"
#include "DNA_space_types.h"
#include "DNA_collection_types.h"
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

/* Set of flags which are dependent on a collection settings. */
static const short g_base_collection_flags = (BASE_VISIBLE | BASE_SELECTABLE |
                                              BASE_ENABLED_VIEWPORT | BASE_ENABLED_RENDER |
                                              BASE_HOLDOUT | BASE_INDIRECT_ONLY);

/* prototype */
static void object_bases_iterator_next(BLI_Iterator *iter, const int flag);

/*********************** Layer Collections and bases *************************/

static LayerCollection *layer_collection_add(ListBase *lb_parent, Collection *collection)
{
  LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");
  lc->collection = collection;
  BLI_addtail(lb_parent, lc);

  return lc;
}

static void layer_collection_free(ViewLayer *view_layer, LayerCollection *lc)
{
  if (lc == view_layer->active_collection) {
    view_layer->active_collection = NULL;
  }

  for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
    layer_collection_free(view_layer, nlc);
  }

  BLI_freelistN(&lc->layer_collections);
}

static Base *object_base_new(Object *ob)
{
  Base *base = MEM_callocN(sizeof(Base), "Object Base");
  base->object = ob;
  if (ob->base_flag & BASE_SELECTED) {
    base->flag |= BASE_SELECTED;
  }
  return base;
}

/********************************* View Layer ********************************/

/* RenderLayer */

/* Returns the default view layer to view in workspaces if there is
 * none linked to the workspace yet. */
ViewLayer *BKE_view_layer_default_view(const Scene *scene)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    if (!(view_layer->flag & VIEW_LAYER_RENDER)) {
      return view_layer;
    }
  }

  BLI_assert(scene->view_layers.first);
  return scene->view_layers.first;
}

/* Returns the default view layer to render if we need to render just one. */
ViewLayer *BKE_view_layer_default_render(const Scene *scene)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    if (view_layer->flag & VIEW_LAYER_RENDER) {
      return view_layer;
    }
  }

  BLI_assert(scene->view_layers.first);
  return scene->view_layers.first;
}

/* Returns view layer with matching name, or NULL if not found. */
ViewLayer *BKE_view_layer_find(const Scene *scene, const char *layer_name)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    if (STREQ(view_layer->name, layer_name)) {
      return view_layer;
    }
  }

  return NULL;
}

/**
 * This is a placeholder to know which areas of the code need to be addressed
 * for the Workspace changes. Never use this, you should typically get the
 * active layer from the context or window.
 */
ViewLayer *BKE_view_layer_context_active_PLACEHOLDER(const Scene *scene)
{
  BLI_assert(scene->view_layers.first);
  return scene->view_layers.first;
}

static ViewLayer *view_layer_add(const char *name)
{
  if (!name) {
    name = DATA_("View Layer");
  }

  ViewLayer *view_layer = MEM_callocN(sizeof(ViewLayer), "View Layer");
  view_layer->flag = VIEW_LAYER_RENDER | VIEW_LAYER_FREESTYLE;

  BLI_strncpy_utf8(view_layer->name, name, sizeof(view_layer->name));

  /* Pure rendering pipeline settings. */
  view_layer->layflag = 0x7FFF; /* solid ztra halo edge strand */
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
  ViewLayer *view_layer = view_layer_add(name);

  BLI_addtail(&scene->view_layers, view_layer);

  /* unique name */
  BLI_uniquename(&scene->view_layers,
                 view_layer,
                 DATA_("ViewLayer"),
                 '.',
                 offsetof(ViewLayer, name),
                 sizeof(view_layer->name));

  BKE_layer_collection_sync(scene, view_layer);

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

  if (view_layer->object_bases_hash) {
    BLI_ghash_free(view_layer->object_bases_hash, NULL, NULL);
  }

  for (LayerCollection *lc = view_layer->layer_collections.first; lc; lc = lc->next) {
    layer_collection_free(view_layer, lc);
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
ViewLayer *BKE_view_layer_find_from_collection(const Scene *scene, LayerCollection *lc)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    if (find_scene_collection_in_scene_collections(&view_layer->layer_collections, lc)) {
      return view_layer;
    }
  }

  return NULL;
}

/* Base */

static void view_layer_bases_hash_create(ViewLayer *view_layer)
{
  static ThreadMutex hash_lock = BLI_MUTEX_INITIALIZER;

  if (view_layer->object_bases_hash == NULL) {
    BLI_mutex_lock(&hash_lock);

    if (view_layer->object_bases_hash == NULL) {
      view_layer->object_bases_hash = BLI_ghash_new(
          BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

      for (Base *base = view_layer->object_bases.first; base; base = base->next) {
        if (base->object) {
          BLI_ghash_insert(view_layer->object_bases_hash, base->object, base);
        }
      }
    }

    BLI_mutex_unlock(&hash_lock);
  }
}

Base *BKE_view_layer_base_find(ViewLayer *view_layer, Object *ob)
{
  if (!view_layer->object_bases_hash) {
    view_layer_bases_hash_create(view_layer);
  }

  return BLI_ghash_lookup(view_layer->object_bases_hash, ob);
}

void BKE_view_layer_base_deselect_all(ViewLayer *view_layer)
{
  Base *base;

  for (base = view_layer->object_bases.first; base; base = base->next) {
    base->flag &= ~BASE_SELECTED;
  }
}

void BKE_view_layer_base_select_and_set_active(struct ViewLayer *view_layer, Base *selbase)
{
  view_layer->basact = selbase;
  if ((selbase->flag & BASE_SELECTABLE) != 0) {
    selbase->flag |= BASE_SELECTED;
  }
}

/**************************** Copy View Layer and Layer Collections ***********************/

static void layer_collections_copy_data(ViewLayer *view_layer_dst,
                                        const ViewLayer *view_layer_src,
                                        ListBase *layer_collections_dst,
                                        const ListBase *layer_collections_src)
{
  BLI_duplicatelist(layer_collections_dst, layer_collections_src);

  LayerCollection *layer_collection_dst = layer_collections_dst->first;
  const LayerCollection *layer_collection_src = layer_collections_src->first;

  while (layer_collection_dst != NULL) {
    layer_collections_copy_data(view_layer_dst,
                                view_layer_src,
                                &layer_collection_dst->layer_collections,
                                &layer_collection_src->layer_collections);

    if (layer_collection_src == view_layer_src->active_collection) {
      view_layer_dst->active_collection = layer_collection_dst;
    }

    layer_collection_dst = layer_collection_dst->next;
    layer_collection_src = layer_collection_src->next;
  }
}

/**
 * Only copy internal data of ViewLayer from source to already allocated/initialized destination.
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_view_layer_copy_data(Scene *scene_dst,
                              const Scene *UNUSED(scene_src),
                              ViewLayer *view_layer_dst,
                              const ViewLayer *view_layer_src,
                              const int flag)
{
  if (view_layer_dst->id_properties != NULL) {
    view_layer_dst->id_properties = IDP_CopyProperty_ex(view_layer_dst->id_properties, flag);
  }
  BKE_freestyle_config_copy(
      &view_layer_dst->freestyle_config, &view_layer_src->freestyle_config, flag);

  view_layer_dst->stats = NULL;

  /* Clear temporary data. */
  BLI_listbase_clear(&view_layer_dst->drawdata);
  view_layer_dst->object_bases_array = NULL;
  view_layer_dst->object_bases_hash = NULL;

  /* Copy layer collections and object bases. */
  /* Inline 'BLI_duplicatelist' and update the active base. */
  BLI_listbase_clear(&view_layer_dst->object_bases);
  for (Base *base_src = view_layer_src->object_bases.first; base_src; base_src = base_src->next) {
    Base *base_dst = MEM_dupallocN(base_src);
    BLI_addtail(&view_layer_dst->object_bases, base_dst);
    if (view_layer_src->basact == base_src) {
      view_layer_dst->basact = base_dst;
    }
  }

  view_layer_dst->active_collection = NULL;
  layer_collections_copy_data(view_layer_dst,
                              view_layer_src,
                              &view_layer_dst->layer_collections,
                              &view_layer_src->layer_collections);

  LayerCollection *lc_scene_dst = view_layer_dst->layer_collections.first;
  lc_scene_dst->collection = scene_dst->master_collection;
}

void BKE_view_layer_rename(Main *bmain, Scene *scene, ViewLayer *view_layer, const char *newname)
{
  char oldname[sizeof(view_layer->name)];

  BLI_strncpy(oldname, view_layer->name, sizeof(view_layer->name));

  BLI_strncpy_utf8(view_layer->name, newname, sizeof(view_layer->name));
  BLI_uniquename(&scene->view_layers,
                 view_layer,
                 DATA_("ViewLayer"),
                 '.',
                 offsetof(ViewLayer, name),
                 sizeof(view_layer->name));

  if (scene->nodetree) {
    bNode *node;
    int index = BLI_findindex(&scene->view_layers, view_layer);

    for (node = scene->nodetree->nodes.first; node; node = node->next) {
      if (node->type == CMP_NODE_R_LAYERS && node->id == NULL) {
        if (node->custom1 == index) {
          BLI_strncpy(node->name, view_layer->name, NODE_MAXSTR);
        }
      }
    }
  }

  /* Fix all the animation data and windows which may link to this. */
  BKE_animdata_fix_paths_rename_all(NULL, "view_layers", oldname, view_layer->name);

  /* WM can be missing on startup. */
  wmWindowManager *wm = bmain->wm.first;
  if (wm) {
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
      if (win->scene == scene && STREQ(win->view_layer_name, oldname)) {
        STRNCPY(win->view_layer_name, view_layer->name);
      }
    }
  }

  /* Dependency graph uses view layer name based lookups. */
  DEG_id_tag_update(&scene->id, 0);
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
  }

  for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
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
  return view_layer->active_collection;
}

/*
 * Activate collection
 */
bool BKE_layer_collection_activate(ViewLayer *view_layer, LayerCollection *lc)
{
  if (lc->flag & LAYER_COLLECTION_EXCLUDE) {
    return false;
  }

  view_layer->active_collection = lc;
  return true;
}

/**
 * Activate first parent collection
 */
LayerCollection *BKE_layer_collection_activate_parent(ViewLayer *view_layer, LayerCollection *lc)
{
  CollectionParent *parent = lc->collection->parents.first;

  if (parent) {
    lc = BKE_layer_collection_first_from_scene_collection(view_layer, parent->collection);
  }
  else {
    lc = NULL;
  }

  if (lc && (lc->flag & LAYER_COLLECTION_EXCLUDE)) {
    /* Don't activate excluded collections. */
    return BKE_layer_collection_activate_parent(view_layer, lc);
  }

  if (!lc) {
    lc = view_layer->layer_collections.first;
  }

  view_layer->active_collection = lc;
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
  }

  for (LayerCollection *lcol = lb->first; lcol; lcol = lcol->next) {
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

/*********************************** Syncing *********************************
 *
 * The layer collection tree mirrors the scene collection tree. Whenever that
 * changes we need to synchronize them so that there is a corresponding layer
 * collection for each collection. Note that the scene collection tree can
 * contain link or override collections, and so this is also called on .blend
 * file load to ensure any new or removed collections are synced.
 *
 * The view layer also contains a list of bases for each object that exists
 * in at least one layer collection. That list is also synchronized here, and
 * stores state like selection. */

static short layer_collection_sync(ViewLayer *view_layer,
                                   const ListBase *lb_scene,
                                   ListBase *lb_layer,
                                   ListBase *new_object_bases,
                                   short parent_exclude,
                                   short parent_restrict,
                                   short parent_layer_restrict)
{
  /* TODO: support recovery after removal of intermediate collections, reordering, ..
   * For local edits we can make editing operating do the appropriate thing, but for
   * linking we can only sync after the fact. */

  /* Remove layer collections that no longer have a corresponding scene collection. */
  for (LayerCollection *lc = lb_layer->first; lc;) {
    /* Note ID remap can set lc->collection to NULL when deleting collections. */
    LayerCollection *lc_next = lc->next;
    Collection *collection = (lc->collection) ?
                                 BLI_findptr(lb_scene,
                                             lc->collection,
                                             offsetof(CollectionChild, collection)) :
                                 NULL;

    if (!collection) {
      if (lc == view_layer->active_collection) {
        view_layer->active_collection = NULL;
      }

      /* Free recursively. */
      layer_collection_free(view_layer, lc);
      BLI_freelinkN(lb_layer, lc);
    }

    lc = lc_next;
  }

  /* Add layer collections for any new scene collections, and ensure order is the same. */
  ListBase new_lb_layer = {NULL, NULL};
  short runtime_flag = 0;

  for (const CollectionChild *child = lb_scene->first; child; child = child->next) {
    Collection *collection = child->collection;
    LayerCollection *lc = BLI_findptr(lb_layer, collection, offsetof(LayerCollection, collection));

    if (lc) {
      BLI_remlink(lb_layer, lc);
      BLI_addtail(&new_lb_layer, lc);
    }
    else {
      lc = layer_collection_add(&new_lb_layer, collection);
      lc->flag = parent_exclude;
    }

    /* Collection restrict is inherited. */
    short child_restrict = parent_restrict;
    short child_layer_restrict = parent_layer_restrict;
    if (!(collection->flag & COLLECTION_IS_MASTER)) {
      child_restrict |= collection->flag;
      child_layer_restrict |= lc->flag;
    }

    /* Sync child collections. */
    short child_runtime_flag = layer_collection_sync(view_layer,
                                                     &collection->children,
                                                     &lc->layer_collections,
                                                     new_object_bases,
                                                     lc->flag,
                                                     child_restrict,
                                                     child_layer_restrict);

    /* Layer collection exclude is not inherited. */
    if (lc->flag & LAYER_COLLECTION_EXCLUDE) {
      lc->runtime_flag = 0;
      continue;
    }
    else {
      lc->runtime_flag = child_runtime_flag;
    }

    if (((child_restrict & COLLECTION_RESTRICT_VIEWPORT) == 0) &&
        ((child_layer_restrict & LAYER_COLLECTION_HIDE) == 0)) {
      lc->runtime_flag |= LAYER_COLLECTION_VISIBLE;
    }

    /* Sync objects, except if collection was excluded. */
    for (CollectionObject *cob = collection->gobject.first; cob; cob = cob->next) {
      if (cob->ob == NULL) {
        continue;
      }

      void **base_p;
      Base *base;
      if (BLI_ghash_ensure_p(view_layer->object_bases_hash, cob->ob, &base_p)) {
        /* Move from old base list to new base list. Base might have already
         * been moved to the new base list and the first/last test ensure that
         * case also works. */
        base = *base_p;
        if (!ELEM(base, new_object_bases->first, new_object_bases->last)) {
          BLI_remlink(&view_layer->object_bases, base);
          BLI_addtail(new_object_bases, base);
        }
      }
      else {
        /* Create new base. */
        base = object_base_new(cob->ob);
        *base_p = base;
        BLI_addtail(new_object_bases, base);
      }

      if ((child_restrict & COLLECTION_RESTRICT_VIEWPORT) == 0) {
        base->flag_from_collection |= BASE_ENABLED_VIEWPORT;
        if ((child_layer_restrict & LAYER_COLLECTION_HIDE) == 0) {
          base->flag_from_collection |= BASE_VISIBLE;
          if (((child_restrict & COLLECTION_RESTRICT_SELECT) == 0)) {
            base->flag_from_collection |= BASE_SELECTABLE;
          }
        }
      }

      if ((child_restrict & COLLECTION_RESTRICT_RENDER) == 0) {
        base->flag_from_collection |= BASE_ENABLED_RENDER;
      }

      /* Holdout and indirect only */
      if (lc->flag & LAYER_COLLECTION_HOLDOUT) {
        base->flag_from_collection |= BASE_HOLDOUT;
      }
      if (lc->flag & LAYER_COLLECTION_INDIRECT_ONLY) {
        base->flag_from_collection |= BASE_INDIRECT_ONLY;
      }

      lc->runtime_flag |= LAYER_COLLECTION_HAS_OBJECTS;

      /* Make sure flags on base are usable right away. */
      BKE_base_eval_flags(base);
    }

    runtime_flag |= lc->runtime_flag;
  }

  /* Replace layer collection list with new one. */
  *lb_layer = new_lb_layer;
  BLI_assert(BLI_listbase_count(lb_scene) == BLI_listbase_count(lb_layer));

  return runtime_flag;
}

/**
 * Update view layer collection tree from collections used in the scene.
 * This is used when collections are removed or added, both while editing
 * and on file loaded in case linked data changed or went missing.
 */
void BKE_layer_collection_sync(const Scene *scene, ViewLayer *view_layer)
{
  if (!scene->master_collection) {
    /* Happens for old files that don't have versioning applied yet. */
    return;
  }

  /* Free cache. */
  MEM_SAFE_FREE(view_layer->object_bases_array);

  /* Create object to base hash if it does not exist yet. */
  if (!view_layer->object_bases_hash) {
    view_layer_bases_hash_create(view_layer);
  }

  /* Clear visible and selectable flags to be reset. */
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    base->flag &= ~g_base_collection_flags;
    base->flag_from_collection &= ~g_base_collection_flags;
  }

  /* Generate new layer connections and object bases when collections changed. */
  CollectionChild child = {NULL, NULL, scene->master_collection};
  const ListBase collections = {&child, &child};
  ListBase new_object_bases = {NULL, NULL};

  const short parent_exclude = 0, parent_restrict = 0, parent_layer_restrict = 0;
  layer_collection_sync(view_layer,
                        &collections,
                        &view_layer->layer_collections,
                        &new_object_bases,
                        parent_exclude,
                        parent_restrict,
                        parent_layer_restrict);

  /* Any remaning object bases are to be removed. */
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    if (view_layer->basact == base) {
      view_layer->basact = NULL;
    }

    if (base->object) {
      BLI_ghash_remove(view_layer->object_bases_hash, base->object, NULL, NULL);
    }
  }

  BLI_freelistN(&view_layer->object_bases);
  view_layer->object_bases = new_object_bases;

  /* Always set a valid active collection. */
  LayerCollection *active = view_layer->active_collection;

  if (active && (active->flag & LAYER_COLLECTION_EXCLUDE)) {
    BKE_layer_collection_activate_parent(view_layer, active);
  }
  else if (active == NULL) {
    view_layer->active_collection = view_layer->layer_collections.first;
  }
}

void BKE_scene_collection_sync(const Scene *scene)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    BKE_layer_collection_sync(scene, view_layer);
  }
}

void BKE_main_collection_sync(const Main *bmain)
{
  /* TODO: if a single collection changed, figure out which
   * scenes it belongs to and only update those. */

  /* TODO: optimize for file load so only linked collections get checked? */

  for (const Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    BKE_scene_collection_sync(scene);
  }
}

void BKE_main_collection_sync_remap(const Main *bmain)
{
  /* On remapping of object or collection pointers free caches. */
  /* TODO: try to make this faster */

  for (const Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
         view_layer = view_layer->next) {
      MEM_SAFE_FREE(view_layer->object_bases_array);

      if (view_layer->object_bases_hash) {
        BLI_ghash_free(view_layer->object_bases_hash, NULL, NULL);
        view_layer->object_bases_hash = NULL;
      }
    }
  }

  for (Collection *collection = bmain->collections.first; collection;
       collection = collection->id.next) {
    BKE_collection_object_cache_free(collection);
    DEG_id_tag_update_ex((Main *)bmain, &collection->id, ID_RECALC_COPY_ON_WRITE);
  }

  BKE_main_collection_sync(bmain);
}

/* ---------------------------------------------------------------------- */

/**
 * Select all the objects of this layer collection
 *
 * It also select the objects that are in nested collections.
 * \note Recursive
 */
bool BKE_layer_collection_objects_select(ViewLayer *view_layer, LayerCollection *lc, bool deselect)
{
  if (lc->collection->flag & COLLECTION_RESTRICT_SELECT) {
    return false;
  }

  bool changed = false;

  if (!(lc->flag & LAYER_COLLECTION_EXCLUDE)) {
    for (CollectionObject *cob = lc->collection->gobject.first; cob; cob = cob->next) {
      Base *base = BKE_view_layer_base_find(view_layer, cob->ob);

      if (base) {
        if (deselect) {
          if (base->flag & BASE_SELECTED) {
            base->flag &= ~BASE_SELECTED;
            changed = true;
          }
        }
        else {
          if ((base->flag & BASE_SELECTABLE) && !(base->flag & BASE_SELECTED)) {
            base->flag |= BASE_SELECTED;
            changed = true;
          }
        }
      }
    }
  }

  for (LayerCollection *iter = lc->layer_collections.first; iter; iter = iter->next) {
    changed |= BKE_layer_collection_objects_select(view_layer, iter, deselect);
  }

  return changed;
}

bool BKE_layer_collection_has_selected_objects(ViewLayer *view_layer, LayerCollection *lc)
{
  if (lc->collection->flag & COLLECTION_RESTRICT_SELECT) {
    return false;
  }

  if (!(lc->flag & LAYER_COLLECTION_EXCLUDE)) {
    for (CollectionObject *cob = lc->collection->gobject.first; cob; cob = cob->next) {
      Base *base = BKE_view_layer_base_find(view_layer, cob->ob);

      if (base && (base->flag & BASE_SELECTED) && (base->flag & BASE_VISIBLE)) {
        return true;
      }
    }
  }

  for (LayerCollection *iter = lc->layer_collections.first; iter; iter = iter->next) {
    if (BKE_layer_collection_has_selected_objects(view_layer, iter)) {
      return true;
    }
  }

  return false;
}

bool BKE_layer_collection_has_layer_collection(LayerCollection *lc_parent,
                                               LayerCollection *lc_child)
{
  if (lc_parent == lc_child) {
    return true;
  }

  for (LayerCollection *lc_iter = lc_parent->layer_collections.first; lc_iter;
       lc_iter = lc_iter->next) {
    if (BKE_layer_collection_has_layer_collection(lc_iter, lc_child)) {
      return true;
    }
  }
  return false;
}

/* ---------------------------------------------------------------------- */

/* Update after toggling visibility of an object base. */
void BKE_base_set_visible(Scene *scene, ViewLayer *view_layer, Base *base, bool extend)
{
  if (!extend) {
    /* Make only one base visible. */
    for (Base *other = view_layer->object_bases.first; other; other = other->next) {
      other->flag |= BASE_HIDDEN;
    }

    base->flag &= ~BASE_HIDDEN;
  }
  else {
    /* Toggle visibility of one base. */
    base->flag ^= BASE_HIDDEN;
  }

  BKE_layer_collection_sync(scene, view_layer);
}

static void layer_collection_flag_set_recursive(LayerCollection *lc, const int flag)
{
  lc->flag |= flag;
  for (LayerCollection *lc_iter = lc->layer_collections.first; lc_iter; lc_iter = lc_iter->next) {
    layer_collection_flag_set_recursive(lc_iter, flag);
  }
}

static void layer_collection_flag_unset_recursive(LayerCollection *lc, const int flag)
{
  lc->flag &= ~flag;
  for (LayerCollection *lc_iter = lc->layer_collections.first; lc_iter; lc_iter = lc_iter->next) {
    layer_collection_flag_unset_recursive(lc_iter, flag);
  }
}

/**
 * Isolate the collection - hide all other collections but this one.
 * Make sure to show all the direct parents and all children of the layer collection as well.
 * When extending we simply show the collections and its direct family.
 *
 * If the collection or any of its parents is disabled, make it enabled.
 * Don't change the children disable state though.
 *
 * Return whether depsgraph needs update.
 */
bool BKE_layer_collection_isolate(Scene *scene,
                                  ViewLayer *view_layer,
                                  LayerCollection *lc,
                                  bool extend)
{
  bool depsgraph_need_update = false;
  LayerCollection *lc_master = view_layer->layer_collections.first;
  bool hide_it = extend && (lc->runtime_flag & LAYER_COLLECTION_VISIBLE);

  if ((!ID_IS_LINKED(lc->collection) && !hide_it)) {
    if (lc->collection->flag & COLLECTION_RESTRICT_VIEWPORT) {
      lc->collection->flag &= ~COLLECTION_RESTRICT_VIEWPORT;
      depsgraph_need_update = true;
    }
  }

  if (!extend) {
    /* Hide all collections . */
    for (LayerCollection *lc_iter = lc_master->layer_collections.first; lc_iter;
         lc_iter = lc_iter->next) {
      layer_collection_flag_set_recursive(lc_iter, LAYER_COLLECTION_HIDE);
    }
  }

  /* Make all the direct parents visible. */
  if (hide_it) {
    lc->flag |= LAYER_COLLECTION_HIDE;
  }
  else {
    LayerCollection *lc_parent = lc;
    for (LayerCollection *lc_iter = lc_master->layer_collections.first; lc_iter;
         lc_iter = lc_iter->next) {
      if (BKE_layer_collection_has_layer_collection(lc_iter, lc)) {
        lc_parent = lc_iter;
        break;
      }
    }

    while (lc_parent != lc) {
      if (!ID_IS_LINKED(lc_parent->collection)) {
        if (lc_parent->collection->flag & COLLECTION_RESTRICT_VIEWPORT) {
          lc_parent->collection->flag &= ~COLLECTION_RESTRICT_VIEWPORT;
          depsgraph_need_update = true;
        }
      }

      lc_parent->flag &= ~LAYER_COLLECTION_HIDE;

      for (LayerCollection *lc_iter = lc_parent->layer_collections.first; lc_iter;
           lc_iter = lc_iter->next) {
        if (BKE_layer_collection_has_layer_collection(lc_iter, lc)) {
          lc_parent = lc_iter;
          break;
        }
      }
    }

    /* Make all the children visible, but respect their disable state. */
    layer_collection_flag_unset_recursive(lc, LAYER_COLLECTION_HIDE);

    BKE_layer_collection_activate(view_layer, lc);
  }

  BKE_layer_collection_sync(scene, view_layer);

  return depsgraph_need_update;
}

static void layer_collection_bases_show_recursive(ViewLayer *view_layer, LayerCollection *lc)
{
  for (CollectionObject *cob = lc->collection->gobject.first; cob; cob = cob->next) {
    Base *base = BKE_view_layer_base_find(view_layer, cob->ob);
    base->flag &= ~BASE_HIDDEN;
  }
  for (LayerCollection *lc_iter = lc->layer_collections.first; lc_iter; lc_iter = lc_iter->next) {
    layer_collection_bases_show_recursive(view_layer, lc_iter);
  }
}

static void layer_collection_bases_hide_recursive(ViewLayer *view_layer, LayerCollection *lc)
{
  for (CollectionObject *cob = lc->collection->gobject.first; cob; cob = cob->next) {
    Base *base = BKE_view_layer_base_find(view_layer, cob->ob);
    base->flag |= BASE_HIDDEN;
  }
  for (LayerCollection *lc_iter = lc->layer_collections.first; lc_iter; lc_iter = lc_iter->next) {
    layer_collection_bases_hide_recursive(view_layer, lc_iter);
  }
}

/**
 * Hide/show all the elements of a collection.
 * Don't change the collection children enable/disable state,
 * but it may change it for the collection itself.
 *
 * Return true if depsgraph needs update.
 */
bool BKE_layer_collection_set_visible(ViewLayer *view_layer,
                                      LayerCollection *lc,
                                      const bool visible,
                                      const bool hierarchy)
{
  bool depsgraph_changed = false;

  if (visible && (!ID_IS_LINKED(lc->collection)) &&
      ((lc->collection->flag & COLLECTION_RESTRICT_VIEWPORT) != 0)) {
    lc->collection->flag &= ~COLLECTION_RESTRICT_VIEWPORT;
    depsgraph_changed = true;
  }

  if (hierarchy) {
    if (visible) {
      layer_collection_flag_unset_recursive(lc, LAYER_COLLECTION_HIDE);
      layer_collection_bases_show_recursive(view_layer, lc);
    }
    else {
      layer_collection_flag_set_recursive(lc, LAYER_COLLECTION_HIDE);
      layer_collection_bases_hide_recursive(view_layer, lc);
    }
  }
  else {
    if (visible) {
      lc->flag &= ~LAYER_COLLECTION_HIDE;
    }
    else {
      lc->flag |= LAYER_COLLECTION_HIDE;
    }
  }
  return depsgraph_changed;
}

/* ---------------------------------------------------------------------- */

static LayerCollection *find_layer_collection_by_scene_collection(LayerCollection *lc,
                                                                  const Collection *collection)
{
  if (lc->collection == collection) {
    return lc;
  }

  for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
    LayerCollection *found = find_layer_collection_by_scene_collection(nlc, collection);
    if (found) {
      return found;
    }
  }
  return NULL;
}

/**
 * Return the first matching LayerCollection in the ViewLayer for the Collection.
 */
LayerCollection *BKE_layer_collection_first_from_scene_collection(ViewLayer *view_layer,
                                                                  const Collection *collection)
{
  for (LayerCollection *layer_collection = view_layer->layer_collections.first;
       layer_collection != NULL;
       layer_collection = layer_collection->next) {
    LayerCollection *found = find_layer_collection_by_scene_collection(layer_collection,
                                                                       collection);

    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

/**
 * See if view layer has the scene collection linked directly, or indirectly (nested)
 */
bool BKE_view_layer_has_collection(ViewLayer *view_layer, const Collection *collection)
{
  return BKE_layer_collection_first_from_scene_collection(view_layer, collection) != NULL;
}

/**
 * See if the object is in any of the scene layers of the scene
 */
bool BKE_scene_has_object(Scene *scene, Object *ob)
{
  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base) {
      return true;
    }
  }
  return false;
}

/** \} */

/* Iterators */

/* -------------------------------------------------------------------- */
/** \name Private Iterator Helpers
 * \{ */

typedef struct LayerObjectBaseIteratorData {
  View3D *v3d;
  Base *base;
} LayerObjectBaseIteratorData;

static bool object_bases_iterator_is_valid(View3D *v3d, Base *base, const int flag)
{
  BLI_assert((v3d == NULL) || (v3d->spacetype == SPACE_VIEW3D));

  /* Any flag satisfies the condition. */
  if (flag == ~0) {
    return (base->flag != 0);
  }

  /* Flags may be more than one flag, so we can't check != 0. */
  return BASE_VISIBLE(v3d, base) && ((base->flag & flag) == flag);
}

static void object_bases_iterator_begin(BLI_Iterator *iter, void *data_in_v, const int flag)
{
  ObjectsVisibleIteratorData *data_in = data_in_v;
  ViewLayer *view_layer = data_in->view_layer;
  View3D *v3d = data_in->v3d;
  Base *base = view_layer->object_bases.first;

  /* when there are no objects */
  if (base == NULL) {
    iter->data = NULL;
    iter->valid = false;
    return;
  }

  LayerObjectBaseIteratorData *data = MEM_callocN(sizeof(LayerObjectBaseIteratorData), __func__);
  iter->data = data;

  data->v3d = v3d;
  data->base = base;

  if (object_bases_iterator_is_valid(v3d, base, flag) == false) {
    object_bases_iterator_next(iter, flag);
  }
  else {
    iter->current = base;
  }
}

static void object_bases_iterator_next(BLI_Iterator *iter, const int flag)
{
  LayerObjectBaseIteratorData *data = iter->data;
  Base *base = data->base->next;

  while (base) {
    if (object_bases_iterator_is_valid(data->v3d, base, flag)) {
      iter->current = base;
      data->base = base;
      return;
    }
    base = base->next;
  }

  iter->valid = false;
}

static void object_bases_iterator_end(BLI_Iterator *iter)
{
  MEM_SAFE_FREE(iter->data);
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

static void objects_iterator_end(BLI_Iterator *iter)
{
  object_bases_iterator_end(iter);
}

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_objects_iterator
 * See: #FOREACH_SELECTED_OBJECT_BEGIN
 * \{ */

void BKE_view_layer_selected_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  objects_iterator_begin(iter, data_in, BASE_VISIBLE | BASE_SELECTED);
}

void BKE_view_layer_selected_objects_iterator_next(BLI_Iterator *iter)
{
  objects_iterator_next(iter, BASE_VISIBLE | BASE_SELECTED);
}

void BKE_view_layer_selected_objects_iterator_end(BLI_Iterator *iter)
{
  objects_iterator_end(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_visible_objects_iterator
 * \{ */

void BKE_view_layer_visible_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  objects_iterator_begin(iter, data_in, 0);
}

void BKE_view_layer_visible_objects_iterator_next(BLI_Iterator *iter)
{
  objects_iterator_next(iter, 0);
}

void BKE_view_layer_visible_objects_iterator_end(BLI_Iterator *iter)
{
  objects_iterator_end(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_editable_objects_iterator
 * \{ */

void BKE_view_layer_selected_editable_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  objects_iterator_begin(iter, data_in, BASE_VISIBLE | BASE_SELECTED);
  if (iter->valid) {
    if (BKE_object_is_libdata((Object *)iter->current) == false) {
      // First object is valid (selectable and not libdata) -> all good.
      return;
    }
    else {
      // Object is selectable but not editable -> search for another one.
      BKE_view_layer_selected_editable_objects_iterator_next(iter);
    }
  }
}

void BKE_view_layer_selected_editable_objects_iterator_next(BLI_Iterator *iter)
{
  // Search while there are objects and the one we have is not editable (editable = not libdata).
  do {
    objects_iterator_next(iter, BASE_VISIBLE | BASE_SELECTED);
  } while (iter->valid && BKE_object_is_libdata((Object *)iter->current) != false);
}

void BKE_view_layer_selected_editable_objects_iterator_end(BLI_Iterator *iter)
{
  objects_iterator_end(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_selected_bases_iterator
 * \{ */

void BKE_view_layer_selected_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  objects_iterator_begin(iter, data_in, BASE_VISIBLE | BASE_SELECTED);
}

void BKE_view_layer_selected_bases_iterator_next(BLI_Iterator *iter)
{
  object_bases_iterator_next(iter, BASE_VISIBLE | BASE_SELECTED);
}

void BKE_view_layer_selected_bases_iterator_end(BLI_Iterator *iter)
{
  object_bases_iterator_end(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_visible_bases_iterator
 * \{ */

void BKE_view_layer_visible_bases_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  object_bases_iterator_begin(iter, data_in, 0);
}

void BKE_view_layer_visible_bases_iterator_next(BLI_Iterator *iter)
{
  object_bases_iterator_next(iter, 0);
}

void BKE_view_layer_visible_bases_iterator_end(BLI_Iterator *iter)
{
  object_bases_iterator_end(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE_view_layer_bases_in_mode_iterator
 * \{ */

static bool base_is_in_mode(struct ObjectsInModeIteratorData *data, Base *base)
{
  return BASE_VISIBLE(data->v3d, base) && (base->object->type == data->object_type) &&
         (base->object->mode & data->object_mode) != 0;
}

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

  /* default type is active object type */
  if (data->object_type < 0) {
    data->object_type = base->object->type;
  }

  if (!base_is_in_mode(data, base)) {
    BKE_view_layer_bases_in_mode_iterator_next(iter);
  }
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
    if ((base != data->base_active) && base_is_in_mode(data, base)) {
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

/* Applies object's restrict flags on top of flags coming from the collection
 * and stores those in base->flag. BASE_VISIBLE is based on viewport visibility. */
void BKE_base_eval_flags(Base *base)
{
  /* Apply collection flags. */
  base->flag &= ~g_base_collection_flags;
  base->flag |= (base->flag_from_collection & g_base_collection_flags);

  /* Apply object restrictions. */
  const int object_restrict = base->object->restrictflag;
  if (object_restrict & OB_RESTRICT_VIEWPORT) {
    base->flag &= ~BASE_ENABLED_VIEWPORT;
  }
  if (object_restrict & OB_RESTRICT_RENDER) {
    base->flag &= ~BASE_ENABLED_RENDER;
  }
  if (object_restrict & OB_RESTRICT_SELECT) {
    base->flag &= ~BASE_SELECTABLE;
  }

  /* Apply viewport visibility by default. The dependency graph for render
   * can change these again, but for tools we always want the viewport
   * visibility to be in sync regardless if depsgraph was evaluated. */
  if (!(base->flag & BASE_ENABLED_VIEWPORT) || (base->flag & BASE_HIDDEN)) {
    base->flag &= ~(BASE_VISIBLE | BASE_SELECTABLE);
  }

  /* Deselect unselectable objects. */
  if (!(base->flag & BASE_SELECTABLE)) {
    base->flag &= ~BASE_SELECTED;
  }
}

static void layer_eval_view_layer(struct Depsgraph *depsgraph,
                                  struct Scene *UNUSED(scene),
                                  ViewLayer *view_layer)
{
  DEG_debug_print_eval(depsgraph, __func__, view_layer->name, view_layer);

  /* Create array of bases, for fast index-based lookup. */
  const int num_object_bases = BLI_listbase_count(&view_layer->object_bases);
  MEM_SAFE_FREE(view_layer->object_bases_array);
  view_layer->object_bases_array = MEM_malloc_arrayN(
      num_object_bases, sizeof(Base *), "view_layer->object_bases_array");
  int base_index = 0;
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    view_layer->object_bases_array[base_index++] = base;
  }
}

void BKE_layer_eval_view_layer_indexed(struct Depsgraph *depsgraph,
                                       struct Scene *scene,
                                       int view_layer_index)
{
  BLI_assert(view_layer_index >= 0);
  ViewLayer *view_layer = BLI_findlink(&scene->view_layers, view_layer_index);
  BLI_assert(view_layer != NULL);
  layer_eval_view_layer(depsgraph, scene, view_layer);
}
