/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * High level `.blend` file link/append code,
 * including linking/appending several IDs from different libraries, handling instantiations of
 * collections/objects/object-data in current scene.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_callbacks.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_main_namemap.hh"
#include "BKE_material.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"

#include "BKE_blendfile_link_append.hh"

#include "BLO_writefile.hh"

static CLG_LogRef LOG = {"lib.link_append"};

using namespace blender::bke;

/* -------------------------------------------------------------------- */
/** \name Link/append context implementation and public management API.
 * \{ */

struct BlendfileLinkAppendContextCallBack {
  BlendfileLinkAppendContext *lapp_context;
  BlendfileLinkAppendContextItem *item;
  ReportList *reports;

  /**
   * Whether the currently evaluated usage is within some liboverride dependency context. Note
   * that this include liboverride reference itself, but also e.g. if a linked Mesh is used by the
   * reference of an overridden object.
   *
   * Mutually exclusive with #is_liboverride_dependency_only.
   */
  bool is_liboverride_dependency;
  /**
   * Whether the currently evaluated usage is exclusively within some liboverride dependency
   * context, i.e. the full all usages of this data so far have only been a part of liboverride
   * references and their dependencies.
   *
   * Mutually exclusive with #is_liboverride_dependency.
   */
  bool is_liboverride_dependency_only;
};

static BlendHandle *link_append_context_library_blohandle_ensure(
    BlendfileLinkAppendContext &lapp_context,
    BlendfileLinkAppendContextLibrary &lib_context,
    ReportList *reports)
{
  if (reports != nullptr) {
    lib_context.bf_reports.reports = reports;
  }

  const blender::StringRefNull libname = lib_context.path;
  BlendHandle *blo_handle = lib_context.blo_handle;
  if (blo_handle == nullptr) {
    if (libname == BLO_EMBEDDED_STARTUP_BLEND) {
      blo_handle = BLO_blendhandle_from_memory(lapp_context.blendfile_mem,
                                               int(lapp_context.blendfile_memsize),
                                               &lib_context.bf_reports);
    }
    else {
      blo_handle = BLO_blendhandle_from_file(libname.c_str(), &lib_context.bf_reports);
    }
    lib_context.blo_handle = blo_handle;
    lib_context.blo_handle_is_owned = true;
  }

  return blo_handle;
}

static void link_append_context_library_blohandle_release(
    BlendfileLinkAppendContext & /*lapp_context*/, BlendfileLinkAppendContextLibrary &lib_context)
{
  if (lib_context.blo_handle_is_owned && lib_context.blo_handle != nullptr) {
    BLO_blendhandle_close(lib_context.blo_handle);
    lib_context.blo_handle = nullptr;
  }
}

BlendfileLinkAppendContext *BKE_blendfile_link_append_context_new(LibraryLink_Params *params)
{
  BlendfileLinkAppendContext *lapp_context = MEM_new<BlendfileLinkAppendContext>(__func__);
  lapp_context->params = params;
  lapp_context->process_stage = BlendfileLinkAppendContext::ProcessStage::Init;
  return lapp_context;
}

void BKE_blendfile_link_append_context_free(BlendfileLinkAppendContext *lapp_context)
{
  for (BlendfileLinkAppendContextLibrary &lib_context : lapp_context->libraries) {
    link_append_context_library_blohandle_release(*lapp_context, lib_context);
  }

  BLI_assert(lapp_context->library_weak_reference_mapping == nullptr);

  MEM_delete(lapp_context);
}

void BKE_blendfile_link_append_context_flag_set(BlendfileLinkAppendContext *lapp_context,
                                                const int flag,
                                                const bool do_set)
{
  if (do_set) {
    lapp_context->params->flag |= flag;
  }
  else {
    lapp_context->params->flag &= ~flag;
  }
}

void BKE_blendfile_link_append_context_embedded_blendfile_set(
    BlendfileLinkAppendContext *lapp_context, const void *blendfile_mem, int blendfile_memsize)
{
  BLI_assert_msg(lapp_context->blendfile_mem == nullptr,
                 "Please explicitly clear reference to an embedded blender memfile before "
                 "setting a new one");
  lapp_context->blendfile_mem = blendfile_mem;
  lapp_context->blendfile_memsize = size_t(blendfile_memsize);
}

void BKE_blendfile_link_append_context_embedded_blendfile_clear(
    BlendfileLinkAppendContext *lapp_context)
{
  lapp_context->blendfile_mem = nullptr;
  lapp_context->blendfile_memsize = 0;
}

void BKE_blendfile_link_append_context_library_add(BlendfileLinkAppendContext *lapp_context,
                                                   const char *libname,
                                                   BlendHandle *blo_handle)
{
  BLI_assert(lapp_context->items.empty());
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Init);

  BlendfileLinkAppendContextLibrary lib_context = {};

  lib_context.path = libname;
  lib_context.blo_handle = blo_handle;
  /* Always steal the ownership on the blendfile handle, as it may be freed by readfile code in
   * case of endianness conversion. */
  lib_context.blo_handle_is_owned = true;

  lapp_context->libraries.append(lib_context);
}

BlendfileLinkAppendContextItem *BKE_blendfile_link_append_context_item_add(
    BlendfileLinkAppendContext *lapp_context,
    const char *idname,
    const short idcode,
    void *userdata)
{
  BlendfileLinkAppendContextItem item = {};

  item.lapp_context = lapp_context;

  item.name = idname;
  item.idcode = idcode;
  item.libraries = blender::BitVector<>(lapp_context->libraries.size(), false);

  item.new_id = nullptr;
  item.action = LINK_APPEND_ACT_UNSET;
  item.userdata = userdata;

  lapp_context->items.push_back(item);

  return &lapp_context->items.back();
}

int BKE_blendfile_link_append_context_item_idtypes_from_library_add(
    BlendfileLinkAppendContext *lapp_context,
    ReportList *reports,
    const uint64_t id_types_filter,
    const int library_index)
{
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Init);

  int id_num = 0;
  int id_code_iter = 0;
  short id_code;

  BlendfileLinkAppendContextLibrary &lib_context = lapp_context->libraries[library_index];
  BlendHandle *blo_handle = link_append_context_library_blohandle_ensure(
      *lapp_context, lib_context, reports);

  if (blo_handle == nullptr) {
    return BLENDFILE_LINK_APPEND_INVALID;
  }

  const bool use_assets_only = (lapp_context->params->flag & FILE_ASSETS_ONLY) != 0;

  while ((id_code = BKE_idtype_idcode_iter_step(&id_code_iter))) {
    if (!BKE_idtype_idcode_is_linkable(id_code) ||
        (id_types_filter != 0 && (BKE_idtype_idcode_to_idfilter(id_code) & id_types_filter) == 0))
    {
      continue;
    }

    int id_names_num;
    LinkNode *id_names_list = BLO_blendhandle_get_datablock_names(
        blo_handle, id_code, use_assets_only, &id_names_num);

    for (LinkNode *link_next = nullptr; id_names_list != nullptr; id_names_list = link_next) {
      link_next = id_names_list->next;

      char *id_name = static_cast<char *>(id_names_list->link);
      BlendfileLinkAppendContextItem *item = BKE_blendfile_link_append_context_item_add(
          lapp_context, id_name, id_code, nullptr);
      BKE_blendfile_link_append_context_item_library_index_enable(
          lapp_context, item, library_index);

      MEM_freeN(id_name);
      MEM_freeN(id_names_list);
    }

    id_num += id_names_num;
  }

  return id_num;
}

void BKE_blendfile_link_append_context_item_library_index_enable(
    BlendfileLinkAppendContext *lapp_context,
    BlendfileLinkAppendContextItem *item,
    const int library_index)
{
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Init);
  UNUSED_VARS_NDEBUG(lapp_context);
  item->libraries[library_index].set();
}

bool BKE_blendfile_link_append_context_is_empty(BlendfileLinkAppendContext *lapp_context)
{
  return lapp_context->items.empty();
}

void *BKE_blendfile_link_append_context_item_userdata_get(
    BlendfileLinkAppendContext * /*lapp_context*/, BlendfileLinkAppendContextItem *item)
{
  return item->userdata;
}

ID *BKE_blendfile_link_append_context_item_newid_get(BlendfileLinkAppendContext *lapp_context,
                                                     BlendfileLinkAppendContextItem *item)
{
  BLI_assert(lapp_context->process_stage != BlendfileLinkAppendContext::ProcessStage::Init);
  UNUSED_VARS_NDEBUG(lapp_context);
  return item->new_id;
}

void BKE_blendfile_link_append_context_item_newid_set(BlendfileLinkAppendContext *lapp_context,
                                                      BlendfileLinkAppendContextItem *item,
                                                      ID *new_id)
{
  BLI_assert(lapp_context->process_stage != BlendfileLinkAppendContext::ProcessStage::Init);
  BLI_assert(item->new_id);
  BLI_assert(!item->liboverride_id);
  BLI_assert(new_id->lib == item->new_id->lib);
  BLI_assert(!lapp_context->new_id_to_item.contains(new_id));

  lapp_context->new_id_to_item.remove(item->new_id);
  item->new_id = new_id;
  lapp_context->new_id_to_item.add(new_id, item);
}

ID *BKE_blendfile_link_append_context_item_liboverrideid_get(
    BlendfileLinkAppendContext *lapp_context, BlendfileLinkAppendContextItem *item)
{
  BLI_assert(lapp_context->process_stage != BlendfileLinkAppendContext::ProcessStage::Init);
  UNUSED_VARS_NDEBUG(lapp_context);
  return item->liboverride_id;
}

short BKE_blendfile_link_append_context_item_idcode_get(
    BlendfileLinkAppendContext * /*lapp_context*/, BlendfileLinkAppendContextItem *item)
{
  return item->idcode;
}

void BKE_blendfile_link_append_context_item_foreach(
    BlendfileLinkAppendContext *lapp_context,
    blender::FunctionRef<bool(BlendfileLinkAppendContext *lapp_context,
                              BlendfileLinkAppendContextItem *item)> callback_function,
    const eBlendfileLinkAppendForeachItemFlag flag)
{
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    if ((flag & BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT) == 0 &&
        (item.tag & LINK_APPEND_TAG_INDIRECT) == 0)
    {
      continue;
    }
    if ((flag & BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_INDIRECT) == 0 &&
        (item.tag & LINK_APPEND_TAG_INDIRECT) != 0)
    {
      continue;
    }

    if (!callback_function(lapp_context, &item)) {
      break;
    }
  }
}

void BKE_blendfile_link_append_context_init_done(BlendfileLinkAppendContext *lapp_context)
{
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Init);

  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_BlendImportContext, lapp_context);
  PointerRNA *pointers[1] = {&ctx_ptr};
  BKE_callback_exec(lapp_context->params->bmain, pointers, 1, BKE_CB_EVT_BLENDIMPORT_PRE);
}

void BKE_blendfile_link_append_context_finalize(BlendfileLinkAppendContext *lapp_context)
{
  BLI_assert(ELEM(lapp_context->process_stage,
                  BlendfileLinkAppendContext::ProcessStage::Linking,
                  BlendfileLinkAppendContext::ProcessStage::Appending,
                  BlendfileLinkAppendContext::ProcessStage::Instantiating));
  lapp_context->process_stage = BlendfileLinkAppendContext::ProcessStage::Done;

  BKE_main_ensure_invariants(*lapp_context->params->bmain);

  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_BlendImportContext, lapp_context);
  PointerRNA *pointers[1] = {&ctx_ptr};
  BKE_callback_exec(lapp_context->params->bmain, pointers, 1, BKE_CB_EVT_BLENDIMPORT_POST);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library link/append helper functions.
 *
 * \{ */

/* Struct gathering all required data to handle instantiation of loose data-blocks. */
struct LooseDataInstantiateContext {
  BlendfileLinkAppendContext *lapp_context;

  /* The collection in which to add loose collections/objects. */
  Collection *active_collection;
};

static bool object_in_any_scene(Main *bmain, Object *ob)
{
  LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
    /* #BKE_scene_has_object checks bases cache of the scenes' view-layer, not actual content of
     * their collections. */
    if (BKE_collection_has_object_recursive(sce->master_collection, ob)) {
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
    if (scene->master_collection != nullptr &&
        BKE_collection_has_object(scene->master_collection, ob))
    {
      return true;
    }
  }

  return false;
}

static bool collection_instantiated_by_any_object(Main *bmain, Collection *collection)
{
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type == OB_EMPTY && ob->instance_collection == collection) {
      return true;
    }
  }
  return false;
}

static ID *loose_data_instantiate_process_check(LooseDataInstantiateContext *instantiate_context,
                                                BlendfileLinkAppendContextItem *item)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  /* In linking case, we always want to handle instantiation. */
  if (lapp_context->params->flag & FILE_LINK) {
    return item->new_id;
  }

  /* We consider that if we either kept it linked, or re-used already local data, instantiation
   * status of those should not be modified. */
  if (!ELEM(item->action, LINK_APPEND_ACT_COPY_LOCAL, LINK_APPEND_ACT_MAKE_LOCAL)) {
    return nullptr;
  }

  ID *id = item->new_id;
  if (id == nullptr) {
    return nullptr;
  }

  BLI_assert(!ID_IS_LINKED(id));
  return id;
}

static void loose_data_instantiate_ensure_active_collection(
    LooseDataInstantiateContext *instantiate_context)
{

  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  Main *bmain = instantiate_context->lapp_context->params->bmain;
  Scene *scene = instantiate_context->lapp_context->params->context.scene;
  ViewLayer *view_layer = instantiate_context->lapp_context->params->context.view_layer;

  /* Find or add collection as needed. When `active_collection` is non-null, it is assumed to be
   * editable. */
  if (instantiate_context->active_collection == nullptr) {
    if (lapp_context->params->flag & FILE_ACTIVE_COLLECTION) {
      LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
      instantiate_context->active_collection = BKE_collection_parent_editable_find_recursive(
          view_layer, lc->collection);
    }
    else {
      if (lapp_context->params->flag & FILE_LINK) {
        instantiate_context->active_collection = BKE_collection_add(
            bmain, scene->master_collection, DATA_("Linked Data"));
      }
      else {
        instantiate_context->active_collection = BKE_collection_add(
            bmain, scene->master_collection, DATA_("Appended Data"));
      }
    }
  }
}

static void loose_data_instantiate_object_base_instance_init(Main *bmain,
                                                             Collection *collection,
                                                             Object *ob,
                                                             const Scene *scene,
                                                             ViewLayer *view_layer,
                                                             const View3D *v3d,
                                                             const int flag,
                                                             bool set_active)
{
  /* Auto-select and appending. */
  if ((flag & FILE_AUTOSELECT) && ((flag & FILE_LINK) == 0)) {
    /* While in general the object should not be manipulated,
     * when the user requests the object to be selected, ensure it's visible and selectable. */
    ob->visibility_flag &= ~(OB_HIDE_VIEWPORT | OB_HIDE_SELECT);
  }

  BKE_collection_object_add(bmain, collection, ob);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (v3d != nullptr) {
    base->local_view_bits |= v3d->local_view_uid;
  }

  if (flag & FILE_AUTOSELECT) {
    /* All objects that use #FILE_AUTOSELECT must be selectable (unless linking data). */
    BLI_assert((base->flag & BASE_SELECTABLE) || (flag & FILE_LINK));
    if (base->flag & BASE_SELECTABLE) {
      base->flag |= BASE_SELECTED;
    }
  }

  if (set_active) {
    view_layer->basact = base;
  }

  BKE_scene_object_base_flag_sync_from_base(base);
}

/* Tag obdata that actually need to be instantiated (those referenced by an object do not, since
 * the object will be instantiated instead if needed. */
static void loose_data_instantiate_obdata_preprocess(
    LooseDataInstantiateContext *instantiate_context)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;

  /* First pass on obdata to enable their instantiation by default, then do a second pass on
   * objects to clear it for any obdata already in use. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr) {
      continue;
    }
    const ID_Type idcode = GS(id->name);
    if (!OB_DATA_SUPPORT_ID(idcode)) {
      continue;
    }
    if (idcode == ID_GD_LEGACY) {
      const bGPdata *legacy_gpd = reinterpret_cast<bGPdata *>(id);
      if ((legacy_gpd->flag & GP_DATA_ANNOTATIONS) != 0) {
        continue;
      }
    }

    id->tag |= ID_TAG_DOIT;
  }
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = item.new_id;
    if (id == nullptr || GS(id->name) != ID_OB) {
      continue;
    }

    Object *ob = reinterpret_cast<Object *>(id);
    Object *new_ob = reinterpret_cast<Object *>(id->newid);
    if (ob->data != nullptr) {
      (static_cast<ID *>(ob->data))->tag &= ~ID_TAG_DOIT;
    }
    if (new_ob != nullptr && new_ob->data != nullptr) {
      (static_cast<ID *>(new_ob->data))->tag &= ~ID_TAG_DOIT;
    }
  }
}

/* Test whether some ancestor collection is also tagged for instantiation (return true) or not
 * (return false). */
static bool loose_data_instantiate_collection_parents_check_recursive(Collection *collection)
{
  for (CollectionParent *parent_collection =
           static_cast<CollectionParent *>(collection->runtime->parents.first);
       parent_collection != nullptr;
       parent_collection = parent_collection->next)
  {
    if ((parent_collection->collection->id.tag & ID_TAG_DOIT) != 0) {
      return true;
    }
    if (loose_data_instantiate_collection_parents_check_recursive(parent_collection->collection)) {
      return true;
    }
  }
  return false;
}

static void loose_data_instantiate_collection_process(
    LooseDataInstantiateContext *instantiate_context)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  Main *bmain = lapp_context->params->bmain;
  Scene *scene = lapp_context->params->context.scene;
  ViewLayer *view_layer = lapp_context->params->context.view_layer;
  const View3D *v3d = lapp_context->params->context.v3d;

  const bool do_append = (lapp_context->params->flag & FILE_LINK) == 0;
  const bool do_instantiate_as_empty = (lapp_context->params->flag &
                                        BLO_LIBLINK_COLLECTION_INSTANCE) != 0;

  /* NOTE: For collections we only view_layer-instantiate duplicated collections that have
   * non-instantiated objects in them.
   * NOTE: Also avoid view-layer-instantiating of collections children of other instantiated
   * collections. This is why we need two passes here. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr || GS(id->name) != ID_GR) {
      continue;
    }

    /* Forced instantiation of indirectly appended collections is not wanted. Users can now
     * easily instantiate collections (and their objects) as needed by themselves. See #67032. */
    /* We need to check that objects in that collections are already instantiated in a scene.
     * Otherwise, it's better to add the collection to the scene's active collection, than to
     * instantiate its objects in active scene's collection directly. See #61141.
     *
     * NOTE: We only check object directly into that collection, not recursively into its
     * children.
     */
    Collection *collection = (Collection *)id;
    /* The collection could be linked/appended together with an Empty object instantiating it,
     * better not instantiate the collection in the view-layer in that case.
     *
     * Can easily happen when copy/pasting such instantiating empty, see #93839. */
    const bool collection_is_instantiated = collection_instantiated_by_any_object(bmain,
                                                                                  collection);
    /* Always consider adding collections directly selected by the user. */
    bool do_add_collection = (item.tag & LINK_APPEND_TAG_INDIRECT) == 0 &&
                             !collection_is_instantiated;
    /* In linking case, do not enforce instantiating non-directly linked collections/objects.
     * This avoids cluttering the view-layers, user can instantiate themselves specific collections
     * or objects easily from the Outliner if needed. */
    if (!do_add_collection && do_append && !collection_is_instantiated) {
      LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
        Object *ob = coll_ob->ob;
        if (!object_in_any_scene(bmain, ob)) {
          do_add_collection = true;
          break;
        }
      }
    }
    if (do_add_collection) {
      collection->id.tag |= ID_TAG_DOIT;
    }
  }

  /* Second loop to actually instantiate collections tagged as such in first loop, unless some of
   * their ancestor is also instantiated in case this is not an empty-instantiation. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr || GS(id->name) != ID_GR) {
      continue;
    }

    Collection *collection = reinterpret_cast<Collection *>(id);
    bool do_add_collection = (id->tag & ID_TAG_DOIT) != 0;

    if (!do_add_collection) {
      continue;
    }
    /* When instantiated into view-layer, do not add collections if one of their parents is also
     * instantiated. */
    if (!do_instantiate_as_empty &&
        loose_data_instantiate_collection_parents_check_recursive(collection))
    {
      continue;
    }
    /* When instantiated as empty, do not add indirectly linked (i.e. non-user-selected)
     * collections. */
    if (do_instantiate_as_empty && (item.tag & LINK_APPEND_TAG_INDIRECT) != 0) {
      continue;
    }

    loose_data_instantiate_ensure_active_collection(instantiate_context);
    Collection *active_collection = instantiate_context->active_collection;

    if (do_instantiate_as_empty) {
      /* BKE_object_add(...) messes with the selection. */
      Object *ob = BKE_object_add_only_object(bmain, OB_EMPTY, collection->id.name + 2);
      ob->type = OB_EMPTY;
      ob->empty_drawsize = U.collection_instance_empty_size;

      const bool set_selected = (lapp_context->params->flag & FILE_AUTOSELECT) != 0;
      /* TODO: why is it OK to make this active here but not in other situations?
       * See other callers of #object_base_instance_init */
      const bool set_active = set_selected;
      loose_data_instantiate_object_base_instance_init(bmain,
                                                       active_collection,
                                                       ob,
                                                       scene,
                                                       view_layer,
                                                       v3d,
                                                       lapp_context->params->flag,
                                                       set_active);

      /* Assign the collection. */
      ob->instance_collection = collection;
      id_us_plus(&collection->id);
      ob->transflag |= OB_DUPLICOLLECTION;
      copy_v3_v3(ob->loc, scene->cursor.location);
    }
    else {
      /* Add collection as child of active collection. */
      BKE_collection_child_add(bmain, active_collection, collection);
      BKE_view_layer_synced_ensure(scene, view_layer);

      if ((lapp_context->params->flag & FILE_AUTOSELECT) != 0) {
        /* All objects contained in this collection need to be processed, including the ones
         * belonging to children collections. */
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
          Base *base = BKE_view_layer_base_find(view_layer, ob);
          if (base) {
            base->flag |= BASE_SELECTED;
            BKE_scene_object_base_flag_sync_from_base(base);
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }
    }
  }
}

static void loose_data_instantiate_object_process(LooseDataInstantiateContext *instantiate_context)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  Main *bmain = lapp_context->params->bmain;
  const Scene *scene = lapp_context->params->context.scene;
  ViewLayer *view_layer = lapp_context->params->context.view_layer;
  const View3D *v3d = lapp_context->params->context.v3d;

  const bool do_object_active_done = (lapp_context->params->flag &
                                      BLO_LIBLINK_APPEND_SET_OB_ACTIVE_CLIPBOARD);

  /* Do NOT make base active here! screws up GUI stuff,
   * if you want it do it at the editor level (unless `do_object_active_done` is set). */
  bool object_set_active = false;

  const bool is_linking = (lapp_context->params->flag & FILE_LINK) != 0;

  /* NOTE: For objects we only view_layer-instantiate duplicated objects that are not yet used
   * anywhere. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr || GS(id->name) != ID_OB) {
      continue;
    }

    /* In linking case, never instantiate stray objects that are not directly linked.
     *
     * While this is not ideal (in theory no object should remain un-owned), in case of indirectly
     * linked objects, the other solution would be to add them to a local collection, which would
     * make them directly linked. Think for now keeping them indirectly linked is more important.
     * Ref. #93757.
     */
    if (is_linking && (item.tag & LINK_APPEND_TAG_INDIRECT) != 0) {
      continue;
    }

    Object *ob = (Object *)id;

    if (object_in_any_collection(bmain, ob)) {
      continue;
    }

    loose_data_instantiate_ensure_active_collection(instantiate_context);
    Collection *active_collection = instantiate_context->active_collection;

    CLAMP_MIN(ob->id.us, 0);
    ob->mode = OB_MODE_OBJECT;

    object_set_active = do_object_active_done && (ob->flag & OB_FLAG_ACTIVE_CLIPBOARD);

    loose_data_instantiate_object_base_instance_init(bmain,
                                                     active_collection,
                                                     ob,
                                                     scene,
                                                     view_layer,
                                                     v3d,
                                                     lapp_context->params->flag,
                                                     object_set_active);
  }
}

static void loose_data_instantiate_obdata_process(LooseDataInstantiateContext *instantiate_context)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  Main *bmain = lapp_context->params->bmain;
  Scene *scene = lapp_context->params->context.scene;
  ViewLayer *view_layer = lapp_context->params->context.view_layer;
  const View3D *v3d = lapp_context->params->context.v3d;

  /* Do NOT make base active here! screws up GUI stuff,
   * if you want it do it at the editor level. */
  const bool object_set_active = false;

  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr) {
      continue;
    }
    const ID_Type idcode = GS(id->name);
    if (!OB_DATA_SUPPORT_ID(idcode)) {
      continue;
    }
    if ((id->tag & ID_TAG_DOIT) == 0) {
      continue;
    }

    loose_data_instantiate_ensure_active_collection(instantiate_context);
    Collection *active_collection = instantiate_context->active_collection;

    const int type = BKE_object_obdata_to_type(id);
    BLI_assert(type != -1);
    Object *ob = BKE_object_add_only_object(bmain, type, id->name + 2);
    ob->data = id;
    id_us_plus(id);
    BKE_object_materials_sync_length(bmain, ob, static_cast<ID *>(ob->data));

    loose_data_instantiate_object_base_instance_init(bmain,
                                                     active_collection,
                                                     ob,
                                                     scene,
                                                     view_layer,
                                                     v3d,
                                                     lapp_context->params->flag,
                                                     object_set_active);

    copy_v3_v3(ob->loc, scene->cursor.location);

    id->tag &= ~ID_TAG_DOIT;
  }
}

static void loose_data_instantiate_object_rigidbody_postprocess(
    LooseDataInstantiateContext *instantiate_context)
{
  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  Main *bmain = lapp_context->params->bmain;

  /* Add rigid body objects and constraints to current RB world(s). */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = loose_data_instantiate_process_check(instantiate_context, &item);
    if (id == nullptr || GS(id->name) != ID_OB) {
      continue;
    }
    BKE_rigidbody_ensure_local_object(bmain, reinterpret_cast<Object *>(id));
  }
}

static void loose_data_instantiate(LooseDataInstantiateContext *instantiate_context)
{
  if (instantiate_context->lapp_context->params->context.scene == nullptr) {
    /* In some cases, like the asset drag&drop e.g., the caller code manages instantiation itself.
     */
    return;
  }

  BlendfileLinkAppendContext *lapp_context = instantiate_context->lapp_context;
  const bool do_obdata = (lapp_context->params->flag & BLO_LIBLINK_OBDATA_INSTANCE) != 0;

  /* First pass on obdata to enable their instantiation by default, then do a second pass on
   * objects to clear it for any obdata already in use. */
  if (do_obdata) {
    loose_data_instantiate_obdata_preprocess(instantiate_context);
  }

  /* First do collections, then objects, then obdata. */
  loose_data_instantiate_collection_process(instantiate_context);
  loose_data_instantiate_object_process(instantiate_context);
  if (do_obdata) {
    loose_data_instantiate_obdata_process(instantiate_context);
  }

  loose_data_instantiate_object_rigidbody_postprocess(instantiate_context);
}

static void new_id_to_item_mapping_add(BlendfileLinkAppendContext &lapp_context,
                                       ID *id,
                                       BlendfileLinkAppendContextItem &item)
{
  lapp_context.new_id_to_item.add(id, &item);
}

/* Generate a mapping between newly linked IDs and their items, and tag linked IDs used as
 * liboverride references as already existing. */
static void new_id_to_item_mapping_create(BlendfileLinkAppendContext &lapp_context)
{
  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }

    new_id_to_item_mapping_add(lapp_context, id, item);
  }
}

/* All callbacks processing dependencies of an ID for link/append post-processing share a same
 * common logic to skip some cases. This is factorized in this helper function.
 *
 * Returns false if further processing should be skipped. */
static bool foreach_libblock_link_append_common_processing(
    LibraryIDLinkCallbackData *cb_data, blender::FunctionRef<LibraryIDLinkCallback> callback)
{
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING | IDWALK_CB_INTERNAL |
                          IDWALK_CB_LOOPBACK))
  {
    return false;
  }

  ID *id = *cb_data->id_pointer;
  if (id == nullptr) {
    return false;
  }
  if (!ID_IS_LINKED(id)) {
    CLOG_ERROR(
        &LOG,
        "Local ID '%s' found as part of the linked data hierarchy, this should never happen",
        id->name);
    return false;
  }

  if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
    /* While we do not want to add non-linkable ID (shape keys...) to the list of linked items,
     * unfortunately they can use fully linkable valid IDs too, like actions. Those need to be
     * processed, so we need to recursively deal with them here. */
    /* NOTE: Since we are bypassing checks in `BKE_library_foreach_ID_link` by manually calling it
     * recursively, we need to take care of potential recursion cases ourselves (e.g.anim-data of
     * shape-key referencing the shape-key itself). */
    /* NOTE: in case both IDs (owner and 'used' ones) are non-linkable, we can assume we can break
     * the dependency here. Indeed, either they are both linked in another way (through their own
     * meshes for shape keys e.g.), or this is an unsupported case (two shape-keys depending on
     * each-other need to be also 'linked' in by their respective meshes, independent shape-keys
     * are not allowed). ref #96048. */
    if (id != cb_data->self_id && BKE_idtype_idcode_is_linkable(GS(cb_data->self_id->name))) {
      BKE_library_foreach_ID_link(cb_data->bmain, id, callback, cb_data->user_data, IDWALK_NOP);
    }
    return false;
  }

  return true;
}

/** \} */

/** \name Library packing code.
 * \{ */

void BKE_blendfile_link_pack(BlendfileLinkAppendContext *lapp_context, ReportList * /*reports*/)
{
  Main *bmain = lapp_context->params->bmain;

  new_id_to_item_mapping_create(*lapp_context);

  /* Find all newly linked data-blocks, these will need to be deleted after they have been
   * successfully packed, to avoid keeping lots of unused linked IDs around.
   *
   * Also add them to the items list, such that they can be checked, and removed from the deletion
   * set in case packing fails. */
  blender::Set<ID *> linked_ids_to_delete;

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter) || ID_IS_PACKED(id_iter) ||
        (id_iter->tag & ID_TAG_PRE_EXISTING) != 0)
    {
      continue;
    }

    linked_ids_to_delete.add(id_iter);

    BlendfileLinkAppendContextItem *item = lapp_context->new_id_to_item.lookup_default(id_iter,
                                                                                       nullptr);
    if (item == nullptr) {
      item = BKE_blendfile_link_append_context_item_add(
          lapp_context, BKE_id_name(*id_iter), GS(id_iter->name), nullptr);
      item->new_id = id_iter;
      item->source_library = id_iter->lib;
      /* Since we did not have an item for that ID yet, we know user did not select it
       * explicitly, it was rather linked indirectly. This info is important for
       * instantiation of collections.
       */
      item->tag |= LINK_APPEND_TAG_INDIRECT;
      item->action = LINK_APPEND_ACT_UNSET;
      new_id_to_item_mapping_add(*lapp_context, id_iter, *item);
    }
  }
  FOREACH_MAIN_ID_END;

  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    BLI_assert(ID_IS_LINKED(id));
    if (!(ID_IS_PACKED(id) || (id->newid && ID_IS_PACKED(id->newid)))) {
      /* No yet packed. */
      blender::bke::library::pack_linked_id_hierarchy(*bmain, *id);
    }
    /* Calling code may want to access newly packed embedded IDs from the link/append context
     * items. */
    if (id->newid) {
      item.new_id = id->newid;
    }

    /* If packing failed for a linked ID, do not delete its linked version. */
    if (!ID_IS_PACKED(item.new_id) && linked_ids_to_delete.contains(id)) {
      linked_ids_to_delete.remove(id);
    }
  }
  BKE_main_id_newptr_and_tag_clear(bmain);

  BKE_id_multi_delete(bmain, linked_ids_to_delete);
}

/** \} */

/** \name Library append code.
 * \{ */

static int foreach_libblock_append_add_dependencies_callback(LibraryIDLinkCallbackData *cb_data)
{
  if (!foreach_libblock_link_append_common_processing(
          cb_data, foreach_libblock_append_add_dependencies_callback))
  {
    return IDWALK_RET_NOP;
  }
  ID *id = *cb_data->id_pointer;
  const BlendfileLinkAppendContextCallBack *data =
      static_cast<BlendfileLinkAppendContextCallBack *>(cb_data->user_data);

  /* NOTE: In append case, all dependencies are needed in the items list, to cover potential
   * complex cases (e.g. linked data from another library referencing other IDs from the  */

  BlendfileLinkAppendContextItem *item = data->lapp_context->new_id_to_item.lookup_default(
      id, nullptr);
  if (item == nullptr) {
    item = BKE_blendfile_link_append_context_item_add(
        data->lapp_context, BKE_id_name(*id), GS(id->name), nullptr);
    item->new_id = id;
    item->source_library = id->lib;
    /* Since we did not have an item for that ID yet, we know user did not select it explicitly,
     * it was rather linked indirectly. This info is important for instantiation of collections.
     */
    item->tag |= LINK_APPEND_TAG_INDIRECT;
    item->action = LINK_APPEND_ACT_UNSET;
    new_id_to_item_mapping_add(*data->lapp_context, id, *item);

    if ((cb_data->cb_flag & IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE) != 0 ||
        data->is_liboverride_dependency_only)
    {
      /* New item, (currently) detected as only used as a liboverride linked dependency. */
      item->tag |= LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY;
    }
    else if (data->is_liboverride_dependency) {
      /* New item, (currently) detected as used as a liboverride linked dependency, among
       * others. */
      item->tag |= LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY;
    }
  }
  else {
    if ((cb_data->cb_flag & IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE) != 0 ||
        data->is_liboverride_dependency_only)
    {
      /* Existing item, here only used as a liboverride reference dependency. If it was not
       * tagged as such before, it is also used by non-liboverride reference data. */
      if ((item->tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) == 0) {
        item->tag |= LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY;
      }
    }
    else if ((item->tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) != 0) {
      /* Existing item, here used in a non-liboverride dependency context. If it was
       * tagged as a liboverride dependency only, its tag and action need to be updated. */
      item->tag &= ~LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY;
      item->tag |= LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY;
    }
  }

  return IDWALK_RET_NOP;
}

static int foreach_libblock_append_ensure_reusable_local_id_callback(
    LibraryIDLinkCallbackData *cb_data)
{
  if (!foreach_libblock_link_append_common_processing(
          cb_data, foreach_libblock_append_ensure_reusable_local_id_callback))
  {
    return IDWALK_RET_NOP;
  }
  ID *id = *cb_data->id_pointer;
  const BlendfileLinkAppendContextCallBack *data =
      static_cast<BlendfileLinkAppendContextCallBack *>(cb_data->user_data);

  if (!data->item->reusable_local_id) {
    return IDWALK_RET_NOP;
  }

  BlendfileLinkAppendContextItem *item = data->lapp_context->new_id_to_item.lookup(id);
  BLI_assert(item != nullptr);

  /* If the currently processed owner ID is not defined as being kept linked, and is using a
   * dependency that cannot be reused form local data, then the owner ID should not reuse its
   * local data either. */
  if (item->action != LINK_APPEND_ACT_KEEP_LINKED && item->reusable_local_id == nullptr) {
    BKE_main_library_weak_reference_remove_item(data->lapp_context->library_weak_reference_mapping,
                                                cb_data->owner_id->lib->filepath,
                                                cb_data->owner_id->name,
                                                data->item->reusable_local_id);
    data->item->reusable_local_id = nullptr;
  }

  return IDWALK_RET_NOP;
}

static int foreach_libblock_append_finalize_action_callback(LibraryIDLinkCallbackData *cb_data)
{
  if (!foreach_libblock_link_append_common_processing(
          cb_data, foreach_libblock_append_finalize_action_callback))
  {
    return IDWALK_RET_NOP;
  }
  ID *id = *cb_data->id_pointer;
  BlendfileLinkAppendContextCallBack *data = static_cast<BlendfileLinkAppendContextCallBack *>(
      cb_data->user_data);

  BlendfileLinkAppendContextItem *item = data->lapp_context->new_id_to_item.lookup(id);
  BLI_assert(item != nullptr);
  BLI_assert(data->item->action == LINK_APPEND_ACT_KEEP_LINKED);

  if (item->action == LINK_APPEND_ACT_MAKE_LOCAL) {
    CLOG_DEBUG(&LOG,
               "Appended ID '%s' was to be made directly local, but is also used by data that is "
               "kept linked, so duplicating it instead.",
               id->name);
    item->action = LINK_APPEND_ACT_COPY_LOCAL;
  }
  return IDWALK_RET_NOP;
}

static void blendfile_append_define_actions(BlendfileLinkAppendContext &lapp_context,
                                            ReportList *reports)
{
  Main *bmain = lapp_context.params->bmain;

  const bool do_recursive = (lapp_context.params->flag & BLO_LIBLINK_APPEND_RECURSIVE) != 0;
  const bool do_reuse_local_id = (lapp_context.params->flag & BLO_LIBLINK_APPEND_LOCAL_ID_REUSE) !=
                                 0;

  /* In case of non-recursive appending, gather a set of all 'original' libraries (i.e. libraries
   * containing data that was explicitly selected by the user). */
  blender::Set<Library *> direct_libraries;
  if (!do_recursive) {
    for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
      ID *id = item.new_id;
      if (id == nullptr) {
        continue;
      }
      direct_libraries.add(id->lib);
    }
  }

  /* Add items for all not yet known IDs (i.e. implicitly linked indirect dependencies) to the
   * list.
   * NOTE: Since items are appended, this list will grow and these IDs will be processed later,
   * leading to a flatten recursive processing of all the linked dependencies.
   */
  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    BLI_assert(item.reusable_local_id == nullptr);

    /* NOTE: handling of reusable local ID info is needed, even if their usage is not requested
     * for that append operation:
     *  - Newly appended data need to get their weak reference, such that it can be reused later
     * if requested.
     *  - Existing appended data may need to get this 'reuse' weak reference cleared, e.g. if a
     * new version of it is made local. */
    item.reusable_local_id = BKE_idtype_idcode_append_is_reusable(GS(id->name)) ?
                                 BKE_main_library_weak_reference_search_item(
                                     lapp_context.library_weak_reference_mapping,
                                     id->lib->filepath,
                                     id->name) :
                                 nullptr;

    BlendfileLinkAppendContextCallBack cb_data{};
    cb_data.lapp_context = &lapp_context;
    cb_data.item = &item;
    cb_data.reports = reports;
    cb_data.is_liboverride_dependency = (item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) != 0;
    cb_data.is_liboverride_dependency_only = (item.tag &
                                              LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) != 0;
    BKE_library_foreach_ID_link(
        bmain, id, foreach_libblock_append_add_dependencies_callback, &cb_data, IDWALK_NOP);
  }

  /* At this point, linked IDs that should remain linked can already be defined as such:
   *  - In case of non-recursive appending, IDs from other libraries.
   *  - IDs only used as liboverride references. */
  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    /* These tags should have been set in above loop, here they can be check for validity (they
     * are mutually exclusive). */
    BLI_assert(
        (item.tag &
         (LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY | LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY)) !=
        (LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY | LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY));

    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    /* IDs exclusively used as liboverride reference should not be made local at all. */
    if ((item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) != 0) {
      CLOG_DEBUG(
          &LOG,
          "Appended ID '%s' is only used as a liboverride linked dependency, keeping it linked.",
          id->name);
      item.action = LINK_APPEND_ACT_KEEP_LINKED;
      item.reusable_local_id = nullptr;
    }
    /* In non-recursive append case, only IDs from the same libraries as the directly appended
     * ones are made local. All dependencies from other libraries are kept linked. */
    if (!do_recursive && !direct_libraries.contains(id->lib)) {
      CLOG_DEBUG(&LOG,
                 "Appended ID '%s' belongs to another library and recursive append is disabled, "
                 "keeping it linked.",
                 id->name);
      item.action = LINK_APPEND_ACT_KEEP_LINKED;
      item.reusable_local_id = nullptr;
    }
  }

  /* The reusable local IDs can cause severe issues in hierarchies of appended data. If an ID
   * user e.g. still has a local reusable ID found, but one of its dependencies does not (i.e.
   * either there were some changes in the library data, or the previously appended local
   * dependencies was modified in current file and therefore cannot be re-used anymore), then the
   * user ID should not be considered as usable either. */
  /* TODO: This process is currently fairly raw and inefficient. This is likely not a
   * (significant) issue currently anyway. But would be good to refactor this whole code to use
   * modern CPP containers (list of items could be an `std::deque` e.g., to be iterable in both
   * directions). Being able to loop backward here (i.e. typically process the dependencies
   * before the user IDs) could avoid a lot of iterations. */
  for (bool keep_looping = do_reuse_local_id; keep_looping;) {
    keep_looping = false;
    for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
      ID *id = item.new_id;
      if (id == nullptr) {
        continue;
      }
      if (!item.reusable_local_id) {
        continue;
      }
      BlendfileLinkAppendContextCallBack cb_data{};
      cb_data.lapp_context = &lapp_context;
      cb_data.item = &item;
      cb_data.reports = reports;
      cb_data.is_liboverride_dependency = (item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) != 0;
      cb_data.is_liboverride_dependency_only = (item.tag &
                                                LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) != 0;
      BKE_library_foreach_ID_link(bmain,
                                  id,
                                  foreach_libblock_append_ensure_reusable_local_id_callback,
                                  &cb_data,
                                  IDWALK_NOP);
      if (!item.reusable_local_id) {
        /* If some reusable ID was cleared, another loop over all items is needed to potentially
         * propagate this change higher in the dependency hierarchy. */
        keep_looping = true;
      }
    }
  }

  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }

    if (item.action != LINK_APPEND_ACT_UNSET) {
      /* Already set, pass. */
      BLI_assert(item.action == LINK_APPEND_ACT_KEEP_LINKED);
      continue;
    }
    BLI_assert((item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) == 0);

    if (do_reuse_local_id && item.reusable_local_id != nullptr) {
      CLOG_DEBUG(&LOG, "Appended ID '%s' as a matching local one, re-using it.", id->name);
      item.action = LINK_APPEND_ACT_REUSE_LOCAL;
    }
    else if (id->tag & ID_TAG_PRE_EXISTING) {
      CLOG_DEBUG(&LOG, "Appended ID '%s' was already linked, duplicating it.", id->name);
      item.action = LINK_APPEND_ACT_COPY_LOCAL;
    }
    else if (item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) {
      CLOG_DEBUG(
          &LOG,
          "Appended ID '%s' is also used as a liboverride linked dependency, duplicating it.",
          id->name);
      item.action = LINK_APPEND_ACT_COPY_LOCAL;
    }
    else if (ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY(id)) {
      /* While in theory liboverrides can be directly made local, this causes complex potential
       * problems, e.g. because hierarchy roots can become temporarily invalid when the root is
       * made local, etc.
       *
       * So for now, simpler to always duplicate linked liboverrides. */
      CLOG_DEBUG(&LOG, "Appended ID '%s' is a liboverride, duplicating it.", id->name);
      item.action = LINK_APPEND_ACT_COPY_LOCAL;
    }
    else {
      /* That last action, making linked data directly local, can still be changed to
       * #LINK_APPEND_ACT_COPY_LOCAL in the last checks below. This can happen in rare cases with
       * complex relationships involving IDs that are kept linked and IDs that are made local,
       * both using some same dependencies. */
      CLOG_DEBUG(&LOG, "Appended ID '%s' will be made local.", id->name);
      item.action = LINK_APPEND_ACT_MAKE_LOCAL;
    }
  }

  /* Some linked IDs marked to be made directly local may also be used by other items
   * marked to be kept linked. in such case, they need to be copied for the local data, such that
   * a linked version of these remains available as dependency for other linked data. */
  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }

    /* Only IDs kept as linked need to be checked here. */
    if (item.action == LINK_APPEND_ACT_KEEP_LINKED) {
      BlendfileLinkAppendContextCallBack cb_data{};
      cb_data.lapp_context = &lapp_context;
      cb_data.item = &item;
      cb_data.reports = reports;
      cb_data.is_liboverride_dependency = (item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) != 0;
      cb_data.is_liboverride_dependency_only = (item.tag &
                                                LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) != 0;
      BKE_library_foreach_ID_link(
          bmain, id, foreach_libblock_append_finalize_action_callback, &cb_data, IDWALK_NOP);
    }

    /* If we found a matching existing local id but are not re-using it, we need to properly
     * clear its weak reference to linked data. */
    if (item.reusable_local_id != nullptr &&
        !ELEM(item.action, LINK_APPEND_ACT_KEEP_LINKED, LINK_APPEND_ACT_REUSE_LOCAL))
    {
      BLI_assert_msg(!do_reuse_local_id,
                     "This code should only be reached when the current append operation does not "
                     "try to reuse local data.");
      BKE_main_library_weak_reference_remove_item(lapp_context.library_weak_reference_mapping,
                                                  id->lib->filepath,
                                                  id->name,
                                                  item.reusable_local_id);
      item.reusable_local_id = nullptr;
    }
  }
}

void BKE_blendfile_append(BlendfileLinkAppendContext *lapp_context, ReportList *reports)
{
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Linking);
  lapp_context->process_stage = BlendfileLinkAppendContext::ProcessStage::Appending;

  if (lapp_context->items.empty()) {
    /* Nothing to append. */
    return;
  }

  Main *bmain = lapp_context->params->bmain;

  BLI_assert((lapp_context->params->flag & FILE_LINK) == 0);

  const bool set_fakeuser = (lapp_context->params->flag & BLO_LIBLINK_APPEND_SET_FAKEUSER) != 0;

  const int make_local_common_flags =
      LIB_ID_MAKELOCAL_FULL_LIBRARY |
      ((lapp_context->params->flag & BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR) != 0 ?
           LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR :
           0) |
      /* In recursive case (i.e. everything becomes local), clear liboverrides. Otherwise (i.e.
       * only data from immediately linked libraries is made local), preserve liboverrides. */
      ((lapp_context->params->flag & BLO_LIBLINK_APPEND_RECURSIVE) != 0 ?
           LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR :
           0);

  new_id_to_item_mapping_create(*lapp_context);
  lapp_context->library_weak_reference_mapping = BKE_main_library_weak_reference_create(bmain);

  /* Add missing items (the indirectly linked ones), and carefully define which action should be
   * applied to each of them. */
  blendfile_append_define_actions(*lapp_context, reports);

  /* Effectively perform required operation on every linked ID. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }

    ID *local_appended_new_id = nullptr;
    char lib_filepath[FILE_MAX];
    STRNCPY(lib_filepath, id->lib->filepath);
    char lib_id_name[MAX_ID_NAME];
    STRNCPY(lib_id_name, id->name);

    switch (item.action) {
      case LINK_APPEND_ACT_COPY_LOCAL:
        BKE_lib_id_make_local(bmain, id, make_local_common_flags | LIB_ID_MAKELOCAL_FORCE_COPY);
        local_appended_new_id = id->newid;
        break;
      case LINK_APPEND_ACT_MAKE_LOCAL:
        BKE_lib_id_make_local(bmain, id, make_local_common_flags | LIB_ID_MAKELOCAL_FORCE_LOCAL);
        BLI_assert(id->newid == nullptr);
        local_appended_new_id = id;
        break;
      case LINK_APPEND_ACT_KEEP_LINKED:
        /* Nothing to do here. */
        break;
      case LINK_APPEND_ACT_REUSE_LOCAL:
        BLI_assert(item.reusable_local_id != nullptr);
        /* We only need to set `newid` to ID found in previous loop, for proper remapping. */
        ID_NEW_SET(id, item.reusable_local_id);
        /* This is not a 'new' local appended id, do not set `local_appended_new_id` here. */
        break;
      case LINK_APPEND_ACT_UNSET:
        CLOG_ERROR(
            &LOG, "Unexpected unset append action for '%s' ID, assuming 'keep link'", id->name);
        break;
      default:
        BLI_assert_unreachable();
    }

    if (local_appended_new_id != nullptr) {
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
  lapp_context->library_weak_reference_mapping = nullptr;

  /* Remap IDs as needed. */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    if (item.action == LINK_APPEND_ACT_KEEP_LINKED) {
      continue;
    }

    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    if (ELEM(item.action, LINK_APPEND_ACT_COPY_LOCAL, LINK_APPEND_ACT_REUSE_LOCAL)) {
      BLI_assert(ID_IS_LINKED(id));
      id = id->newid;
      if (id == nullptr) {
        continue;
      }
    }

    BLI_assert(!ID_IS_LINKED(id));

    BKE_libblock_relink_to_newid(bmain, id, 0);
  }

  /* Remove linked IDs when a local existing data has been reused instead. */
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    if (!ELEM(item.action, LINK_APPEND_ACT_COPY_LOCAL, LINK_APPEND_ACT_REUSE_LOCAL)) {
      continue;
    }

    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    BLI_assert(ID_IS_LINKED(id));
    BLI_assert(id->newid != nullptr);

    /* Calling code may want to access newly appended IDs from the link/append context items. */
    item.new_id = id->newid;

    /* Only the 'reuse local' action should leave unused newly linked data behind. */
    if (item.action != LINK_APPEND_ACT_REUSE_LOCAL) {
      continue;
    }
    /* Do NOT delete a linked data that was already linked before this append. */
    if (id->tag & ID_TAG_PRE_EXISTING) {
      continue;
    }
    /* Do NOT delete a linked data that is (also) used a liboverride dependency. */
    BLI_assert((item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY) == 0);
    if (item.tag & LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) {
      continue;
    }

    id->tag |= ID_TAG_DOIT;
  }
  BKE_id_multi_tagged_delete(bmain);

  BKE_main_id_newptr_and_tag_clear(bmain);

  BlendFileReadReport bf_reports{};
  bf_reports.reports = reports;
  BLO_read_do_version_after_setup(bmain, lapp_context, &bf_reports);

  BLO_readfile_id_runtime_data_free_all(*bmain);
}

/** \} */

/** \name Library link code.
 * \{ */

static int foreach_libblock_link_finalize_cb(LibraryIDLinkCallbackData *cb_data)
{
  if (!foreach_libblock_link_append_common_processing(cb_data, foreach_libblock_link_finalize_cb))
  {
    return IDWALK_RET_NOP;
  }
  ID *id = *cb_data->id_pointer;
  const BlendfileLinkAppendContextCallBack *data =
      static_cast<BlendfileLinkAppendContextCallBack *>(cb_data->user_data);

  if ((id->tag & ID_TAG_PRE_EXISTING) != 0) {
    /* About to re-use a linked data that was already there, and that will stay linked. This case
     * does not need any further processing of the child hierarchy (existing linked data
     * instantiation status should not be modified here). */
    return IDWALK_RET_NOP;
  }

  /* In linking case, all linked IDs are considered for instantiation, including from other
   * libraries. So all linked IDs that were not skipped so far need to be added to the items
   * list.
   */
  BlendfileLinkAppendContextItem *item = data->lapp_context->new_id_to_item.lookup_default(
      id, nullptr);
  /* NOTE: liboverride info (tags like #LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY) can be
   * ignored/skipped here, since all data are kept linked anyway, they are not useful currently.
   */
  if (item == nullptr) {
    item = BKE_blendfile_link_append_context_item_add(
        data->lapp_context, BKE_id_name(*id), GS(id->name), nullptr);
    item->new_id = id;
    item->source_library = id->lib;
    /* Since there is no item for that ID yet, the user did not select it explicitly, it was
     * rather linked indirectly. This info is important for instantiation of collections. */
    item->tag |= LINK_APPEND_TAG_INDIRECT;
    /* In linking case we already know what we want to do with these items. */
    item->action = LINK_APPEND_ACT_KEEP_LINKED;
    new_id_to_item_mapping_add(*data->lapp_context, id, *item);
  }
  return IDWALK_RET_NOP;
}

void BKE_blendfile_link_append_instantiate_loose(BlendfileLinkAppendContext *lapp_context,
                                                 ReportList *reports)
{
  BLI_assert(ELEM(lapp_context->process_stage,
                  BlendfileLinkAppendContext::ProcessStage::Linking,
                  BlendfileLinkAppendContext::ProcessStage::Appending));
  lapp_context->process_stage = BlendfileLinkAppendContext::ProcessStage::Instantiating;

  if (!lapp_context->params->context.scene) {
    return;
  }
  if (lapp_context->params->flag & FILE_LINK) {
    new_id_to_item_mapping_create(*lapp_context);
    /* Add items for all not yet known IDs (i.e. implicitly linked indirect dependencies) to the
     * list.
     * NOTE: Since items are appended to the list, this list will grow and these IDs will be
     * processed later, leading to a flatten recursive processing of all the linked dependencies.
     */
    for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
      ID *id = item.new_id;
      if (id == nullptr) {
        continue;
      }
      BLI_assert(item.userdata == nullptr);

      BlendfileLinkAppendContextCallBack cb_data{};
      cb_data.lapp_context = lapp_context;
      cb_data.item = &item;
      cb_data.reports = reports;
      BKE_library_foreach_ID_link(lapp_context->params->bmain,
                                  id,
                                  foreach_libblock_link_finalize_cb,
                                  &cb_data,
                                  IDWALK_NOP);
    }
  }

  LooseDataInstantiateContext instantiate_context{};
  instantiate_context.lapp_context = lapp_context;
  instantiate_context.active_collection = nullptr;
  loose_data_instantiate(&instantiate_context);
}

void BKE_blendfile_link(BlendfileLinkAppendContext *lapp_context, ReportList *reports)
{
  BLI_assert(lapp_context->process_stage == BlendfileLinkAppendContext::ProcessStage::Init);
  lapp_context->process_stage = BlendfileLinkAppendContext::ProcessStage::Linking;

  if (lapp_context->items.empty()) {
    /* Nothing to be linked. */
    return;
  }

  BLI_assert(!lapp_context->libraries.is_empty());

  Main *mainl;
  Library *lib;

  for (const int lib_idx : lapp_context->libraries.index_range()) {
    BlendfileLinkAppendContextLibrary &lib_context = lapp_context->libraries[lib_idx];
    const char *libname = lib_context.path.c_str();

    if (!link_append_context_library_blohandle_ensure(*lapp_context, lib_context, reports)) {
      /* Unlikely since we just browsed it, but possible
       * Error reports will have been made by BLO_blendhandle_from_file() */
      continue;
    }

    /* here appending/linking starts */

    mainl = BLO_library_link_begin(&lib_context.blo_handle, libname, lapp_context->params);
    lib = mainl->curlib;
    BLI_assert(lib != nullptr);
    /* In case lib was already existing but not found originally, see #99820. */
    lib->id.tag &= ~ID_TAG_MISSING;

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
     * and tag those successful to not try to load them again with the other libraries. */
    for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
      ID *new_id;

      if (!item.libraries[lib_idx]) {
        continue;
      }

      new_id = BLO_library_link_named_part(
          mainl, &lib_context.blo_handle, item.idcode, item.name.c_str(), lapp_context->params);

      if (new_id) {
        /* If the link is successful, clear item's libraries 'todo' flags.
         * This avoids trying to link same item with other libraries to come. */
        item.libraries.fill(false);
        item.new_id = new_id;
        item.source_library = new_id->lib;
      }
    }

    BLO_library_link_end(mainl, &lib_context.blo_handle, lapp_context->params, reports);
    link_append_context_library_blohandle_release(*lapp_context, lib_context);
  }

  /* In linking case finalizing process (ensuring all data is valid, instantiating loose
   * collections or objects, etc.) can be done here directly.
   *
   * In append case, the finalizing process is much more complex and requires and additional call
   * to #BKE_blendfile_append for caller code. */
  if (lapp_context->params->flag & FILE_LINK) {
    BlendFileReadReport bf_reports{};
    bf_reports.reports = reports;
    BLO_read_do_version_after_setup(lapp_context->params->bmain, lapp_context, &bf_reports);

    BLO_readfile_id_runtime_data_free_all(*lapp_context->params->bmain);
  }
}

void BKE_blendfile_override(BlendfileLinkAppendContext *lapp_context,
                            const eBKELibLinkOverride flags,
                            ReportList * /*reports*/)
{
  if (lapp_context->items.empty()) {
    /* Nothing to override. */
    return;
  }

  Main *bmain = lapp_context->params->bmain;

  /* Liboverride only makes sense if data was linked, not appended. */
  BLI_assert((lapp_context->params->flag & FILE_LINK) != 0);

  const bool set_runtime = (flags & BKE_LIBLINK_OVERRIDE_CREATE_RUNTIME) != 0;
  const bool do_use_exisiting_liboverrides = (flags &
                                              BKE_LIBLINK_OVERRIDE_USE_EXISTING_LIBOVERRIDES) != 0;

  blender::Map<ID *, ID *> linked_ids_to_local_liboverrides;
  if (do_use_exisiting_liboverrides) {
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (ID_IS_LINKED(id_iter)) {
        continue;
      }
      if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_iter)) {
        continue;
      }
      /* Do not consider regular liboverrides if runtime ones are requested, and vice-versa. */
      if ((set_runtime && (id_iter->tag & ID_TAG_RUNTIME) == 0) ||
          (!set_runtime && (id_iter->tag & ID_TAG_RUNTIME) != 0))
      {
        continue;
      }

      /* In case several liboverrides exist of the same data, only consider the first found one, so
       * don't use `add_overwrite`. */
      linked_ids_to_local_liboverrides.add(id_iter->override_library->reference, id_iter);
    }
    FOREACH_MAIN_ID_END;
  }

  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *id = item.new_id;
    if (id == nullptr) {
      continue;
    }
    BLI_assert(item.userdata == nullptr);

    if (do_use_exisiting_liboverrides) {
      item.liboverride_id = linked_ids_to_local_liboverrides.lookup_default(id, nullptr);
    }
    if (item.liboverride_id == nullptr) {
      item.liboverride_id = BKE_lib_override_library_create_from_id(bmain, id, false);
      if (set_runtime) {
        item.liboverride_id->tag |= ID_TAG_RUNTIME;
        if ((id->tag & ID_TAG_PRE_EXISTING) == 0) {
          /* If the linked ID is newly linked, in case its override is runtime-only, assume its
           * reference to be indirectly linked.
           *
           * This is more of an heuristic for 'as best as possible' user feedback in the UI
           * (Outliner), which is expected to be valid in almost all practical use-cases. Direct or
           * indirect linked status is properly checked before saving .blend file. */
          id->tag &= ~ID_TAG_EXTERN;
          id->tag |= ID_TAG_INDIRECT;
        }
      }
    }
  }

  BKE_main_namemap_clear(*bmain);
}

/** \} */

/** \name Library relocating code.
 * \{ */

static void blendfile_library_relocate_id_remap_prepare(
    id::IDRemapper &remapper,
    blender::Map<ID *, ID *> &old_owner_id_to_shapekey,
    ID *old_id,
    ID *new_id,
    const bool do_reload)
{
  BLI_assert(old_id);
  if (do_reload) {
    /* Since we asked for placeholders in case of missing IDs,
     * we expect to always get a valid one. */
    BLI_assert(new_id);
  }
  if (new_id) {
    CLOG_DEBUG(&LOG,
               "Before remap of %s, old_id users: %d, new_id users: %d",
               old_id->name,
               old_id->us,
               new_id->us);
    remapper.add(old_id, new_id);
  }

  /* Usual special code for ShapeKeys snowflakes...
   *
   * NOTE: Unfortunately, actual reasons for why the old shapekeys needs to be removed from their
   * old owner ID was not documented in the initial commit. Suspect it's related to the fact that
   * the old ID should not end up using the new shapekeys? */
  Key **old_key_p = BKE_key_from_id_p(old_id);
  if (old_key_p == nullptr) {
    return;
  }
  Key *old_key = *old_key_p;
  Key *new_key = BKE_key_from_id(new_id);
  if (old_key != nullptr) {
    old_owner_id_to_shapekey.add(old_id, &old_key->id);
    *old_key_p = nullptr;
    id_us_min(&old_key->id);
    remapper.add(&old_key->id, &new_key->id);
  }
}

static void blendfile_library_relocate_id_remap_finalize(
    Main *bmain,
    blender::Map<ID *, ID *> &old_owner_id_to_shapekey,
    ID *old_id,
    ID *new_id,
    ReportList *reports,
    const bool do_reload)
{
  /* Restore old shapekey pointer in old id (see also
   * #blendfile_library_relocate_id_remap_prepare above). */
  Key **old_key_p = BKE_key_from_id_p(old_id);
  if (old_key_p) {
    Key *old_key = reinterpret_cast<Key *>(
        old_owner_id_to_shapekey.lookup_default_as(old_id, nullptr));
    if (old_key) {
      BLI_assert(GS(old_key->id.name) == ID_KE);
      *old_key_p = old_key;
      id_us_plus_no_lib(&old_key->id);
    }
  }

  if (old_id->flag & ID_FLAG_FAKEUSER) {
    id_fake_user_clear(old_id);
    id_fake_user_set(new_id);
  }

  CLOG_DEBUG(&LOG,
             "After remap of %s, old_id users: %d, new_id users: %d",
             old_id->name,
             old_id->us,
             new_id->us);

  /* In some cases, new_id might become direct link, remove parent of library in this case. */
  if (new_id->lib->runtime->parent && (new_id->tag & ID_TAG_INDIRECT) == 0) {
    if (do_reload) {
      BLI_assert_unreachable(); /* Should not happen in 'pure' reload case... */
    }
    new_id->lib->runtime->parent = nullptr;
  }

  if (old_id->us > 0 && old_id->lib == new_id->lib) {
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
      len = std::min<size_t>(len, MAX_ID_NAME - 7);
      BLI_strncpy(&old_id->name[len], "~000", 7);
    }

    id_sort_by_name(which_libbase(bmain, GS(old_id->name)), old_id, nullptr);

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

static void blendfile_library_relocate_id_remap(BlendfileLinkAppendContext &lapp_context,
                                                ReportList *reports,
                                                const bool do_reload,
                                                const int remap_flags)
{
  Main *bmain = lapp_context.params->bmain;

  id::IDRemapper remapper;
  blender::Map<ID *, ID *> old_owner_id_to_shapekey;

  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *old_id = static_cast<ID *>(item.userdata);
    if (!old_id) {
      continue;
    }
    ID *new_id = item.new_id;
    blendfile_library_relocate_id_remap_prepare(
        remapper, old_owner_id_to_shapekey, old_id, new_id, do_reload);
  }

  BKE_libblock_remap_multiple_locked(bmain, remapper, remap_flags);

  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    ID *old_id = static_cast<ID *>(item.userdata);
    if (!old_id) {
      continue;
    }
    ID *new_id = item.new_id;
    if (!new_id) {
      if (do_reload) {
        /* Since we asked for placeholders in case of missing IDs, we expect to always get a valid
         * one. */
        BLI_assert_msg(false,
                       "On library reload, placeholders should be generated when a linked ID is "
                       "missing, so there should never be a nullptr 'new_id' here");
      }
      /* If finding a valid matching ID for `old_id` in the searched library(-ies) failed, do not
       * clear references to the current 'old_id' placeholder. */
      continue;
    }
    blendfile_library_relocate_id_remap_finalize(
        bmain, old_owner_id_to_shapekey, old_id, new_id, reports, do_reload);
  }
}

/** Delete now unused linked IDs and libraries. */
static void blendfile_relocate_postprocess_cleanup(BlendfileLinkAppendContext &lapp_context)
{
  Main &bmain = *lapp_context.params->bmain;

  blender::Set<ID *> ids_to_delete = {};
  ID *id_iter;

  /* Delete all no more used old IDs. */
  /* NOTE: While this looping over until we are sure we deleted everything is very far from
   * efficient, doing otherwise would require a much more complex handling of indirectly linked IDs
   * in steps above. Currently, in case of relocation, those are skipped in remapping phase, though
   * in some cases (essentially internal links between IDs from the same library) remapping should
   * happen. But getting this to work reliably would be very difficult, so since this is not a
   * performance-critical code, better to go with the (relatively) simpler, brute-force approach
   * here in 'removal of old IDs' step. */
  bool keep_looping = true;
  while (keep_looping) {
    keep_looping = false;

    for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
      ID *old_id = static_cast<ID *>(item.userdata);

      if (old_id == nullptr) {
        continue;
      }

      if (GS(old_id->name) == ID_KE) {
        /* Shape Keys are handled as part of their owning obdata (see below). This implies that
         * there is no way to know when the old pointer gets invalid, so just clear it immediately.
         */
        item.userdata = nullptr;
        continue;
      }

      /* In case the active scene was reloaded, the context pointers in
       * `lapp_context->params->context` need to be updated before the old Scene ID is freed. */
      if (old_id == &lapp_context.params->context.scene->id) {
        BLI_assert(GS(old_id->name) == ID_SCE);
        Scene *new_scene = reinterpret_cast<Scene *>(item.new_id);
        BLI_assert(new_scene != nullptr);
        lapp_context.params->context.scene = new_scene;
        if (lapp_context.params->context.view_layer != nullptr) {
          ViewLayer *new_view_layer = BKE_view_layer_find(
              new_scene, lapp_context.params->context.view_layer->name);
          lapp_context.params->context.view_layer = static_cast<ViewLayer *>(
              (new_view_layer != nullptr) ? new_view_layer : new_scene->view_layers.first);
        }
        /* lapp_context->params->context.v3d should never be made invalid by newly linked data
         * here, as it is UI data, ultimately owned by a #bScreen ID, which is not linkable. */
      }

      if (old_id->us == 0) {
        ids_to_delete.add(old_id);
        item.userdata = nullptr;
        keep_looping = true;
        Key *old_key = BKE_key_from_id(old_id);
        if (old_key != nullptr) {
          ids_to_delete.add(&old_key->id);
        }
      }
    }
    BKE_id_multi_delete(&bmain, ids_to_delete);
    ids_to_delete.clear();
  }

  /* Some datablocks can get reloaded/replaced 'silently' because they are not linkable
   * (shape keys e.g.), so we need another loop here to clear old ones if possible. */
  FOREACH_MAIN_ID_BEGIN (&bmain, id_iter) {
    /* XXX That check may be a bit to generic/permissive? */
    if (id_iter->lib && (id_iter->flag & ID_TAG_PRE_EXISTING) && id_iter->us == 0) {
      ids_to_delete.add(id_iter);
    }
  }
  FOREACH_MAIN_ID_END;
  BKE_id_multi_delete(&bmain, ids_to_delete);
  ids_to_delete.clear();

  /* Get rid of no more used libraries... */
  ListBase *libraries = which_libbase(&bmain, ID_LI);
  LISTBASE_FOREACH (ID *, id_iter, libraries) {
    ids_to_delete.add(id_iter);
  }
  FOREACH_MAIN_ID_BEGIN (&bmain, id_iter) {
    if (id_iter->lib) {
      ids_to_delete.remove(&id_iter->lib->id);
      /* If the used library is an archive one, its owner 'normal' library is also used. */
      if (id_iter->lib->archive_parent_library) {
        BLI_assert(id_iter->lib->flag & LIBRARY_FLAG_IS_ARCHIVE);
        ids_to_delete.remove(&id_iter->lib->archive_parent_library->id);
      }
    }
  }
  FOREACH_MAIN_ID_END;
  BKE_id_multi_delete(&bmain, ids_to_delete);
}

/** Update and resync as needed liboverrides. */
static void blendfile_relocate_postprocess_liboverrides(
    BlendfileLinkAppendContext &lapp_context,
    const blender::Map<Library *, Library *> &new_to_old_libraries_map,
    ReportList *reports)
{
  Main &bmain = *lapp_context.params->bmain;

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (&bmain, id_iter) {
    if (ID_IS_LINKED(id_iter) || !ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) ||
        (id_iter->tag & ID_TAG_PRE_EXISTING) == 0)
    {
      continue;
    }
    if ((id_iter->override_library->reference->tag & ID_TAG_MISSING) == 0) {
      id_iter->tag &= ~ID_TAG_MISSING;
    }
    if ((id_iter->override_library->reference->tag & ID_TAG_PRE_EXISTING) == 0) {
      BKE_lib_override_library_update(&bmain, id_iter);
    }
  }
  FOREACH_MAIN_ID_END;

  BKE_library_main_rebuild_hierarchy(&bmain);

  /* Resync overrides if needed. */
  if (liboverride::is_auto_resync_enabled() && lapp_context.params->context.scene != nullptr) {
    BlendFileReadReport report{};
    report.reports = reports;
    BKE_lib_override_library_main_resync(&bmain,
                                         &new_to_old_libraries_map,
                                         lapp_context.params->context.scene,
                                         lapp_context.params->context.view_layer,
                                         &report);
    /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
    BKE_lib_override_library_main_operations_create(&bmain, true, nullptr);
  }
}

void BKE_blendfile_library_relocate(BlendfileLinkAppendContext *lapp_context,
                                    ReportList *reports,
                                    Library *library,
                                    const bool do_reload)
{
  Main *bmain = lapp_context->params->bmain;

  /* All override rules need to be up to date, since there will be no do_version here, otherwise
   * older, now-invalid rules might be applied and likely fail, or some changes might be missing,
   * etc. See #93353. */
  BKE_lib_override_library_main_operations_create(bmain, true, nullptr);

  /* Remove all IDs to be reloaded from Main. */
  MainListsArray lbarray = BKE_main_lists_get(*bmain);
  int lba_idx = lbarray.size();
  while (lba_idx--) {
    ID *id = static_cast<ID *>(lbarray[lba_idx]->first);
    const short idcode = id ? GS(id->name) : 0;

    if (!id || !BKE_idtype_idcode_is_linkable(idcode)) {
      /* No need to reload non-linkable data-types,
       * those will get relinked with their 'users ID'. */
      continue;
    }

    for (; id; id = static_cast<ID *>(id->next)) {
      if (id->lib == library) {
        BlendfileLinkAppendContextItem *item;

        /* We remove it from current Main, and add it to items to link... */
        /* Note that non-linkable IDs (like e.g. shape-keys) are also explicitly linked here... */
        BLI_remlink(lbarray[lba_idx], id);
        /* Usual special code for ShapeKeys snowflakes... */
        Key *old_key = BKE_key_from_id(id);
        if (old_key != nullptr) {
          BLI_remlink(which_libbase(bmain, GS(old_key->id.name)), &old_key->id);
        }

        item = BKE_blendfile_link_append_context_item_add(
            lapp_context, BKE_id_name(*id), idcode, id);
        item->libraries.fill(true);

        CLOG_DEBUG(&LOG, "Data-block to seek for: %s", id->name);
      }
    }
  }

  if (lapp_context->items.empty()) {
    /* Early out in case there is nothing to do. */
    return;
  }

  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* Since some IDs have been removed from Main, trying to rebuild collections hierarchy should not
   * happen. It has to be done manually below once removed IDs have been added back to Main. Also
   * see #136432. */
  BKE_blendfile_link_append_context_flag_set(
      lapp_context, BLO_LIBLINK_COLLECTION_NO_HIERARCHY_REBUILD, true);

  BKE_blendfile_link_append_context_init_done(lapp_context);

  /* We do not want any instantiation here! */
  BKE_blendfile_link(lapp_context, reports);

  BKE_main_lock(bmain);

  /* We add back old id to bmain.
   * We need to do this in a first, separated loop, otherwise some of those may not be handled by
   * ID remapping, which means they would still reference old data to be deleted... */
  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *old_id = static_cast<ID *>(item.userdata);

    BLI_assert(old_id);
    BLI_addtail(which_libbase(bmain, GS(old_id->name)), old_id);

    /* Usual special code for ShapeKeys snowflakes... */
    Key *old_key = BKE_key_from_id(old_id);
    if (old_key != nullptr) {
      BLI_addtail(which_libbase(bmain, GS(old_key->id.name)), &old_key->id);
    }
  }

  /* FIXME Temporary 'fix' to a problem in how temp ID are copied in
   * `BKE_lib_override_library_main_update`, see #103062.
   * Proper fix involves first addressing #90610. */
  BKE_main_collections_parent_relations_rebuild(bmain);

  /* Since our (old) reloaded IDs were removed from main, the user count done for them in linking
   * code is wrong, we need to redo it here after adding them back to main. */
  BKE_main_id_refcount_recompute(bmain, false);

  /* Mapping from old to new libraries, needed to allow liboverride resync to map properly old and
   * new data. */
  blender::Map<Library *, Library *> new_to_old_libraries_map;

  for (BlendfileLinkAppendContextItem &item : lapp_context->items) {
    ID *old_id = static_cast<ID *>(item.userdata);
    ID *new_id = item.new_id;
    if (new_id) {
      new_to_old_libraries_map.add(new_id->lib, old_id->lib);
    }
  }

  BKE_layer_collection_resync_forbid();

  /* Note that in reload case, we also want to replace indirect usages. */
  const int remap_flags = ID_REMAP_SKIP_NEVER_NULL_USAGE |
                          (do_reload ? 0 : ID_REMAP_SKIP_INDIRECT_USAGE);
  blendfile_library_relocate_id_remap(*lapp_context, reports, do_reload, remap_flags);

  BKE_layer_collection_resync_allow();
  BKE_main_collection_sync_remap(bmain);

  BKE_main_unlock(bmain);

  /* Delete all no more used old IDs. */
  blendfile_relocate_postprocess_cleanup(*lapp_context);

  /* Update and resync liboverrides of reloaded linked data-blocks. */
  blendfile_relocate_postprocess_liboverrides(*lapp_context, new_to_old_libraries_map, reports);

  BKE_main_collection_sync(bmain);
}

void BKE_blendfile_id_relocate(BlendfileLinkAppendContext &lapp_context, ReportList *reports)
{
  if (lapp_context.items.empty()) {
    /* Nothing to relocate. */
    return;
  }
  /* Only support relocating one ID at a time currently. */
  BLI_assert(lapp_context.items.size() == 1);

  Main *bmain = lapp_context.params->bmain;

  /* Relocate only works on linked data currently. */
  BLI_assert((lapp_context.params->flag & FILE_LINK) != 0);

  /* Tag everything, its generally useful to know what is new.
   *
   * Take extra care `BKE_main_id_flag_all(bmain, ID_TAG_PRE_EXISTING, false)` is called after! */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* XXX We'd need re-entrant locking on Main for this to work... */
  // BKE_main_lock(bmain);

  BKE_blendfile_link(&lapp_context, reports);

  // BKE_main_unlock(bmain);

  /* Finalize relocation (remap ID usages, rebuild LibOverrides if needed, etc.). */

  /* Mapping from old to new libraries, needed to allow liboverride resync to map properly old and
   * new data. */
  blender::Map<Library *, Library *> new_to_old_libraries_map{};

  /* The first item should be the root of the relocation, and the only one containing a non-null
   * `userdata`. */
  BlendfileLinkAppendContextItem &root_item = lapp_context.items.front();
  BLI_assert(root_item.userdata);
  ID *old_id = static_cast<ID *>(root_item.userdata);
  ID *new_id = root_item.new_id;
  new_to_old_libraries_map.add(new_id->lib, old_id->lib);
  BLI_assert(GS(old_id->name) == GS(new_id->name));
#ifndef NDEBUG
  for (BlendfileLinkAppendContextItem &item : lapp_context.items) {
    BLI_assert(&item == &root_item || item.userdata == nullptr);
  }
#endif

  BKE_main_lock(bmain);
  BKE_layer_collection_resync_forbid();

  /* Do not affect indirect usages. */
  const int remap_flags = ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_SKIP_INDIRECT_USAGE;
  blendfile_library_relocate_id_remap(lapp_context, reports, false, remap_flags);

  BKE_layer_collection_resync_allow();
  BKE_main_collection_sync_remap(bmain);
  BKE_main_unlock(bmain);

  /* Delete all no more used old IDs. */
  blendfile_relocate_postprocess_cleanup(lapp_context);

  /* Update and resync liboverrides of reloaded linked data-blocks. */
  blendfile_relocate_postprocess_liboverrides(lapp_context, new_to_old_libraries_map, reports);

  BKE_main_collection_sync(bmain);

  /* Important we unset, otherwise these object won't
   * link into other scenes from this blend file. */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);
}

/** \} */
