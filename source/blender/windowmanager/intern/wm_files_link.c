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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Functions for dealing with append/link operators and helpers.
 */

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#include "BLO_readfile.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_global.h"
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

#include "BKE_idtype.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "IMB_colormanagement.h"

#include "ED_datafiles.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_files.h"

static CLG_LogRef LOG = {"wm.files_link"};

/* -------------------------------------------------------------------- */
/** \name Link/Append Operator
 * \{ */

static bool wm_link_append_poll(bContext *C)
{
  if (WM_operator_winactive(C)) {
    /* linking changes active object which is pretty useful in general,
     * but which totally confuses edit mode (i.e. it becoming not so obvious
     * to leave from edit mode and invalid tools in toolbar might be displayed)
     * so disable link/append when in edit mode (sergey) */
    if (CTX_data_edit_object(C)) {
      return 0;
    }

    return 1;
  }

  return 0;
}

static int wm_link_append_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    if (G.lib[0] != '\0') {
      RNA_string_set(op->ptr, "filepath", G.lib);
    }
    else if (G.relbase_valid) {
      char path[FILE_MAX];
      BLI_strncpy(path, BKE_main_blendfile_path_from_global(), sizeof(path));
      BLI_path_parent_dir(path);
      RNA_string_set(op->ptr, "filepath", path);
    }
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int wm_link_append_flag(wmOperator *op)
{
  PropertyRNA *prop;
  int flag = 0;

  if (RNA_boolean_get(op->ptr, "autoselect")) {
    flag |= FILE_AUTOSELECT;
  }
  if (RNA_boolean_get(op->ptr, "active_collection")) {
    flag |= FILE_ACTIVE_COLLECTION;
  }
  if ((prop = RNA_struct_find_property(op->ptr, "relative_path")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    flag |= FILE_RELPATH;
  }
  if (RNA_boolean_get(op->ptr, "link")) {
    flag |= FILE_LINK;
  }
  else {
    if (RNA_boolean_get(op->ptr, "use_recursive")) {
      flag |= BLO_LIBLINK_APPEND_RECURSIVE;
    }
    if (RNA_boolean_get(op->ptr, "set_fake")) {
      flag |= BLO_LIBLINK_APPEND_SET_FAKEUSER;
    }
    if (RNA_boolean_get(op->ptr, "do_reuse_local_id")) {
      flag |= BLO_LIBLINK_APPEND_LOCAL_ID_REUSE;
    }
  }
  if (RNA_boolean_get(op->ptr, "instance_collections")) {
    flag |= BLO_LIBLINK_COLLECTION_INSTANCE;
  }
  if (RNA_boolean_get(op->ptr, "instance_object_data")) {
    flag |= BLO_LIBLINK_OBDATA_INSTANCE;
  }

  return flag;
}

typedef struct WMLinkAppendDataItem {
  char *name;
  BLI_bitmap
      *libraries; /* All libs (from WMLinkAppendData.libraries) to try to load this ID from. */
  short idcode;

  /** Type of action to do to append this item, and other append-specific information. */
  char append_action;
  char append_tag;

  ID *new_id;
  Library *source_library;
  void *customdata;
} WMLinkAppendDataItem;

typedef struct WMLinkAppendData {
  LinkNodePair libraries;
  LinkNodePair items;
  int num_libraries;
  int num_items;
  /**
   * Combines #eFileSel_Params_Flag from DNA_space_types.h & #eBLOLibLinkFlags from BLO_readfile.h
   */
  int flag;

  /** Allows to easily find an existing items from an ID pointer. Used by append code. */
  GHash *new_id_to_item;

  /** Runtime info used by append code to manage re-use of already appended matching IDs. */
  GHash *library_weak_reference_mapping;

  /* Internal 'private' data */
  MemArena *memarena;
} WMLinkAppendData;

typedef struct WMLinkAppendDataCallBack {
  WMLinkAppendData *lapp_data;
  WMLinkAppendDataItem *item;
  ReportList *reports;

} WMLinkAppendDataCallBack;

enum {
  WM_APPEND_ACT_UNSET = 0,
  WM_APPEND_ACT_KEEP_LINKED,
  WM_APPEND_ACT_REUSE_LOCAL,
  WM_APPEND_ACT_MAKE_LOCAL,
  WM_APPEND_ACT_COPY_LOCAL,
};

enum {
  WM_APPEND_TAG_INDIRECT = 1 << 0,
};

static WMLinkAppendData *wm_link_append_data_new(const int flag)
{
  MemArena *ma = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  WMLinkAppendData *lapp_data = BLI_memarena_calloc(ma, sizeof(*lapp_data));

  lapp_data->flag = flag;
  lapp_data->memarena = ma;

  return lapp_data;
}

static void wm_link_append_data_free(WMLinkAppendData *lapp_data)
{
  if (lapp_data->new_id_to_item != NULL) {
    BLI_ghash_free(lapp_data->new_id_to_item, NULL, NULL);
  }

  BLI_assert(lapp_data->library_weak_reference_mapping == NULL);

  BLI_memarena_free(lapp_data->memarena);
}

/* WARNING! *Never* call wm_link_append_data_library_add() after having added some items! */

static void wm_link_append_data_library_add(WMLinkAppendData *lapp_data, const char *libname)
{
  size_t len = strlen(libname) + 1;
  char *libpath = BLI_memarena_alloc(lapp_data->memarena, len);

  BLI_strncpy(libpath, libname, len);
  BLI_linklist_append_arena(&lapp_data->libraries, libpath, lapp_data->memarena);
  lapp_data->num_libraries++;
}

static WMLinkAppendDataItem *wm_link_append_data_item_add(WMLinkAppendData *lapp_data,
                                                          const char *idname,
                                                          const short idcode,
                                                          void *customdata)
{
  WMLinkAppendDataItem *item = BLI_memarena_alloc(lapp_data->memarena, sizeof(*item));
  size_t len = strlen(idname) + 1;

  item->name = BLI_memarena_alloc(lapp_data->memarena, len);
  BLI_strncpy(item->name, idname, len);
  item->idcode = idcode;
  item->libraries = BLI_BITMAP_NEW_MEMARENA(lapp_data->memarena, lapp_data->num_libraries);

  item->new_id = NULL;
  item->append_action = WM_APPEND_ACT_UNSET;
  item->customdata = customdata;

  BLI_linklist_append_arena(&lapp_data->items, item, lapp_data->memarena);
  lapp_data->num_items++;

  return item;
}

/* -------------------------------------------------------------------- */
/** \name Library appending helper functions.
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

/**
 * Shared operations to perform on the object's base after adding it to the scene.
 */
static void wm_append_loose_data_instantiate_object_base_instance_init(
    Object *ob, bool set_selected, bool set_active, ViewLayer *view_layer, const View3D *v3d)
{
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (v3d != NULL) {
    base->local_view_bits |= v3d->local_view_uuid;
  }

  if (set_selected) {
    if (base->flag & BASE_SELECTABLE) {
      base->flag |= BASE_SELECTED;
    }
  }

  if (set_active) {
    view_layer->basact = base;
  }

  BKE_scene_object_base_flag_sync_from_base(base);
}

static ID *wm_append_loose_data_instantiate_process_check(WMLinkAppendDataItem *item)
{
  /* We consider that if we either kept it linked, or re-used already local data, instantiation
   * status of those should not be modified. */
  if (!ELEM(item->append_action, WM_APPEND_ACT_COPY_LOCAL, WM_APPEND_ACT_MAKE_LOCAL)) {
    return NULL;
  }

  ID *id = item->new_id;
  if (id == NULL) {
    return NULL;
  }

  if (item->append_action == WM_APPEND_ACT_COPY_LOCAL) {
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

static void wm_append_loose_data_instantiate_ensure_active_collection(
    WMLinkAppendData *lapp_data,
    Main *bmain,
    Scene *scene,
    ViewLayer *view_layer,
    Collection **r_active_collection)
{
  /* Find or add collection as needed. */
  if (*r_active_collection == NULL) {
    if (lapp_data->flag & FILE_ACTIVE_COLLECTION) {
      LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
      *r_active_collection = lc->collection;
    }
    else {
      *r_active_collection = BKE_collection_add(bmain, scene->master_collection, NULL);
    }
  }
}

/* TODO: De-duplicate this code with the one in readfile.c, think we need some utils code for that
 * in BKE. */
static void wm_append_loose_data_instantiate(WMLinkAppendData *lapp_data,
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
  const bool do_obdata = (lapp_data->flag & BLO_LIBLINK_OBDATA_INSTANCE) != 0;

  const bool object_set_selected = (lapp_data->flag & FILE_AUTOSELECT) != 0;
  /* Do NOT make base active here! screws up GUI stuff,
   * if you want it do it at the editor level. */
  const bool object_set_active = false;

  /* First pass on obdata to enable their instantiation by default, then do a second pass on
   * objects to clear it for any obdata already in use. */
  if (do_obdata) {
    for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
      WMLinkAppendDataItem *item = itemlink->link;
      ID *id = wm_append_loose_data_instantiate_process_check(item);
      if (id == NULL) {
        continue;
      }
      const ID_Type idcode = GS(id->name);
      if (!OB_DATA_SUPPORT_ID(idcode)) {
        continue;
      }

      id->tag |= LIB_TAG_DOIT;
    }
    for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
      WMLinkAppendDataItem *item = itemlink->link;
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
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = wm_append_loose_data_instantiate_process_check(item);
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
    bool do_add_collection = (item->append_tag & WM_APPEND_TAG_INDIRECT) == 0;
    LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
      Object *ob = coll_ob->ob;
      if (!object_in_any_scene(bmain, ob)) {
        do_add_collection = true;
        break;
      }
    }
    if (do_add_collection) {
      wm_append_loose_data_instantiate_ensure_active_collection(
          lapp_data, bmain, scene, view_layer, &active_collection);

      /* In case user requested instantiation of collections as empties, we do so for the one they
       * explicitly selected (originally directly linked IDs). */
      if ((lapp_data->flag & BLO_LIBLINK_COLLECTION_INSTANCE) != 0 &&
          (item->append_tag & WM_APPEND_TAG_INDIRECT) == 0) {
        /* BKE_object_add(...) messes with the selection. */
        Object *ob = BKE_object_add_only_object(bmain, OB_EMPTY, collection->id.name + 2);
        ob->type = OB_EMPTY;
        ob->empty_drawsize = U.collection_instance_empty_size;

        BKE_collection_object_add(bmain, active_collection, ob);

        const bool set_selected = (lapp_data->flag & FILE_AUTOSELECT) != 0;
        /* TODO: why is it OK to make this active here but not in other situations?
         * See other callers of #object_base_instance_init */
        const bool set_active = set_selected;
        wm_append_loose_data_instantiate_object_base_instance_init(
            ob, set_selected, set_active, view_layer, v3d);

        /* Assign the collection. */
        ob->instance_collection = collection;
        id_us_plus(&collection->id);
        ob->transflag |= OB_DUPLICOLLECTION;
        copy_v3_v3(ob->loc, scene->cursor.location);
      }
      else {
        /* Add collection as child of active collection. */
        BKE_collection_child_add(bmain, active_collection, collection);

        if ((lapp_data->flag & FILE_AUTOSELECT) != 0) {
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
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = wm_append_loose_data_instantiate_process_check(item);
    if (id == NULL || GS(id->name) != ID_OB) {
      continue;
    }

    Object *ob = (Object *)id;

    if (object_in_any_collection(bmain, ob)) {
      continue;
    }

    wm_append_loose_data_instantiate_ensure_active_collection(
        lapp_data, bmain, scene, view_layer, &active_collection);

    CLAMP_MIN(ob->id.us, 0);
    ob->mode = OB_MODE_OBJECT;

    BKE_collection_object_add(bmain, active_collection, ob);

    wm_append_loose_data_instantiate_object_base_instance_init(
        ob, object_set_selected, object_set_active, view_layer, v3d);
  }

  if (!do_obdata) {
    return;
  }

  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = wm_append_loose_data_instantiate_process_check(item);
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

    wm_append_loose_data_instantiate_ensure_active_collection(
        lapp_data, bmain, scene, view_layer, &active_collection);

    const int type = BKE_object_obdata_to_type(id);
    BLI_assert(type != -1);
    Object *ob = BKE_object_add_only_object(bmain, type, id->name + 2);
    ob->data = id;
    id_us_plus(id);
    BKE_object_materials_test(bmain, ob, ob->data);

    BKE_collection_object_add(bmain, active_collection, ob);

    wm_append_loose_data_instantiate_object_base_instance_init(
        ob, object_set_selected, object_set_active, view_layer, v3d);

    copy_v3_v3(ob->loc, scene->cursor.location);

    id->tag &= ~LIB_TAG_DOIT;
  }

  /* Finally, add rigid body objects and constraints to current RB world(s). */
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = wm_append_loose_data_instantiate_process_check(item);
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

  WMLinkAppendDataCallBack *data = cb_data->user_data;
  ID *id = *cb_data->id_pointer;

  if (id == NULL) {
    return IDWALK_RET_NOP;
  }

  if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
    /* While we do not want to add non-linkable ID (shape keys...) to the list of linked items,
     * unfortunately they can use fully linkable valid IDs too, like actions. Those need to be
     * processed, so we need to recursively deal with them here. */
    BKE_library_foreach_ID_link(
        cb_data->bmain, id, foreach_libblock_append_callback, data, IDWALK_NOP);
    return IDWALK_RET_NOP;
  }

  const bool do_recursive = (data->lapp_data->flag & BLO_LIBLINK_APPEND_RECURSIVE) != 0;
  if (!do_recursive && cb_data->id_owner->lib != id->lib) {
    /* When `do_recursive` is false, we only make local IDs from same library(-ies) as the
     * initially directly linked ones. */
    return IDWALK_RET_NOP;
  }

  WMLinkAppendDataItem *item = BLI_ghash_lookup(data->lapp_data->new_id_to_item, id);
  if (item == NULL) {
    item = wm_link_append_data_item_add(data->lapp_data, id->name, GS(id->name), NULL);
    item->new_id = id;
    item->source_library = id->lib;
    /* Since we did not have an item for that ID yet, we know user did not selected it explicitly,
     * it was rather linked indirectly. This info is important for instantiation of collections. */
    item->append_tag |= WM_APPEND_TAG_INDIRECT;
    BLI_ghash_insert(data->lapp_data->new_id_to_item, id, item);
  }

  /* NOTE: currently there is no need to do anything else here, but in the future this would be
   * the place to add specific per-usage decisions on how to append an ID. */

  return IDWALK_RET_NOP;
}

/* Perform append operation, using modern ID usage looper to detect which ID should be kept linked,
 * made local, duplicated as local, re-used from local etc.
 *
 * TODO: Expose somehow this logic to the two other parts of code performing actual append
 * (i.e. copy/paste and `bpy` link/append API).
 * Then we can heavily simplify #BKE_library_make_local(). */
static void wm_append_do(WMLinkAppendData *lapp_data,
                         ReportList *reports,
                         Main *bmain,
                         Scene *scene,
                         ViewLayer *view_layer,
                         const View3D *v3d)
{
  BLI_assert((lapp_data->flag & FILE_LINK) == 0);

  const bool set_fakeuser = (lapp_data->flag & BLO_LIBLINK_APPEND_SET_FAKEUSER) != 0;
  const bool do_reuse_local_id = (lapp_data->flag & BLO_LIBLINK_APPEND_LOCAL_ID_REUSE) != 0;

  LinkNode *itemlink;

  /* Generate a mapping between newly linked IDs and their items, and tag linked IDs used as
   * liboverride references as already existing. */
  lapp_data->new_id_to_item = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_ghash_insert(lapp_data->new_id_to_item, id, item);

    /* This ensures that if a liboverride reference is also linked/used by some other appended
     * data, it gets a local copy instead of being made directly local, so that the liboverride
     * references remain valid (i.e. linked data). */
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      id->override_library->reference->tag |= LIB_TAG_PRE_EXISTING;
    }
  }

  lapp_data->library_weak_reference_mapping = BKE_main_library_weak_reference_create(bmain);

  /* NOTE: Since we append items for IDs not already listed (i.e. implicitly linked indirect
   * dependencies), this list will grow and we will process those IDs later, leading to a flatten
   * recursive processing of all the linked dependencies. */
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    BLI_assert(item->customdata == NULL);

    /* In Append case linked IDs should never be marked as needing post-processing (instantiation
     * of loose objects etc.). */
    BLI_assert((id->tag & LIB_TAG_DOIT) == 0);

    ID *existing_local_id = BKE_idtype_idcode_append_is_reusable(GS(id->name)) ?
                                BKE_main_library_weak_reference_search_item(
                                    lapp_data->library_weak_reference_mapping,
                                    id->lib->filepath,
                                    id->name) :
                                NULL;

    if (item->append_action != WM_APPEND_ACT_UNSET) {
      /* Already set, pass. */
    }
    if (GS(id->name) == ID_OB && ((Object *)id)->proxy_from != NULL) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' is proxified, keeping it linked...", id->name);
      item->append_action = WM_APPEND_ACT_KEEP_LINKED;
    }
    else if (do_reuse_local_id && existing_local_id != NULL) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' as a matching local one, re-using it...", id->name);
      item->append_action = WM_APPEND_ACT_REUSE_LOCAL;
      item->customdata = existing_local_id;
    }
    else if (id->tag & LIB_TAG_PRE_EXISTING) {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' was already linked, need to copy it...", id->name);
      item->append_action = WM_APPEND_ACT_COPY_LOCAL;
    }
    else {
      CLOG_INFO(&LOG, 3, "Appended ID '%s' will be made local...", id->name);
      item->append_action = WM_APPEND_ACT_MAKE_LOCAL;
    }

    /* Only check dependencies if we are not keeping linked data, nor re-using existing local data.
     */
    if (!ELEM(item->append_action, WM_APPEND_ACT_KEEP_LINKED, WM_APPEND_ACT_REUSE_LOCAL)) {
      WMLinkAppendDataCallBack cb_data = {
          .lapp_data = lapp_data, .item = item, .reports = reports};
      BKE_library_foreach_ID_link(
          bmain, id, foreach_libblock_append_callback, &cb_data, IDWALK_NOP);
    }

    /* If we found a matching existing local id but are not re-using it, we need to properly clear
     * its weak reference to linked data. */
    if (existing_local_id != NULL &&
        !ELEM(item->append_action, WM_APPEND_ACT_KEEP_LINKED, WM_APPEND_ACT_REUSE_LOCAL)) {
      BKE_main_library_weak_reference_remove_item(lapp_data->library_weak_reference_mapping,
                                                  id->lib->filepath,
                                                  id->name,
                                                  existing_local_id);
    }
  }

  /* Effectively perform required operation on every linked ID. */
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }

    ID *local_appended_new_id = NULL;
    char lib_filepath[FILE_MAX];
    BLI_strncpy(lib_filepath, id->lib->filepath, sizeof(lib_filepath));
    char lib_id_name[MAX_ID_NAME];
    BLI_strncpy(lib_id_name, id->name, sizeof(lib_id_name));

    switch (item->append_action) {
      case WM_APPEND_ACT_COPY_LOCAL: {
        BKE_lib_id_make_local(
            bmain, id, LIB_ID_MAKELOCAL_FULL_LIBRARY | LIB_ID_MAKELOCAL_FORCE_COPY);
        local_appended_new_id = id->newid;
        break;
      }
      case WM_APPEND_ACT_MAKE_LOCAL:
        BKE_lib_id_make_local(bmain,
                              id,
                              LIB_ID_MAKELOCAL_FULL_LIBRARY | LIB_ID_MAKELOCAL_FORCE_LOCAL |
                                  LIB_ID_MAKELOCAL_OBJECT_NO_PROXY_CLEARING);
        BLI_assert(id->newid == NULL);
        local_appended_new_id = id;
        break;
      case WM_APPEND_ACT_KEEP_LINKED:
        /* Nothing to do here. */
        break;
      case WM_APPEND_ACT_REUSE_LOCAL:
        /* We only need to set `newid` to ID found in previous loop, for proper remapping. */
        ID_NEW_SET(id, item->customdata);
        /* This is not a 'new' local appended id, do not set `local_appended_new_id` here. */
        break;
      case WM_APPEND_ACT_UNSET:
        CLOG_ERROR(
            &LOG, "Unexpected unset append action for '%s' ID, assuming 'keep link'", id->name);
        break;
      default:
        BLI_assert(0);
    }

    if (local_appended_new_id != NULL) {
      if (BKE_idtype_idcode_append_is_reusable(GS(local_appended_new_id->name))) {
        BKE_main_library_weak_reference_add_item(lapp_data->library_weak_reference_mapping,
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

  BKE_main_library_weak_reference_destroy(lapp_data->library_weak_reference_mapping);
  lapp_data->library_weak_reference_mapping = NULL;

  /* Remap IDs as needed. */
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;

    if (item->append_action == WM_APPEND_ACT_KEEP_LINKED) {
      continue;
    }

    ID *id = item->new_id;
    if (id == NULL) {
      continue;
    }
    if (ELEM(item->append_action, WM_APPEND_ACT_COPY_LOCAL, WM_APPEND_ACT_REUSE_LOCAL)) {
      BLI_assert(ID_IS_LINKED(id));
      id = id->newid;
      if (id == NULL) {
        continue;
      }
    }

    BLI_assert(!ID_IS_LINKED(id));

    BKE_libblock_relink_to_newid_new(bmain, id);
  }

  /* Remove linked IDs when a local existing data has been reused instead. */
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;

    if (item->append_action != WM_APPEND_ACT_REUSE_LOCAL) {
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
  wm_append_loose_data_instantiate(lapp_data, bmain, scene, view_layer, v3d);

  /* Attempt to deal with object proxies.
   *
   * NOTE: Copied from `BKE_library_make_local`, but this is not really working (as in, not
   * producing any useful result in any known use case), neither here nor in
   * `BKE_library_make_local` currently.
   * Proxies are end of life anyway, so not worth spending time on this. */
  for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;

    if (item->append_action != WM_APPEND_ACT_COPY_LOCAL) {
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

static void wm_link_do(WMLinkAppendData *lapp_data,
                       ReportList *reports,
                       Main *bmain,
                       Scene *scene,
                       ViewLayer *view_layer,
                       const View3D *v3d)
{
  Main *mainl;
  BlendHandle *bh;
  Library *lib;

  const int flag = lapp_data->flag;
  const int id_tag_extra = 0;

  LinkNode *liblink, *itemlink;
  int lib_idx, item_idx;

  BLI_assert(lapp_data->num_items && lapp_data->num_libraries);

  for (lib_idx = 0, liblink = lapp_data->libraries.list; liblink;
       lib_idx++, liblink = liblink->next) {
    char *libname = liblink->link;
    BlendFileReadReport bf_reports = {.reports = reports};

    if (STREQ(libname, BLO_EMBEDDED_STARTUP_BLEND)) {
      bh = BLO_blendhandle_from_memory(
          datatoc_startup_blend, datatoc_startup_blend_size, &bf_reports);
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
     * (see #wm_append_loose_data_instantiate ). */
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
    for (item_idx = 0, itemlink = lapp_data->items.list; itemlink;
         item_idx++, itemlink = itemlink->next) {
      WMLinkAppendDataItem *item = itemlink->link;
      ID *new_id;

      if (!BLI_BITMAP_TEST(item->libraries, lib_idx)) {
        continue;
      }

      new_id = BLO_library_link_named_part(mainl, &bh, item->idcode, item->name, &liblink_params);

      if (new_id) {
        /* If the link is successful, clear item's libs 'todo' flags.
         * This avoids trying to link same item with other libraries to come. */
        BLI_bitmap_set_all(item->libraries, false, lapp_data->num_libraries);
        item->new_id = new_id;
        item->source_library = new_id->lib;
      }
    }

    BLO_library_link_end(mainl, &bh, &liblink_params);
    BLO_blendhandle_close(bh);
  }
}

/**
 * Check if an item defined by \a name and \a group can be appended/linked.
 *
 * \param reports: Optionally report an error when an item can't be appended/linked.
 */
static bool wm_link_append_item_poll(ReportList *reports,
                                     const char *path,
                                     const char *group,
                                     const char *name,
                                     const bool do_append)
{
  short idcode;

  if (!group || !name) {
    CLOG_WARN(&LOG, "Skipping %s", path);
    return false;
  }

  idcode = BKE_idtype_idcode_from_name(group);

  if (!BKE_idtype_idcode_is_linkable(idcode) ||
      (!do_append && BKE_idtype_idcode_is_only_appendable(idcode))) {
    if (reports) {
      if (do_append) {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Can't append data-block '%s' of type '%s'",
                    name,
                    group);
      }
      else {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Can't link data-block '%s' of type '%s'",
                    name,
                    group);
      }
    }
    return false;
  }

  return true;
}

static int wm_link_append_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PropertyRNA *prop;
  WMLinkAppendData *lapp_data;
  char path[FILE_MAX_LIBEXTRA], root[FILE_MAXDIR], libname[FILE_MAX_LIBEXTRA], relname[FILE_MAX];
  char *group, *name;
  int totfiles = 0;

  RNA_string_get(op->ptr, "filename", relname);
  RNA_string_get(op->ptr, "directory", root);

  BLI_join_dirfile(path, sizeof(path), root, relname);

  /* test if we have a valid data */
  if (!BLO_library_path_explode(path, libname, &group, &name)) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': not a library", path);
    return OPERATOR_CANCELLED;
  }
  if (!group) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
    return OPERATOR_CANCELLED;
  }
  if (BLI_path_cmp(BKE_main_blendfile_path(bmain), libname) == 0) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': cannot use current file as library", path);
    return OPERATOR_CANCELLED;
  }

  /* check if something is indicated for append/link */
  prop = RNA_struct_find_property(op->ptr, "files");
  if (prop) {
    totfiles = RNA_property_collection_length(op->ptr, prop);
    if (totfiles == 0) {
      if (!name) {
        BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else if (!name) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
    return OPERATOR_CANCELLED;
  }

  int flag = wm_link_append_flag(op);
  const bool do_append = (flag & FILE_LINK) == 0;

  /* sanity checks for flag */
  if (scene && scene->id.lib) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Scene '%s' is linked, instantiation of objects is disabled",
                scene->id.name + 2);
    flag &= ~(BLO_LIBLINK_COLLECTION_INSTANCE | BLO_LIBLINK_OBDATA_INSTANCE);
    scene = NULL;
  }

  /* from here down, no error returns */

  if (view_layer && RNA_boolean_get(op->ptr, "autoselect")) {
    BKE_view_layer_base_deselect_all(view_layer);
  }

  /* tag everything, all untagged data can be made local
   * its also generally useful to know what is new
   *
   * take extra care BKE_main_id_flag_all(bmain, LIB_TAG_PRE_EXISTING, false) is called after! */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* We define our working data...
   * Note that here, each item 'uses' one library, and only one. */
  lapp_data = wm_link_append_data_new(flag);
  if (totfiles != 0) {
    GHash *libraries = BLI_ghash_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, __func__);
    int lib_idx = 0;

    RNA_BEGIN (op->ptr, itemptr, "files") {
      RNA_string_get(&itemptr, "name", relname);

      BLI_join_dirfile(path, sizeof(path), root, relname);

      if (BLO_library_path_explode(path, libname, &group, &name)) {
        if (!wm_link_append_item_poll(NULL, path, group, name, do_append)) {
          continue;
        }

        if (!BLI_ghash_haskey(libraries, libname)) {
          BLI_ghash_insert(libraries, BLI_strdup(libname), POINTER_FROM_INT(lib_idx));
          lib_idx++;
          wm_link_append_data_library_add(lapp_data, libname);
        }
      }
    }
    RNA_END;

    RNA_BEGIN (op->ptr, itemptr, "files") {
      RNA_string_get(&itemptr, "name", relname);

      BLI_join_dirfile(path, sizeof(path), root, relname);

      if (BLO_library_path_explode(path, libname, &group, &name)) {
        WMLinkAppendDataItem *item;

        if (!wm_link_append_item_poll(op->reports, path, group, name, do_append)) {
          continue;
        }

        lib_idx = POINTER_AS_INT(BLI_ghash_lookup(libraries, libname));

        item = wm_link_append_data_item_add(
            lapp_data, name, BKE_idtype_idcode_from_name(group), NULL);
        BLI_BITMAP_ENABLE(item->libraries, lib_idx);
      }
    }
    RNA_END;

    BLI_ghash_free(libraries, MEM_freeN, NULL);
  }
  else {
    WMLinkAppendDataItem *item;

    wm_link_append_data_library_add(lapp_data, libname);
    item = wm_link_append_data_item_add(lapp_data, name, BKE_idtype_idcode_from_name(group), NULL);
    BLI_BITMAP_ENABLE(item->libraries, 0);
  }

  if (lapp_data->num_items == 0) {
    /* Early out in case there is nothing to link. */
    wm_link_append_data_free(lapp_data);
    /* Clear pre existing tag. */
    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);
    return OPERATOR_CANCELLED;
  }

  /* XXX We'd need re-entrant locking on Main for this to work... */
  // BKE_main_lock(bmain);

  wm_link_do(lapp_data, op->reports, bmain, scene, view_layer, CTX_wm_view3d(C));

  // BKE_main_unlock(bmain);

  /* mark all library linked objects to be updated */
  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* append, rather than linking */
  if (do_append) {
    wm_append_do(lapp_data, op->reports, bmain, scene, view_layer, CTX_wm_view3d(C));
  }

  wm_link_append_data_free(lapp_data);

  /* important we unset, otherwise these object won't
   * link into other scenes from this blend file */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

  /* TODO(sergey): Use proper flag for tagging here. */

  /* TODO(dalai): Temporary solution!
   * Ideally we only need to tag the new objects themselves, not the scene.
   * This way we'll avoid flush of collection properties
   * to all objects and limit update to the particular object only.
   * But afraid first we need to change collection evaluation in DEG
   * according to depsgraph manifesto. */
  DEG_id_tag_update(&scene->id, 0);

  /* recreate dependency graph to include new objects */
  DEG_relations_tag_update(bmain);

  /* XXX TODO: align G.lib with other directory storage (like last opened image etc...) */
  BLI_strncpy(G.lib, root, FILE_MAX);

  WM_event_add_notifier(C, NC_WINDOW, NULL);

  return OPERATOR_FINISHED;
}

static void wm_link_append_properties_common(wmOperatorType *ot, bool is_link)
{
  PropertyRNA *prop;

  /* better not save _any_ settings for this operator */
  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "link", is_link, "Link", "Link the objects or data-blocks rather than appending");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_boolean(
      ot->srna,
      "do_reuse_local_id",
      false,
      "Re-Use Local Data",
      "Try to re-use previously matching appended data-blocks instead of appending a new copy");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_boolean(ot->srna, "autoselect", true, "Select", "Select new objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "active_collection",
                         true,
                         "Active Collection",
                         "Put new objects on the active collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "instance_collections",
      is_link,
      "Instance Collections",
      "Create instances for collections, rather than adding them directly to the scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "instance_object_data",
      true,
      "Instance Object Data",
      "Create instances for object data which are not referenced by any objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_OT_link(wmOperatorType *ot)
{
  ot->name = "Link";
  ot->idname = "WM_OT_link";
  ot->description = "Link from a Library .blend file";

  ot->invoke = wm_link_append_invoke;
  ot->exec = wm_link_append_exec;
  ot->poll = wm_link_append_poll;

  ot->flag |= OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB,
                                 FILE_LOADLIB,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_RELPATH | WM_FILESEL_FILES | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  wm_link_append_properties_common(ot, true);
}

void WM_OT_append(wmOperatorType *ot)
{
  ot->name = "Append";
  ot->idname = "WM_OT_append";
  ot->description = "Append from a Library .blend file";

  ot->invoke = wm_link_append_invoke;
  ot->exec = wm_link_append_exec;
  ot->poll = wm_link_append_poll;

  ot->flag |= OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB,
                                 FILE_LOADLIB,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_FILES | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  wm_link_append_properties_common(ot, false);
  RNA_def_boolean(ot->srna,
                  "set_fake",
                  false,
                  "Fake User",
                  "Set \"Fake User\" for appended items (except objects and collections)");
  RNA_def_boolean(
      ot->srna,
      "use_recursive",
      true,
      "Localize All",
      "Localize all appended data, including those indirectly linked from other libraries");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link/Append Single Data-Block & Return it
 *
 * \{ */

static ID *wm_file_link_append_datablock_ex(Main *bmain,
                                            Scene *scene,
                                            ViewLayer *view_layer,
                                            View3D *v3d,
                                            const char *filepath,
                                            const short id_code,
                                            const char *id_name,
                                            const int flag)
{
  const bool do_append = (flag & FILE_LINK) == 0;
  /* Tag everything so we can make local only the new datablock. */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* Define working data, with just the one item we want to link. */
  WMLinkAppendData *lapp_data = wm_link_append_data_new(flag);

  wm_link_append_data_library_add(lapp_data, filepath);
  WMLinkAppendDataItem *item = wm_link_append_data_item_add(lapp_data, id_name, id_code, NULL);
  BLI_BITMAP_ENABLE(item->libraries, 0);

  /* Link datablock. */
  wm_link_do(lapp_data, NULL, bmain, scene, view_layer, v3d);

  if (do_append) {
    wm_append_do(lapp_data, NULL, bmain, scene, view_layer, v3d);
  }

  /* Get linked datablock and free working data. */
  ID *id = item->new_id;

  wm_link_append_data_free(lapp_data);

  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

  return id;
}

/*
 * NOTE: `scene` (and related `view_layer` and `v3d`) pointers may be NULL, in which case no
 * instantiation of linked objects, collections etc. will be performed.
 */
ID *WM_file_link_datablock(Main *bmain,
                           Scene *scene,
                           ViewLayer *view_layer,
                           View3D *v3d,
                           const char *filepath,
                           const short id_code,
                           const char *id_name,
                           int flag)
{
  flag |= FILE_LINK;
  return wm_file_link_append_datablock_ex(
      bmain, scene, view_layer, v3d, filepath, id_code, id_name, flag);
}

/*
 * NOTE: `scene` (and related `view_layer` and `v3d`) pointers may be NULL, in which case no
 * instantiation of appended objects, collections etc. will be performed.
 */
ID *WM_file_append_datablock(Main *bmain,
                             Scene *scene,
                             ViewLayer *view_layer,
                             View3D *v3d,
                             const char *filepath,
                             const short id_code,
                             const char *id_name,
                             int flag)
{
  BLI_assert((flag & FILE_LINK) == 0);
  ID *id = wm_file_link_append_datablock_ex(
      bmain, scene, view_layer, v3d, filepath, id_code, id_name, flag);

  return id;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Relocate Operator & Library Reload API
 * \{ */

static int wm_lib_relocate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Library *lib;
  char lib_name[MAX_NAME];

  RNA_string_get(op->ptr, "library", lib_name);
  lib = (Library *)BKE_libblock_find_name(CTX_data_main(C), ID_LI, lib_name);

  if (lib) {
    if (lib->parent) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Cannot relocate indirectly linked library '%s'",
                  lib->filepath_abs);
      return OPERATOR_CANCELLED;
    }
    RNA_string_set(op->ptr, "filepath", lib->filepath_abs);

    WM_event_add_fileselect(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_CANCELLED;
}

static void lib_relocate_do_remap(Main *bmain,
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

static void lib_relocate_do(bContext *C,
                            Library *library,
                            WMLinkAppendData *lapp_data,
                            ReportList *reports,
                            const bool do_reload)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int lba_idx;

  LinkNode *itemlink;
  int item_idx;

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

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
        WMLinkAppendDataItem *item;

        /* We remove it from current Main, and add it to items to link... */
        /* Note that non-linkable IDs (like e.g. shapekeys) are also explicitly linked here... */
        BLI_remlink(lbarray[lba_idx], id);
        /* Usual special code for ShapeKeys snowflakes... */
        Key *old_key = BKE_key_from_id(id);
        if (old_key != NULL) {
          BLI_remlink(which_libbase(bmain, GS(old_key->id.name)), &old_key->id);
        }

        item = wm_link_append_data_item_add(lapp_data, id->name + 2, idcode, id);
        BLI_bitmap_set_all(item->libraries, true, lapp_data->num_libraries);

        CLOG_INFO(&LOG, 4, "Datablock to seek for: %s", id->name);
      }
    }
  }

  if (lapp_data->num_items == 0) {
    /* Early out in case there is nothing to do. */
    return;
  }

  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* We do not want any instantiation here! */
  wm_link_do(lapp_data, reports, bmain, NULL, NULL, NULL);

  BKE_main_lock(bmain);

  /* We add back old id to bmain.
   * We need to do this in a first, separated loop, otherwise some of those may not be handled by
   * ID remapping, which means they would still reference old data to be deleted... */
  for (item_idx = 0, itemlink = lapp_data->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *old_id = item->customdata;

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
  for (item_idx = 0, itemlink = lapp_data->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *old_id = item->customdata;
    ID *new_id = item->new_id;

    lib_relocate_do_remap(bmain, old_id, new_id, reports, do_reload, remap_flags);
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
      lib_relocate_do_remap(bmain, &old_key->id, &new_key->id, reports, do_reload, remap_flags);
      *old_key_p = old_key;
      id_us_plus_no_lib(&old_key->id);
    }
  }

  BKE_main_unlock(bmain);

  for (item_idx = 0, itemlink = lapp_data->items.list; itemlink;
       item_idx++, itemlink = itemlink->next) {
    WMLinkAppendDataItem *item = itemlink->link;
    ID *old_id = item->customdata;

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

  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* important we unset, otherwise these object won't
   * link into other scenes from this blend file */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

  /* recreate dependency graph to include new objects */
  DEG_relations_tag_update(bmain);
}

void WM_lib_reload(Library *lib, bContext *C, ReportList *reports)
{
  if (!BLO_has_bfile_extension(lib->filepath_abs)) {
    BKE_reportf(reports, RPT_ERROR, "'%s' is not a valid library filepath", lib->filepath_abs);
    return;
  }

  if (!BLI_exists(lib->filepath_abs)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Trying to reload library '%s' from invalid path '%s'",
                lib->id.name,
                lib->filepath_abs);
    return;
  }

  WMLinkAppendData *lapp_data = wm_link_append_data_new(BLO_LIBLINK_USE_PLACEHOLDERS |
                                                        BLO_LIBLINK_FORCE_INDIRECT);

  wm_link_append_data_library_add(lapp_data, lib->filepath_abs);

  lib_relocate_do(C, lib, lapp_data, reports, true);

  wm_link_append_data_free(lapp_data);

  WM_event_add_notifier(C, NC_WINDOW, NULL);
}

static int wm_lib_relocate_exec_do(bContext *C, wmOperator *op, bool do_reload)
{
  Library *lib;
  char lib_name[MAX_NAME];

  RNA_string_get(op->ptr, "library", lib_name);
  lib = (Library *)BKE_libblock_find_name(CTX_data_main(C), ID_LI, lib_name);

  if (lib) {
    Main *bmain = CTX_data_main(C);
    PropertyRNA *prop;
    WMLinkAppendData *lapp_data;

    char path[FILE_MAX], root[FILE_MAXDIR], libname[FILE_MAX], relname[FILE_MAX];
    short flag = 0;

    if (RNA_boolean_get(op->ptr, "relative_path")) {
      flag |= FILE_RELPATH;
    }

    if (lib->parent && !do_reload) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Cannot relocate indirectly linked library '%s'",
                  lib->filepath_abs);
      return OPERATOR_CANCELLED;
    }

    RNA_string_get(op->ptr, "directory", root);
    RNA_string_get(op->ptr, "filename", libname);

    if (!BLO_has_bfile_extension(libname)) {
      BKE_report(op->reports, RPT_ERROR, "Not a library");
      return OPERATOR_CANCELLED;
    }

    BLI_join_dirfile(path, sizeof(path), root, libname);

    if (!BLI_exists(path)) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Trying to reload or relocate library '%s' to invalid path '%s'",
                  lib->id.name,
                  path);
      return OPERATOR_CANCELLED;
    }

    if (BLI_path_cmp(BKE_main_blendfile_path(bmain), path) == 0) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Cannot relocate library '%s' to current blend file '%s'",
                  lib->id.name,
                  path);
      return OPERATOR_CANCELLED;
    }

    if (BLI_path_cmp(lib->filepath_abs, path) == 0) {
      CLOG_INFO(&LOG, 4, "We are supposed to reload '%s' lib (%d)", lib->filepath, lib->id.us);

      do_reload = true;

      lapp_data = wm_link_append_data_new(flag);
      wm_link_append_data_library_add(lapp_data, path);
    }
    else {
      int totfiles = 0;

      CLOG_INFO(
          &LOG, 4, "We are supposed to relocate '%s' lib to new '%s' one", lib->filepath, libname);

      /* Check if something is indicated for relocate. */
      prop = RNA_struct_find_property(op->ptr, "files");
      if (prop) {
        totfiles = RNA_property_collection_length(op->ptr, prop);
        if (totfiles == 0) {
          if (!libname[0]) {
            BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
            return OPERATOR_CANCELLED;
          }
        }
      }

      lapp_data = wm_link_append_data_new(flag);

      if (totfiles) {
        RNA_BEGIN (op->ptr, itemptr, "files") {
          RNA_string_get(&itemptr, "name", relname);

          BLI_join_dirfile(path, sizeof(path), root, relname);

          if (BLI_path_cmp(path, lib->filepath_abs) == 0 || !BLO_has_bfile_extension(relname)) {
            continue;
          }

          CLOG_INFO(&LOG, 4, "\tCandidate new lib to reload datablocks from: %s", path);
          wm_link_append_data_library_add(lapp_data, path);
        }
        RNA_END;
      }
      else {
        CLOG_INFO(&LOG, 4, "\tCandidate new lib to reload datablocks from: %s", path);
        wm_link_append_data_library_add(lapp_data, path);
      }
    }

    if (do_reload) {
      lapp_data->flag |= BLO_LIBLINK_USE_PLACEHOLDERS | BLO_LIBLINK_FORCE_INDIRECT;
    }

    lib_relocate_do(C, lib, lapp_data, op->reports, do_reload);

    wm_link_append_data_free(lapp_data);

    /* XXX TODO: align G.lib with other directory storage (like last opened image etc...) */
    BLI_strncpy(G.lib, root, FILE_MAX);

    WM_event_add_notifier(C, NC_WINDOW, NULL);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static int wm_lib_relocate_exec(bContext *C, wmOperator *op)
{
  return wm_lib_relocate_exec_do(C, op, false);
}

void WM_OT_lib_relocate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Relocate Library";
  ot->idname = "WM_OT_lib_relocate";
  ot->description = "Relocate the given library to one or several others";

  ot->invoke = wm_lib_relocate_invoke;
  ot->exec = wm_lib_relocate_exec;

  ot->flag |= OPTYPE_UNDO;

  prop = RNA_def_string(ot->srna, "library", NULL, MAX_NAME, "Library", "Library to relocate");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_FILES | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static int wm_lib_reload_exec(bContext *C, wmOperator *op)
{
  return wm_lib_relocate_exec_do(C, op, true);
}

void WM_OT_lib_reload(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Reload Library";
  ot->idname = "WM_OT_lib_reload";
  ot->description = "Reload the given library";

  ot->exec = wm_lib_reload_exec;

  ot->flag |= OPTYPE_UNDO;

  prop = RNA_def_string(ot->srna, "library", NULL, MAX_NAME, "Library", "Library to reload");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */
