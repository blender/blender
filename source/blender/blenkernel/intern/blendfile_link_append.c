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
 *
 * High level `.blend` file link/append code,
 * including linking/appending several IDs from different libraries, handling instanciations of
 * collections/objects/obdata in current scene.
 */

#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"

#include "BKE_blendfile_link_append.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

static CLG_LogRef LOG = {"bke.blendfile_link_append"};

/* -------------------------------------------------------------------- */
/** \name Link/append context implementation and public management API.
 * \{ */

typedef struct BlendfileLinkAppendContextItem {
  /** Name of the ID (without the heading two-chars IDcode). */
  char *name;
  /** All libs (from BlendfileLinkAppendContext.libraries) to try to load this ID from. */
  BLI_bitmap *libraries;
  /** ID type. */
  short idcode;

  /** Type of action to perform on this item, and general status tag information.
   *  NOTE: Mostly used by append post-linking processing. */
  char action;
  char tag;

  /** Newly linked ID (NULL until it has been successfully linked). */
  ID *new_id;
  /** Library ID from which the #new_id has been linked (NULL until it has been successfully
   * linked). */
  Library *source_library;
  /** Opaque user data pointer. */
  void *userdata;
} BlendfileLinkAppendContextItem;

typedef struct BlendfileLinkAppendContext {
  /** List of library paths to search IDs in. */
  LinkNodePair libraries;
  /** List of all ID to try to link from #libraries. */
  LinkNodePair items;
  int num_libraries;
  int num_items;
  /**
   * Combines #eFileSel_Params_Flag from DNA_space_types.h & #eBLOLibLinkFlags from BLO_readfile.h
   */
  int flag;

  /** Allows to easily find an existing items from an ID pointer. */
  GHash *new_id_to_item;

  /** Runtime info used by append code to manage re-use of already appended matching IDs. */
  GHash *library_weak_reference_mapping;

  /** Embedded blendfile and its size, if needed. */
  const void *blendfile_mem;
  size_t blendfile_memsize;

  /** Internal 'private' data */
  MemArena *memarena;
} BlendfileLinkAppendContext;

typedef struct BlendfileLinkAppendContextCallBack {
  BlendfileLinkAppendContext *lapp_context;
  BlendfileLinkAppendContextItem *item;
  ReportList *reports;

} BlendfileLinkAppendContextCallBack;

/* Actions to apply to an item (i.e. linked ID). */
enum {
  LINK_APPEND_ACT_UNSET = 0,
  LINK_APPEND_ACT_KEEP_LINKED,
  LINK_APPEND_ACT_REUSE_LOCAL,
  LINK_APPEND_ACT_MAKE_LOCAL,
  LINK_APPEND_ACT_COPY_LOCAL,
};

/* Various status info about an item (i.e. linked ID). */
enum {
  /* An indirectly linked ID. */
  LINK_APPEND_TAG_INDIRECT = 1 << 0,
};

/** Allocate and initialize a new context to link/append datablocks.
 *
 *  \param flag a combination of #eFileSel_Params_Flag from DNA_space_types.h & #eBLOLibLinkFlags
 * from BLO_readfile.h
 */
BlendfileLinkAppendContext *BKE_blendfile_link_append_context_new(const int flag)
{
  MemArena *ma = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  BlendfileLinkAppendContext *lapp_context = BLI_memarena_calloc(ma, sizeof(*lapp_context));

  lapp_context->flag = flag;
  lapp_context->memarena = ma;

  return lapp_context;
}

/** Free a link/append context. */
void BKE_blendfile_link_append_context_free(BlendfileLinkAppendContext *lapp_context)
{
  if (lapp_context->new_id_to_item != NULL) {
    BLI_ghash_free(lapp_context->new_id_to_item, NULL, NULL);
  }

  BLI_assert(lapp_context->library_weak_reference_mapping == NULL);

  BLI_memarena_free(lapp_context->memarena);
}

/** Set or clear flags in given \a lapp_context.
 *
 * \param do_set Set the given \a flag if true, clear it otherwise.
 */
void BKE_blendfile_link_append_context_flag_set(BlendfileLinkAppendContext *lapp_context,
                                                const int flag,
                                                const bool do_set)
{
  if (do_set) {
    lapp_context->flag |= flag;
  }
  else {
    lapp_context->flag &= ~flag;
  }
}

/** Store reference to a Blender's embedded memfile into the context.
 *
 * \note This is required since embedded startup blender file is handled in `ED` module, which
 * cannot be linked in BKE code.
 */
void BKE_blendfile_link_append_context_embedded_blendfile_set(
    BlendfileLinkAppendContext *lapp_context, const void *blendfile_mem, int blendfile_memsize)
{
  BLI_assert_msg(lapp_context->blendfile_mem == NULL,
                 "Please explicitely clear reference to an embedded blender memfile before "
                 "setting a new one");
  lapp_context->blendfile_mem = blendfile_mem;
  lapp_context->blendfile_memsize = (size_t)blendfile_memsize;
}

/** Clear reference to Blender's embedded startup file into the context. */
void BKE_blendfile_link_append_context_embedded_blendfile_clear(
    BlendfileLinkAppendContext *lapp_context)
{
  lapp_context->blendfile_mem = NULL;
  lapp_context->blendfile_memsize = 0;
}

/** Add a new source library to search for items to be linked to the given link/append context.
 *
 * \note *Never* call BKE_blendfile_link_append_context_library_add() after having added some
 * items. */
void BKE_blendfile_link_append_context_library_add(BlendfileLinkAppendContext *lapp_context,
                                                   const char *libname)
{
  BLI_assert(lapp_context->items.list == NULL);

  size_t len = strlen(libname) + 1;
  char *libpath = BLI_memarena_alloc(lapp_context->memarena, len);

  BLI_strncpy(libpath, libname, len);
  BLI_linklist_append_arena(&lapp_context->libraries, libpath, lapp_context->memarena);
  lapp_context->num_libraries++;
}

/** Add a new item (datablock name and idcode) to be searched and linked/appended from libraries
 * associated to the given context.
 *
 * \param userdata: an opaque user-data pointer stored in generated link/append item. */
BlendfileLinkAppendContextItem *BKE_blendfile_link_append_context_item_add(
    BlendfileLinkAppendContext *lapp_context,
    const char *idname,
    const short idcode,
    void *userdata)
{
  BlendfileLinkAppendContextItem *item = BLI_memarena_calloc(lapp_context->memarena,
                                                             sizeof(*item));
  size_t len = strlen(idname) + 1;

  item->name = BLI_memarena_alloc(lapp_context->memarena, len);
  BLI_strncpy(item->name, idname, len);
  item->idcode = idcode;
  item->libraries = BLI_BITMAP_NEW_MEMARENA(lapp_context->memarena, lapp_context->num_libraries);

  item->new_id = NULL;
  item->action = LINK_APPEND_ACT_UNSET;
  item->userdata = userdata;

  BLI_linklist_append_arena(&lapp_context->items, item, lapp_context->memarena);
  lapp_context->num_items++;

  return item;
}

/** Enable search of the given \a item into the library stored at given index in the link/append
 * context. */
void BKE_blendfile_link_append_context_item_library_index_enable(
    BlendfileLinkAppendContext *UNUSED(lapp_context),
    BlendfileLinkAppendContextItem *item,
    const int library_index)
{
  BLI_BITMAP_ENABLE(item->libraries, library_index);
}

/** Check if given link/append context is empty (has no items to process) or not. */
bool BKE_blendfile_link_append_context_is_empty(struct BlendfileLinkAppendContext *lapp_context)
{
  return lapp_context->num_items == 0;
}

void *BKE_blendfile_link_append_context_item_userdata_get(
    BlendfileLinkAppendContext *UNUSED(lapp_context), BlendfileLinkAppendContextItem *item)
{
  return item->userdata;
}

ID *BKE_blendfile_link_append_context_item_newid_get(
    BlendfileLinkAppendContext *UNUSED(lapp_context), BlendfileLinkAppendContextItem *item)
{
  return item->new_id;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library link/append helper functions.
 *
 *  FIXME: Deduplicate code with similar one in readfile.c
 * \{ */

static bool object_in_any_scene(Main *bmain, Object *ob)
{
  LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
    if (BKE_scene_object_find(sce, ob)) {
      return true;
    }
  }

  return false;
}

static bool object_in_any_collection(Main *bmain, Object *ob)
{
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (BKE_collection_has_object(collection, ob)) {
      return true;
    }
  }

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->master_collection != NULL &&
        BKE_collection_has_object(scene->master_collection, ob)) {
      return true;
    }
  }

  return false;
}

static ID *append_loose_data_instantiate_process_check(BlendfileLinkAppendContextItem *item)
{
  /* We consider that if we either kept it linked, or re-used already local data, instantiation
   * status of those should not be modified. */
  if (!ELEM(item->action, LINK_APPEND_ACT_COPY_LOCAL, LINK_APPEND_ACT_MAKE_LOCAL)) {
    return NULL;
  }

  ID *id = item->new_id;
  if (id == NULL) {
    return NULL;
  }

  if (item->action == LINK_APPEND_ACT_COPY_LOCAL) {
    BLI_assert(ID_IS_LINKED(id));
    id = id->newid;
    if (id == NULL) {
      return NULL;
    }

    BLI_assert(!ID_IS_LINKED(id));
    return id;
  }

  BLI_assert(!ID_IS_LINKED(id));
  return id;
}

static void append_loose_data_instantiate_ensure_active_collection(
    BlendfileLinkAppendContext *lapp_context,
    Main *bmain,
    Scene *scene,
    ViewLayer *view_layer,
    Collection **r_active_collection)
{
  /* Find or add collection as needed. */
  if (*r_active_collection == NULL) {
    if (lapp_context->flag & FILE_ACTIVE_COLLECTION) {
      LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
      *r_active_collection = lc->collection;
    }
    else {
      *r_active_collection = BKE_collection_add(
          bmain, scene->master_collection, DATA_("Appended Data"));
    }
  }
}

/* TODO: De-duplicate this code with the one in readfile.c, think we need some utils code for that
 * in BKE. */
static void append_loose_data_instantiate(BlendfileLinkAppendContext *lapp_context,
                                          Main *bmain,
                                          Scene *scene,
                                          ViewLayer *view_layer,
                                          const View3D *v3d)
{
  if (scene == NULL) {
    /* In some cases, like the asset drag&drop e.g., the caller code manages instantiation itself.
     */
    return;
  }

  LinkNode *itemlink;
  Collection *active_collection = NULL;
  const bool do_obdata = (lapp_context->flag & BLO_LIBLINK_OBDATA_INSTANCE) != 0;

  /* Do NOT make base active here! screws up GUI stuff,
   * if you want it do it at the editor level. */
  const bool object_set_active = false;

  /* First pass on obdata to enable their instantiation by default, then do a second pass on
   * objects to clear it for any obdata already in use. */
  if (do_obdata) {
    for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
      BlendfileLinkAppendContextItem *item = itemlink->link;
      ID *id = append_loose_data_instantiate_process_check(item);
      if (id == NULL) {
        continue;
      }
      const ID_Type idcode = GS(id->name);
      if (!OB_DATA_SUPPORT_ID(idcode)) {
        continue;
      }

      id->tag |= LIB_TAG_DOIT;
    }
    for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
      BlendfileLinkAppendContextItem *item = itemlink->link;
      ID *id = item->new_id;
      if (id == NULL || GS(id->name) != ID_OB) {
        continue;
      }

      Object *ob = (Object *)id;
      Object *new_ob = (Object *)id->newid;
      if (ob->data != NULL) {
        ((ID *)(ob->data))->tag &= ~LIB_TAG_DOIT;
      }
      if (new_ob != NULL && new_ob->data != NULL) {
        ((ID *)(new_ob->data))->tag &= ~LIB_TAG_DOIT;
      }
    }
  }

  /* First do collections, then objects, then obdata. */

  /* NOTE: For collections we only view_layer-instantiate duplicated collections that have
   * non-instantiated objects in them. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = append_loose_data_instantiate_process_check(item);
    if (id == NULL || GS(id->name) != ID_GR) {
      continue;
    }

    /* We do not want to force instantiation of indirectly appended collections. Users can now
     * easily instantiate collections (and their objects) as needed by themselves. See T67032. */
    /* We need to check that objects in that collections are already instantiated in a scene.
     * Otherwise, it's better to add the collection to the scene's active collection, than to
     * instantiate its objects in active scene's collection directly. See T61141.
     *
     * NOTE: We only check object directly into that collection, not recursively into its
     * children.
     */
    Collection *collection = (Collection *)id;
    /* We always add collections directly selected by the user. */
    bool do_add_collection = (item->tag & LINK_APPEND_TAG_INDIRECT) == 0;
    if (!do_add_collection) {
      LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
        Object *ob = coll_ob->ob;
        if (!object_in_any_scene(bmain, ob)) {
          do_add_collection = true;
          break;
        }
      }
    }
    if (do_add_collection) {
      append_loose_data_instantiate_ensure_active_collection(
          lapp_context, bmain, scene, view_layer, &active_collection);

      /* In case user requested instantiation of collections as empties, we do so for the one they
       * explicitly selected (originally directly linked IDs). */
      if ((lapp_context->flag & BLO_LIBLINK_COLLECTION_INSTANCE) != 0 &&
          (item->tag & LINK_APPEND_TAG_INDIRECT) == 0) {
        /* BKE_object_add(...) messes with the selection. */
        Object *ob = BKE_object_add_only_object(bmain, OB_EMPTY, collection->id.name + 2);
        ob->type = OB_EMPTY;
        ob->empty_drawsize = U.collection_instance_empty_size;

        const bool set_selected = (lapp_context->flag & FILE_AUTOSELECT) != 0;
        /* TODO: why is it OK to make this active here but not in other situations?
         * See other callers of #object_base_instance_init */
        const bool set_active = set_selected;
        BLO_object_instantiate_object_base_instance_init(
            bmain, active_collection, ob, view_layer, v3d, lapp_context->flag, set_active);

        /* Assign the collection. */
        ob->instance_collection = collection;
        id_us_plus(&collection->id);
        ob->transflag |= OB_DUPLICOLLECTION;
        copy_v3_v3(ob->loc, scene->cursor.location);
      }
      else {
        /* Add collection as child of active collection. */
        BKE_collection_child_add(bmain, active_collection, collection);

        if ((lapp_context->flag & FILE_AUTOSELECT) != 0) {
          LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
            Object *ob = coll_ob->ob;
            Base *base = BKE_view_layer_base_find(view_layer, ob);
            if (base) {
              base->flag |= BASE_SELECTED;
              BKE_scene_object_base_flag_sync_from_base(base);
            }
          }
        }
      }
    }
  }

  /* NOTE: For objects we only view_layer-instantiate duplicated objects that are not yet used
   * anywhere. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = append_loose_data_instantiate_process_check(item);
    if (id == NULL || GS(id->name) != ID_OB) {
      continue;
    }

    Object *ob = (Object *)id;

    if (object_in_any_collection(bmain, ob)) {
      continue;
    }

    append_loose_data_instantiate_ensure_active_collection(
        lapp_context, bmain, scene, view_layer, &active_collection);

    CLAMP_MIN(ob->id.us, 0);
    ob->mode = OB_MODE_OBJECT;

    BLO_object_instantiate_object_base_instance_init(
        bmain, active_collection, ob, view_layer, v3d, lapp_context->flag, object_set_active);
  }

  if (!do_obdata) {
    return;
  }

  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = append_loose_data_instantiate_process_check(item);
    if (id == NULL) {
      continue;
    }
    const ID_Type idcode = GS(id->name);
    if (!OB_DATA_SUPPORT_ID(idcode)) {
      continue;
    }
    if ((id->tag & LIB_TAG_DOIT) == 0) {
      continue;
    }

    append_loose_data_instantiate_ensure_active_collection(
        lapp_context, bmain, scene, view_layer, &active_collection);

    const int type = BKE_object_obdata_to_type(id);
    BLI_assert(type != -1);
    Object *ob = BKE_object_add_only_object(bmain, type, id->name + 2);
    ob->data = id;
    id_us_plus(id);
    BKE_object_materials_test(bmain, ob, ob->data);

    BLO_object_instantiate_object_base_instance_init(
        bmain, active_collection, ob, view_layer, v3d, lapp_context->flag, object_set_active);

    copy_v3_v3(ob->loc, scene->cursor.location);

    id->tag &= ~LIB_TAG_DOIT;
  }

  /* Finally, add rigid body objects and constraints to current RB world(s). */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = append_loose_data_instantiate_process_check(item);
    if (id == NULL || GS(id->name) != ID_OB) {
      continue;
    }
    BKE_rigidbody_ensure_local_object(bmain, (Object *)id);
  }
}

/** \} */

static int foreach_libblock_append_callback(LibraryIDLinkCallbackData *cb_data)
{
  /* NOTE: It is important to also skip liboverride references here, as those should never be made
   * local. */
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_INTERNAL | IDWALK_CB_LOOPBACK |
                          IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE)) {
    return IDWALK_RET_NOP;
  }

  BlendfileLinkAppendContextCallBack *data = cb_data->user_data;
  ID *id = *cb_data->id_pointer;

  if (id == NULL) {
    return IDWALK_RET_NOP;
  }

  if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
    /* While we do not want to add non-linkable ID (shape keys...) to the list of linked items,
     * unfortunately they can use fully linkable valid IDs too, like actions. Those need to be
     * processed, so we need to recursively deal with them here. */
    /* NOTE: Since we are by-passing checks in `BKE_library_foreach_ID_link` by manually calling it
     * recursively, we need to take care of potential recursion cases ourselves (e.g.animdata of
     * shapekey referencing the shapekey itself). */
    if (id != cb_data->id_self) {
      BKE_library_foreach_ID_link(
          cb_data->bmain, id, foreach_libblock_append_callback, data, IDWALK_NOP);
    }
    return IDWALK_RET_NOP;
  }

  const bool do_recursive = (data->lapp_context->flag & BLO_LIBLINK_APPEND_RECURSIVE) != 0;
  if (!do_recursive && cb_data->id_owner->lib != id->lib) {
    /* When `do_recursive` is false, we only make local IDs from same library(-ies) as the
     * initially directly linked ones.
     * NOTE: Since linked IDs are also fully skipped during instantiation step (see
     * #append_loose_data_instantiate_process_check), we can avoid adding them to the items list
     * completely. */
    return IDWALK_RET_NOP;
  }

  BlendfileLinkAppendContextItem *item = BLI_ghash_lookup(data->lapp_context->new_id_to_item, id);
  if (item == NULL) {
    item = BKE_blendfile_link_append_context_item_add(
        data->lapp_context, id->name, GS(id->name), NULL);
    item->new_id = id;
    item->source_library = id->lib;
    /* Since we did not have an item for that ID yet, we know user did not selected it explicitly,
     * it was rather linked indirectly. This info is important for instantiation of collections. */
    item->tag |= LINK_APPEND_TAG_INDIRECT;
    BLI_ghash_insert(data->lapp_context->new_id_to_item, id, item);
  }

  /* NOTE: currently there is no need to do anything else here, but in the future this would be
   * the place to add specific per-usage decisions on how to append an ID. */

  return IDWALK_RET_NOP;
}

/** \name Library link/append code.
 * \{ */

/* Perform append operation, using modern ID usage looper to detect which ID should be kept linked,
 * made local, duplicated as local, re-used from local etc.
 *
 * TODO: Expose somehow this logic to the two other parts of code performing actual append
 * (i.e. copy/paste and `bpy` link/append API).
 * Then we can heavily simplify #BKE_library_make_local(). */
void BKE_blendfile_append(BlendfileLinkAppendContext *lapp_context,
                          ReportList *reports,
                          Main *bmain,
                          Scene *scene,
                          ViewLayer *view_layer,
                          const View3D *v3d)
{
  BLI_assert((lapp_context->flag & FILE_LINK) == 0);

  const bool set_fakeuser = (lapp_context->flag & BLO_LIBLINK_APPEND_SET_FAKEUSER) != 0;
  const bool do_reuse_local_id = (lapp_context->flag & BLO_LIBLINK_APPEND_LOCAL_ID_REUSE) != 0;

  const int make_local_common_flags = LIB_ID_MAKELOCAL_FULL_LIBRARY |
                                      ((lapp_context->flag &
                                        BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR) != 0 ?
                                           LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR :
                                           0);

  LinkNode *itemlink;

  /* Generate a mapping between newly linked IDs and their items, and tag linked IDs used as
   * liboverride references as already existing. */
  lapp_context->new_id_to_item = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_ghash_insert(lapp_context->new_id_to_item, id, item);

    /* This ensures that if a liboverride reference is also linked/used by some other appended
     * data, it gets a local copy instead of being made directly local, so that the liboverride
     * references remain valid (i.e. linked data). */
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      id->override_library->reference->tag |= LIB_TAG_PRE_EXISTING;
    }
  }

  lapp_context->library_weak_reference_mapping = BKE_main_library_weak_reference_create(bmain);

  /* NOTE: Since we append items for IDs not already listed (i.e. implicitly linked indirect
   * dependencies), this list will grow and we will process those IDs later, leading to a flatten
   * recursive processing of all the linked dependencies. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_assert(item->userdata == NULL);

    /* In Append case linked IDs should never be marked as needing post-processing (instantiation
     * of loose objects etc.). */
    BLI_assert((id->tag & LIB_TAG_DOIT) == 0);

    ID *existing_local_id = BKE_idtype_idcode_append_is_reusable(GS(id->name)) ?
                                BKE_main_library_weak_reference_search_item(
                                    lapp_context->library_weak_reference_mapping,
                                    id->lib->filepath,
                                    id->name) :
                                NULL;

    if (item->action != LINK_APPEND_ACT_UNSET) {
      /* Already set, pass. */
    }
    if (GS(id->name) == ID_OB && ((Object *)id)->proxy_from != NULL) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' is proxified, keeping it linked...", id->name);
      item->action = LINK_APPEND_ACT_KEEP_LINKED;
    }
    else if (do_reuse_local_id && existing_local_id != NULL) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' as a matching local one, re-using it...", id->name);
      item->action = LINK_APPEND_ACT_REUSE_LOCAL;
      item->userdata = existing_local_id;
    }
    else if (id->tag & LIB_TAG_PRE_EXISTING) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' was already linked, need to copy it...", id->name);
      item->action = LINK_APPEND_ACT_COPY_LOCAL;
    }
    else {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' will be made local...", id->name);
      item->action = LINK_APPEND_ACT_MAKE_LOCAL;
    }

    /* Only check dependencies if we are not keeping linked data, nor re-using existing local data.
     */
    if (!ELEM(item->action, LINK_APPEND_ACT_KEEP_LINKED, LINK_APPEND_ACT_REUSE_LOCAL)) {
      BlendfileLinkAppendContextCallBack cb_data = {
          .lapp_context = lapp_context, .item = item, .reports = reports};
      BKE_library_foreach_ID_link(
          bmain, id, foreach_libblock_append_callback, &cb_data, IDWALK_NOP);
    }

    /* If we found a matching existing local id but are not re-using it, we need to properly clear
     * its weak reference to linked data. */
    if (existing_local_id != NULL &&
        !ELEM(item->action, LINK_APPEND_ACT_KEEP_LINKED, LINK_APPEND_ACT_REUSE_LOCAL)) {
      BKE_main_library_weak_reference_remove_item(lapp_context->library_weak_reference_mapping,
                                                  id->lib->filepath,
                                                  id->name,
                                                  existing_local_id);
    }
  }

  /* Effectively perform required operation on every linked ID. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }

    ID *local_appended_new_id = NULL;
    char lib_filepath[FILE_MAX];
    BLI_strncpy(lib_filepath, id->lib->filepath, sizeof(lib_filepath));
    char lib_id_name[MAX_ID_NAME];
    BLI_strncpy(lib_id_name, id->name, sizeof(lib_id_name));

    switch (item->action) {
      case LINK_APPEND_ACT_COPY_LOCAL:
        BKE_lib_id_make_local(bmain, id, make_local_common_flags | LIB_ID_MAKELOCAL_FORCE_COPY);
        local_appended_new_id = id->newid;
        break;
      case LINK_APPEND_ACT_MAKE_LOCAL:
        BKE_lib_id_make_local(bmain,
                              id,
                              make_local_common_flags | LIB_ID_MAKELOCAL_FORCE_LOCAL |
                                  LIB_ID_MAKELOCAL_OBJECT_NO_PROXY_CLEARING);
        BLI_assert(id->newid == NULL);
        local_appended_new_id = id;
        break;
      case LINK_APPEND_ACT_KEEP_LINKED:
        /* Nothing to do here. */
        break;
      case LINK_APPEND_ACT_REUSE_LOCAL:
        /* We only need to set `newid` to ID found in previous loop, for proper remapping. */
        ID_NEW_SET(id, item->userdata);
        /* This is not a 'new' local appended id, do not set `local_appended_new_id` here. */
        break;
      case LINK_APPEND_ACT_UNSET:
        CLOG_ERROR(
            &LOG, "Unexpected unset append action for '%s' ID, assuming 'keep link'", id->name);
        break;
      default:
        BLI_assert(0);
    }

    if (local_appended_new_id != NULL) {
      if (BKE_idtype_idcode_append_is_reusable(GS(local_appended_new_id->name))) {
        BKE_main_library_weak_reference_add_item(lapp_context->library_weak_reference_mapping,
                                                 lib_filepath,
                                                 lib_id_name,
                                                 local_appended_new_id);
      }

      if (set_fakeuser) {
        if (!ELEM(GS(local_appended_new_id->name), ID_OB, ID_GR)) {
          /* Do not set fake user on objects nor collections (instancing). */
          id_fake_user_set(local_appended_new_id);
        }
      }
    }
  }

  BKE_main_library_weak_reference_destroy(lapp_context->library_weak_reference_mapping);
  lapp_context->library_weak_reference_mapping = NULL;

  /* Remap IDs as needed. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;

    if (item->action == LINK_APPEND_ACT_KEEP_LINKED) {
      continue;
    }

    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    if (ELEM(item->action, LINK_APPEND_ACT_COPY_LOCAL, LINK_APPEND_ACT_REUSE_LOCAL)) {
      BLI_assert(ID_IS_LINKED(id));
      id = id->newid;
      if (id == NULL) {
        continue;
      }
    }

    BLI_assert(!ID_IS_LINKED(id));

    BKE_libblock_relink_to_newid(bmain, id, 0);
  }

  /* Remove linked IDs when a local existing data has been reused instead. */
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;

    if (item->action != LINK_APPEND_ACT_REUSE_LOCAL) {
      continue;
    }

    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_assert(ID_IS_LINKED(id));
    BLI_assert(id->newid != NULL);

    id->tag |= LIB_TAG_DOIT;
    item->new_id = id->newid;
  }
  BKE_id_multi_tagged_delete(bmain);

  /* Instantiate newly created (duplicated) IDs as needed. */
  append_loose_data_instantiate(lapp_context, bmain, scene, view_layer, v3d);

  /* Attempt to deal with object proxies.
   *
   * NOTE: Copied from `BKE_library_make_local`, but this is not really working (as in, not
   * producing any useful result in any known use case), neither here nor in
   * `BKE_library_make_local` currently.
   * Proxies are end of life anyway, so not worth spending time on this. */
  for (itemlink = lapp_context->items.list; itemlink; itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;

    if (item->action != LINK_APPEND_ACT_COPY_LOCAL) {
      continue;
    }

    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_assert(ID_IS_LINKED(id));

    /* Attempt to re-link copied proxy objects. This allows appending of an entire scene
     * from another blend file into this one, even when that blend file contains proxified
     * armatures that have local references. Since the proxified object needs to be linked
     * (not local), this will only work when the "Localize all" checkbox is disabled.
     * TL;DR: this is a dirty hack on top of an already weak feature (proxies). */
    if (GS(id->name) == ID_OB && ((Object *)id)->proxy != NULL) {
      Object *ob = (Object *)id;
      Object *ob_new = (Object *)id->newid;
      bool is_local = false, is_lib = false;

      /* Proxies only work when the proxified object is linked-in from a library. */
      if (!ID_IS_LINKED(ob->proxy)) {
        CLOG_WARN(&LOG,
                  "Proxy object %s will lose its link to %s, because the "
                  "proxified object is local",
                  id->newid->name,
                  ob->proxy->id.name);
        continue;
      }

      BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

      /* We can only switch the proxy'ing to a made-local proxy if it is no longer
       * referred to from a library. Not checking for local use; if new local proxy
       * was not used locally would be a nasty bug! */
      if (is_local || is_lib) {
        CLOG_WARN(&LOG,
                  "Made-local proxy object %s will lose its link to %s, "
                  "because the linked-in proxy is referenced (is_local=%i, is_lib=%i)",
                  id->newid->name,
                  ob->proxy->id.name,
                  is_local,
                  is_lib);
      }
      else {
        /* we can switch the proxy'ing from the linked-in to the made-local proxy.
         * BKE_object_make_proxy() shouldn't be used here, as it allocates memory that
         * was already allocated by object_make_local() (which called BKE_object_copy). */
        ob_new->proxy = ob->proxy;
        ob_new->proxy_group = ob->proxy_group;
        ob_new->proxy_from = ob->proxy_from;
        ob_new->proxy->proxy_from = ob_new;
        ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
      }
    }
  }

  BKE_main_id_newptr_and_tag_clear(bmain);
}

void BKE_blendfile_link(BlendfileLinkAppendContext *lapp_context,
                        ReportList *reports,
                        Main *bmain,
                        Scene *scene,
                        ViewLayer *view_layer,
                        const View3D *v3d)
{
  Main *mainl;
  BlendHandle *bh;
  Library *lib;

  const int flag = lapp_context->flag;
  const int id_tag_extra = 0;

  LinkNode *liblink, *itemlink;
  int lib_idx, item_idx;

  BLI_assert(lapp_context->num_items && lapp_context->num_libraries);

  for (lib_idx = 0, liblink = lapp_context->libraries.list; liblink;
       lib_idx++, liblink = liblink->next) {
    char *libname = liblink->link;
    BlendFileReadReport bf_reports = {.reports = reports};

    if (STREQ(libname, BLO_EMBEDDED_STARTUP_BLEND)) {
      bh = BLO_blendhandle_from_memory(
          lapp_context->blendfile_mem, (int)lapp_context->blendfile_memsize, &bf_reports);
    }
    else {
      bh = BLO_blendhandle_from_file(libname, &bf_reports);
    }

    if (bh == NULL) {
      /* Unlikely since we just browsed it, but possible
       * Error reports will have been made by BLO_blendhandle_from_file() */
      continue;
    }

    /* here appending/linking starts */
    struct LibraryLink_Params liblink_params;
    BLO_library_link_params_init_with_context(
        &liblink_params, bmain, flag, id_tag_extra, scene, view_layer, v3d);
    /* In case of append, do not handle instantiation in linking process, but during append phase
     * (see #append_loose_data_instantiate ). */
    if ((flag & FILE_LINK) == 0) {
      liblink_params.flag &= ~BLO_LIBLINK_NEEDS_ID_TAG_DOIT;
    }

    mainl = BLO_library_link_begin(&bh, libname, &liblink_params);
    lib = mainl->curlib;
    BLI_assert(lib);
    UNUSED_VARS_NDEBUG(lib);

    if (mainl->versionfile < 250) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Linking or appending from a very old .blend file format (%d.%d), no animation "
                  "conversion will "
                  "be done! You may want to re-save your lib file with current Blender",
                  mainl->versionfile,
                  mainl->subversionfile);
    }

    /* For each lib file, we try to link all items belonging to that lib,
     * and tag those successful to not try to load them again with the other libs. */
    for (item_idx = 0, itemlink = lapp_context->items.list; itemlink;
         item_idx++, itemlink = itemlink->next) {
      BlendfileLinkAppendContextItem *item = itemlink->link;
      ID *new_id;

      if (!BLI_BITMAP_TEST(item->libraries, lib_idx)) {
        continue;
      }

      new_id = BLO_library_link_named_part(mainl, &bh, item->idcode, item->name, &liblink_params);

      if (new_id) {
        /* If the link is successful, clear item's libs 'todo' flags.
         * This avoids trying to link same item with other libraries to come. */
        BLI_bitmap_set_all(item->libraries, false, lapp_context->num_libraries);
        item->new_id = new_id;
        item->source_library = new_id->lib;
      }
    }

    BLO_library_link_end(mainl, &bh, &liblink_params);
    BLO_blendhandle_close(bh);
  }
}

/** \} */

/** \name Library relocating code.
 * \{ */

static void blendfile_library_relocate_remap(Main *bmain,
                                             ID *old_id,
                                             ID *new_id,
                                             ReportList *reports,
                                             const bool do_reload,
                                             const short remap_flags)
{
  BLI_assert(old_id);
  if (do_reload) {
    /* Since we asked for placeholders in case of missing IDs,
     * we expect to always get a valid one. */
    BLI_assert(new_id);
  }
  if (new_id) {
    CLOG_INFO(&LOG,
              4,
              "Before remap of %s, old_id users: %d, new_id users: %d",
              old_id->name,
              old_id->us,
              new_id->us);
    BKE_libblock_remap_locked(bmain, old_id, new_id, remap_flags);

    if (old_id->flag & LIB_FAKEUSER) {
      id_fake_user_clear(old_id);
      id_fake_user_set(new_id);
    }

    CLOG_INFO(&LOG,
              4,
              "After remap of %s, old_id users: %d, new_id users: %d",
              old_id->name,
              old_id->us,
              new_id->us);

    /* In some cases, new_id might become direct link, remove parent of library in this case. */
    if (new_id->lib->parent && (new_id->tag & LIB_TAG_INDIRECT) == 0) {
      if (do_reload) {
        BLI_assert_unreachable(); /* Should not happen in 'pure' reload case... */
      }
      new_id->lib->parent = NULL;
    }
  }

  if (old_id->us > 0 && new_id && old_id->lib == new_id->lib) {
    /* Note that this *should* not happen - but better be safe than sorry in this area,
     * at least until we are 100% sure this cannot ever happen.
     * Also, we can safely assume names were unique so far,
     * so just replacing '.' by '~' should work,
     * but this does not totally rules out the possibility of name collision. */
    size_t len = strlen(old_id->name);
    size_t dot_pos;
    bool has_num = false;

    for (dot_pos = len; dot_pos--;) {
      char c = old_id->name[dot_pos];
      if (c == '.') {
        break;
      }
      if (c < '0' || c > '9') {
        has_num = false;
        break;
      }
      has_num = true;
    }

    if (has_num) {
      old_id->name[dot_pos] = '~';
    }
    else {
      len = MIN2(len, MAX_ID_NAME - 7);
      BLI_strncpy(&old_id->name[len], "~000", 7);
    }

    id_sort_by_name(which_libbase(bmain, GS(old_id->name)), old_id, NULL);

    BKE_reportf(
        reports,
        RPT_WARNING,
        "Lib Reload: Replacing all references to old data-block '%s' by reloaded one failed, "
        "old one (%d remaining users) had to be kept and was renamed to '%s'",
        new_id->name,
        old_id->us,
        old_id->name);
  }
}

void BKE_blendfile_library_relocate(BlendfileLinkAppendContext *lapp_context,
                                    ReportList *reports,
                                    Library *library,
                                    const bool do_reload,
                                    Main *bmain,
                                    Scene *scene,
                                    ViewLayer *view_layer)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int lba_idx;

  LinkNode *itemlink;
  int item_idx;

  /* Remove all IDs to be reloaded from Main. */
  lba_idx = set_listbasepointers(bmain, lbarray);
  while (lba_idx--) {
    ID *id = lbarray[lba_idx]->first;
    const short idcode = id ? GS(id->name) : 0;

    if (!id || !BKE_idtype_idcode_is_linkable(idcode)) {
      /* No need to reload non-linkable datatypes,
       * those will get relinked with their 'users ID'. */
      continue;
    }

    for (; id; id = id->next) {
      if (id->lib == library) {
        BlendfileLinkAppendContextItem *item;

        /* We remove it from current Main, and add it to items to link... */
        /* Note that non-linkable IDs (like e.g. shapekeys) are also explicitly linked here... */
        BLI_remlink(lbarray[lba_idx], id);
        /* Usual special code for ShapeKeys snowflakes... */
        Key *old_key = BKE_key_from_id(id);
        if (old_key != NULL) {
          BLI_remlink(which_libbase(bmain, GS(old_key->id.name)), &old_key->id);
        }

        item = BKE_blendfile_link_append_context_item_add(lapp_context, id->name + 2, idcode, id);
        BLI_bitmap_set_all(item->libraries, true, (size_t)lapp_context->num_libraries);

        CLOG_INFO(&LOG, 4, "Datablock to seek for: %s", id->name);
      }
    }
  }

  if (lapp_context->num_items == 0) {
    /* Early out in case there is nothing to do. */
    return;
  }

  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* We do not want any instantiation here! */
  BKE_blendfile_link(lapp_context, reports, bmain, NULL, NULL, NULL);

  BKE_main_lock(bmain);

  /* We add back old id to bmain.
   * We need to do this in a first, separated loop, otherwise some of those may not be handled by
   * ID remapping, which means they would still reference old data to be deleted... */
  for (item_idx = 0, itemlink = lapp_context->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *old_id = item->userdata;

    BLI_assert(old_id);
    BLI_addtail(which_libbase(bmain, GS(old_id->name)), old_id);

    /* Usual special code for ShapeKeys snowflakes... */
    Key *old_key = BKE_key_from_id(old_id);
    if (old_key != NULL) {
      BLI_addtail(which_libbase(bmain, GS(old_key->id.name)), &old_key->id);
    }
  }

  /* Since our (old) reloaded IDs were removed from main, the user count done for them in linking
   * code is wrong, we need to redo it here after adding them back to main. */
  BKE_main_id_refcount_recompute(bmain, false);

  /* Note that in reload case, we also want to replace indirect usages. */
  const short remap_flags = ID_REMAP_SKIP_NEVER_NULL_USAGE |
                            ID_REMAP_NO_INDIRECT_PROXY_DATA_USAGE |
                            (do_reload ? 0 : ID_REMAP_SKIP_INDIRECT_USAGE);
  for (item_idx = 0, itemlink = lapp_context->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *old_id = item->userdata;
    ID *new_id = item->new_id;

    blendfile_library_relocate_remap(bmain, old_id, new_id, reports, do_reload, remap_flags);
    if (new_id == NULL) {
      continue;
    }
    /* Usual special code for ShapeKeys snowflakes... */
    Key **old_key_p = BKE_key_from_id_p(old_id);
    if (old_key_p == NULL) {
      continue;
    }
    Key *old_key = *old_key_p;
    Key *new_key = BKE_key_from_id(new_id);
    if (old_key != NULL) {
      *old_key_p = NULL;
      id_us_min(&old_key->id);
      blendfile_library_relocate_remap(
          bmain, &old_key->id, &new_key->id, reports, do_reload, remap_flags);
      *old_key_p = old_key;
      id_us_plus_no_lib(&old_key->id);
    }
  }

  BKE_main_unlock(bmain);

  for (item_idx = 0, itemlink = lapp_context->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    BlendfileLinkAppendContextItem *item = itemlink->link;
    ID *old_id = item->userdata;

    if (old_id->us == 0) {
      BKE_id_free(bmain, old_id);
    }
  }

  /* Some datablocks can get reloaded/replaced 'silently' because they are not linkable
   * (shape keys e.g.), so we need another loop here to clear old ones if possible. */
  lba_idx = set_listbasepointers(bmain, lbarray);
  while (lba_idx--) {
    ID *id, *id_next;
    for (id = lbarray[lba_idx]->first; id; id = id_next) {
      id_next = id->next;
      /* XXX That check may be a bit to generic/permissive? */
      if (id->lib && (id->flag & LIB_TAG_PRE_EXISTING) && id->us == 0) {
        BKE_id_free(bmain, id);
      }
    }
  }

  /* Get rid of no more used libraries... */
  BKE_main_id_tag_idcode(bmain, ID_LI, LIB_TAG_DOIT, true);
  lba_idx = set_listbasepointers(bmain, lbarray);
  while (lba_idx--) {
    ID *id;
    for (id = lbarray[lba_idx]->first; id; id = id->next) {
      if (id->lib) {
        id->lib->id.tag &= ~LIB_TAG_DOIT;
      }
    }
  }
  Library *lib, *lib_next;
  for (lib = which_libbase(bmain, ID_LI)->first; lib; lib = lib_next) {
    lib_next = lib->id.next;
    if (lib->id.tag & LIB_TAG_DOIT) {
      id_us_clear_real(&lib->id);
      if (lib->id.us == 0) {
        BKE_id_free(bmain, (ID *)lib);
      }
    }
  }

  /* Update overrides of reloaded linked data-blocks. */
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_LINKED(id) || !ID_IS_OVERRIDE_LIBRARY_REAL(id) ||
        (id->tag & LIB_TAG_PRE_EXISTING) == 0) {
      continue;
    }
    if ((id->override_library->reference->tag & LIB_TAG_PRE_EXISTING) == 0) {
      BKE_lib_override_library_update(bmain, id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Resync overrides if needed. */
  if (!USER_EXPERIMENTAL_TEST(&U, no_override_auto_resync)) {
    BKE_lib_override_library_main_resync(bmain,
                                         scene,
                                         view_layer,
                                         &(struct BlendFileReadReport){
                                             .reports = reports,
                                         });
    /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
    BKE_lib_override_library_main_operations_create(bmain, true);
  }

  BKE_main_collection_sync(bmain);
}

/** \} */
