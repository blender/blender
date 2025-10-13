/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_anim_data.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_packedFile.hh"

#include "BLI_listbase.h"
#include "BLI_string_search.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"

#include "ED_id_management.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "UI_interface_layout.hh"
#include "UI_string_search.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

using blender::StringRef;
using blender::StringRefNull;

struct TemplateID {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;

  ListBase *idlb = nullptr;
  short idcode = 0;
  short filter = 0;
  int prv_rows = 0;
  int prv_cols = 0;
  bool preview = false;
  float scale = 0.0f;
};

/* Search browse menu, assign. */
static void template_ID_set_property_exec_fn(bContext *C, void *arg_template, void *item)
{
  TemplateID *template_ui = (TemplateID *)arg_template;

  /* ID */
  if (item) {
    PointerRNA idptr = RNA_id_pointer_create(static_cast<ID *>(item));
    RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
    RNA_property_update(C, &template_ui->ptr, template_ui->prop);
  }
}

static bool id_search_allows_id(TemplateID *template_ui, const int flag, ID *id, const char *query)
{
  ID *id_from = template_ui->ptr.owner_id;

  /* Do self check. */
  if ((flag & PROP_ID_SELF_CHECK) && id == id_from) {
    return false;
  }

  /* Use filter. */
  if (RNA_property_type(template_ui->prop) == PROP_POINTER) {
    PointerRNA ptr = RNA_id_pointer_create(id);
    if (RNA_property_pointer_poll(&template_ui->ptr, template_ui->prop, &ptr) == 0) {
      return false;
    }
  }

  /* Hide dot prefixed data-blocks, but only if filter does not force them visible. */
  if (U.uiflag & USER_HIDE_DOT) {
    if ((id->name[2] == '.') && (query[0] != '.')) {
      return false;
    }
  }

  return true;
}

static bool id_search_add(const bContext *C, TemplateID *template_ui, uiSearchItems *items, ID *id)
{
  /* +1 is needed because BKE_id_ui_prefix used 3 letter prefix
   * followed by ID_NAME-2 characters from id->name
   */
  char name_ui[MAX_ID_FULL_NAME_UI];
  int iconid = ui_id_icon_get(C, id, template_ui->preview);
  const bool use_lib_prefix = template_ui->preview || iconid;
  const bool has_sep_char = ID_IS_LINKED(id);

  /* When using previews, the library hint (linked, overridden, missing) is added with a
   * character prefix, otherwise we can use a icon. */
  int name_prefix_offset;
  BKE_id_full_name_ui_prefix_get(name_ui, id, use_lib_prefix, UI_SEP_CHAR, &name_prefix_offset);
  if (!use_lib_prefix) {
    iconid = UI_icon_from_library(id);
  }

  if (!UI_search_item_add(items,
                          name_ui,
                          id,
                          iconid,
                          has_sep_char ? int(UI_BUT_HAS_SEP_CHAR) : 0,
                          name_prefix_offset))
  {
    return false;
  }

  return true;
}

/* ID Search browse menu, do the search */
static void id_search_cb(const bContext *C,
                         void *arg_template,
                         const char *str,
                         uiSearchItems *items,
                         const bool /*is_first*/)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  const int flag = RNA_property_flag(template_ui->prop);

  blender::ui::string_search::StringSearch<ID> search;

  /* ID listbase */
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id_search_allows_id(template_ui, flag, id, str)) {
      search.add(id->name + 2, id);
    }
  }

  const blender::Vector<ID *> filtered_ids = search.query(str);

  for (ID *id : filtered_ids) {
    if (!id_search_add(C, template_ui, items, id)) {
      break;
    }
  }
}

/**
 * Use id tags for filtering.
 */
static void id_search_cb_tagged(const bContext *C,
                                void *arg_template,
                                const char *str,
                                uiSearchItems *items)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  const int flag = RNA_property_flag(template_ui->prop);

  blender::string_search::StringSearch<ID> search{nullptr,
                                                  blender::string_search::MainWordsHeuristic::All};

  /* ID listbase */
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id->tag & ID_TAG_DOIT) {
      if (id_search_allows_id(template_ui, flag, id, str)) {
        search.add(id->name + 2, id);
      }
      id->tag &= ~ID_TAG_DOIT;
    }
  }

  blender::Vector<ID *> filtered_ids = search.query(str);

  for (ID *id : filtered_ids) {
    if (!id_search_add(C, template_ui, items, id)) {
      break;
    }
  }
}

/**
 * A version of 'id_search_cb' that lists scene objects.
 */
static void id_search_cb_objects_from_scene(const bContext *C,
                                            void *arg_template,
                                            const char *str,
                                            uiSearchItems *items,
                                            const bool /*is_first*/)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  Scene *scene = nullptr;
  ID *id_from = template_ui->ptr.owner_id;

  if (id_from && GS(id_from->name) == ID_SCE) {
    scene = (Scene *)id_from;
  }
  else {
    scene = CTX_data_scene(C);
  }

  BKE_main_id_flag_listbase(lb, ID_TAG_DOIT, false);

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->id.tag |= ID_TAG_DOIT;
  }
  FOREACH_SCENE_OBJECT_END;
  id_search_cb_tagged(C, arg_template, str, items);
}

static ARegion *template_ID_search_menu_item_tooltip(
    bContext *C, ARegion *region, const rcti *item_rect, void * /*arg*/, void *active)
{
  ID *active_id = static_cast<ID *>(active);
  return UI_tooltip_create_from_search_item_generic(C, region, item_rect, active_id);
}

/* ID Search browse menu, open */
static uiBlock *id_search_menu(bContext *C, ARegion *region, void *arg_litem)
{
  static TemplateID template_ui;
  PointerRNA active_item_ptr;
  void (*id_search_update_fn)(
      const bContext *, void *, const char *, uiSearchItems *, const bool) = id_search_cb;

  /* arg_litem is malloced, can be freed by parent button */
  template_ui = *((TemplateID *)arg_litem);
  active_item_ptr = RNA_property_pointer_get(&template_ui.ptr, template_ui.prop);

  if (template_ui.filter) {
    /* Currently only used for objects. */
    if (template_ui.idcode == ID_OB) {
      if (template_ui.filter == UI_TEMPLATE_ID_FILTER_AVAILABLE) {
        id_search_update_fn = id_search_cb_objects_from_scene;
      }
    }
  }

  return template_common_search_menu(C,
                                     region,
                                     id_search_update_fn,
                                     &template_ui,
                                     template_ID_set_property_exec_fn,
                                     active_item_ptr.data,
                                     template_ID_search_menu_item_tooltip,
                                     template_ui.prv_rows,
                                     template_ui.prv_cols,
                                     template_ui.scale);
}

static void template_id_cb(bContext *C, void *arg_litem, void *arg_event);

void UI_context_active_but_prop_get_templateID(const bContext *C,
                                               PointerRNA *r_ptr,
                                               PropertyRNA **r_prop)
{
  uiBut *but = UI_context_active_but_get(C);

  *r_ptr = {};
  *r_prop = nullptr;

  if (but && (but->funcN == template_id_cb) && but->func_argN) {
    TemplateID *template_ui = static_cast<TemplateID *>(but->func_argN);
    *r_ptr = template_ui->ptr;
    *r_prop = template_ui->prop;
  }
}

static void template_id_liboverride_hierarchy_collection_root_find_recursive(
    Collection *collection,
    const int parent_level,
    Collection **r_collection_parent_best,
    int *r_parent_level_best)
{
  if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY_REAL(collection)) {
    return;
  }
  if (ID_IS_OVERRIDABLE_LIBRARY(collection) || ID_IS_OVERRIDE_LIBRARY_REAL(collection)) {
    if (parent_level > *r_parent_level_best) {
      *r_parent_level_best = parent_level;
      *r_collection_parent_best = collection;
    }
  }
  for (CollectionParent *iter =
           static_cast<CollectionParent *>(collection->runtime->parents.first);
       iter != nullptr;
       iter = iter->next)
  {
    if (iter->collection->id.lib != collection->id.lib && ID_IS_LINKED(iter->collection)) {
      continue;
    }
    template_id_liboverride_hierarchy_collection_root_find_recursive(
        iter->collection, parent_level + 1, r_collection_parent_best, r_parent_level_best);
  }
}

static void template_id_liboverride_hierarchy_collections_tag_recursive(
    Collection *root_collection, ID *target_id, const bool do_parents)
{
  root_collection->id.tag |= ID_TAG_DOIT;

  /* Tag all local parents of the root collection, so that usages of the root collection and other
   * linked ones can be replaced by the local overrides in those parents too. */
  if (do_parents) {
    for (CollectionParent *iter =
             static_cast<CollectionParent *>(root_collection->runtime->parents.first);
         iter != nullptr;
         iter = iter->next)
    {
      if (ID_IS_LINKED(iter->collection)) {
        continue;
      }
      iter->collection->id.tag |= ID_TAG_DOIT;
    }
  }

  for (CollectionChild *iter = static_cast<CollectionChild *>(root_collection->children.first);
       iter != nullptr;
       iter = iter->next)
  {
    if (ID_IS_LINKED(iter->collection) && iter->collection->id.lib != target_id->lib) {
      continue;
    }
    if (GS(target_id->name) == ID_OB &&
        !BKE_collection_has_object_recursive(iter->collection, (Object *)target_id))
    {
      continue;
    }
    if (GS(target_id->name) == ID_GR &&
        !BKE_collection_has_collection(iter->collection, (Collection *)target_id))
    {
      continue;
    }
    template_id_liboverride_hierarchy_collections_tag_recursive(
        iter->collection, target_id, false);
  }
}

ID *ui_template_id_liboverride_hierarchy_make(
    bContext *C, Main *bmain, ID *owner_id, ID *id, const char **r_undo_push_label)
{
  const char *undo_push_label;
  if (r_undo_push_label == nullptr) {
    r_undo_push_label = &undo_push_label;
  }

  /* If this is called on an already local override, 'toggle' between user-editable state, and
   * system override with reset. */
  if (!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY(id)) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      BKE_lib_override_library_get(bmain, id, nullptr, &id);
    }
    if (id->override_library->flag & LIBOVERRIDE_FLAG_SYSTEM_DEFINED) {
      id->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      *r_undo_push_label = "Make Library Override Hierarchy Editable";
    }
    else {
      BKE_lib_override_library_id_reset(bmain, id, true);
      *r_undo_push_label = "Clear Library Override Hierarchy";
    }

    WM_event_add_notifier(C, NC_WM | ND_DATACHANGED, nullptr);
    WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
    return id;
  }

  /* Attempt to perform a hierarchy override, based on contextual data available.
   * NOTE: do not attempt to perform such hierarchy override at all cost, if there is not enough
   * context, better to abort than create random overrides all over the place. */
  if (!ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(id)) {
    WM_global_reportf(RPT_ERROR, "The data-block %s is not overridable", id->name);
    return nullptr;
  }

  Object *object_active = CTX_data_active_object(C);
  if (object_active == nullptr && GS(owner_id->name) == ID_OB) {
    object_active = (Object *)owner_id;
  }
  if (object_active != nullptr) {
    if (ID_IS_LINKED(object_active)) {
      if (object_active->id.lib != id->lib || !ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(object_active))
      {
        /* The active object is from a different library than the overridden ID, or otherwise
         * cannot be used in hierarchy. */
        object_active = nullptr;
      }
    }
    else if (!ID_IS_OVERRIDE_LIBRARY_REAL(object_active)) {
      /* Fully local object cannot be used in override hierarchy either. */
      object_active = nullptr;
    }
  }

  Collection *collection_active_context = CTX_data_collection(C);
  Collection *collection_active = collection_active_context;
  if (collection_active == nullptr && GS(owner_id->name) == ID_GR) {
    collection_active = (Collection *)owner_id;
  }
  if (collection_active != nullptr) {
    if (ID_IS_LINKED(collection_active)) {
      if (collection_active->id.lib != id->lib ||
          !ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(collection_active))
      {
        /* The active collection is from a different library than the overridden ID, or otherwise
         * cannot be used in hierarchy. */
        collection_active = nullptr;
      }
      else {
        int parent_level_best = -1;
        Collection *collection_parent_best = nullptr;
        template_id_liboverride_hierarchy_collection_root_find_recursive(
            collection_active, 0, &collection_parent_best, &parent_level_best);
        collection_active = collection_parent_best;
      }
    }
    else if (!ID_IS_OVERRIDE_LIBRARY_REAL(collection_active)) {
      /* Fully local collection cannot be used in override hierarchy either. */
      collection_active = nullptr;
    }
  }
  if (collection_active == nullptr && object_active != nullptr &&
      (ID_IS_LINKED(object_active) || ID_IS_OVERRIDE_LIBRARY_REAL(object_active)))
  {
    /* If we failed to find a valid 'active' collection so far for our override hierarchy, but do
     * have a valid 'active' object, try to find a collection from that object. */
    LISTBASE_FOREACH (Collection *, collection_iter, &bmain->collections) {
      if (ID_IS_LINKED(collection_iter) && collection_iter->id.lib != id->lib) {
        continue;
      }
      if (!ID_IS_OVERRIDE_LIBRARY_REAL(collection_iter)) {
        continue;
      }
      if (!BKE_collection_has_object_recursive(collection_iter, object_active)) {
        continue;
      }
      int parent_level_best = -1;
      Collection *collection_parent_best = nullptr;
      template_id_liboverride_hierarchy_collection_root_find_recursive(
          collection_iter, 0, &collection_parent_best, &parent_level_best);
      collection_active = collection_parent_best;
      break;
    }
  }

  ID *id_override = nullptr;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  switch (GS(id->name)) {
    case ID_GR:
      if (collection_active != nullptr &&
          BKE_collection_has_collection(collection_active, (Collection *)id))
      {
        template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
        if (object_active != nullptr) {
          object_active->id.tag |= ID_TAG_DOIT;
        }
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        nullptr,
                                        id,
                                        &collection_active->id,
                                        nullptr,
                                        &id_override,
                                        false);
      }
      else if (object_active != nullptr && !ID_IS_LINKED(object_active) &&
               &object_active->instance_collection->id == id)
      {
        object_active->id.tag |= ID_TAG_DOIT;
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        id->lib,
                                        id,
                                        &object_active->id,
                                        &object_active->id,
                                        &id_override,
                                        false);
      }
      break;
    case ID_OB:
      if (collection_active != nullptr &&
          BKE_collection_has_object_recursive(collection_active, (Object *)id))
      {
        template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
        if (object_active != nullptr) {
          object_active->id.tag |= ID_TAG_DOIT;
        }
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        nullptr,
                                        id,
                                        &collection_active->id,
                                        nullptr,
                                        &id_override,
                                        false);
      }
      else {
        if (object_active != nullptr) {
          object_active->id.tag |= ID_TAG_DOIT;
        }
        BKE_lib_override_library_create(
            bmain, scene, view_layer, nullptr, id, nullptr, nullptr, &id_override, false);
        BKE_scene_collections_object_remove(bmain, scene, (Object *)id, true);
        WM_event_add_notifier(C, NC_ID | NA_REMOVED, nullptr);
      }
      break;
    case ID_ME:
    case ID_CU_LEGACY:
    case ID_MB:
    case ID_LT:
    case ID_LA:
    case ID_CA:
    case ID_SPK:
    case ID_AR:
    case ID_GD_LEGACY:
    case ID_CV:
    case ID_PT:
    case ID_VO:
    case ID_NT: /* Essentially geometry nodes from modifier currently. */
      if (object_active != nullptr) {
        if (collection_active != nullptr &&
            BKE_collection_has_object_recursive(collection_active, object_active))
        {
          template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
          object_active->id.tag |= ID_TAG_DOIT;
          BKE_lib_override_library_create(bmain,
                                          scene,
                                          view_layer,
                                          nullptr,
                                          id,
                                          &collection_active->id,
                                          nullptr,
                                          &id_override,
                                          false);
        }
        else {
          object_active->id.tag |= ID_TAG_DOIT;
          BKE_lib_override_library_create(bmain,
                                          scene,
                                          view_layer,
                                          nullptr,
                                          id,
                                          &object_active->id,
                                          nullptr,
                                          &id_override,
                                          false);
        }
      }
      else {
        BKE_lib_override_library_create(
            bmain, scene, view_layer, nullptr, id, id, nullptr, &id_override, false);
      }
      break;
    case ID_MA:
    case ID_TE:
    case ID_IM:
      WM_global_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    case ID_WO:
      WM_global_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    case ID_PA:
      WM_global_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    default:
      WM_global_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
  }

  if (id_override != nullptr) {
    id_override->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;

    /* Ensure that the hierarchy root of the newly overridden data is instantiated in the scene, in
     * case it's a collection or object. */
    ID *hierarchy_root = id_override->override_library->hierarchy_root;
    if (GS(hierarchy_root->name) == ID_OB) {
      Object *object_hierarchy_root = reinterpret_cast<Object *>(hierarchy_root);
      if (!BKE_scene_has_object(scene, object_hierarchy_root)) {
        if (!ID_IS_LINKED(collection_active_context)) {
          BKE_collection_object_add(bmain, collection_active_context, object_hierarchy_root);
        }
        else {
          BKE_collection_object_add(bmain, scene->master_collection, object_hierarchy_root);
        }
      }
    }
    else if (GS(hierarchy_root->name) == ID_GR) {
      Collection *collection_hierarchy_root = reinterpret_cast<Collection *>(hierarchy_root);
      if (!BKE_collection_has_collection(scene->master_collection, collection_hierarchy_root)) {
        if (!ID_IS_LINKED(collection_active_context)) {
          BKE_collection_child_add(bmain, collection_active_context, collection_hierarchy_root);
        }
        else {
          BKE_collection_child_add(bmain, scene->master_collection, collection_hierarchy_root);
        }
      }
    }

    *r_undo_push_label = "Make Library Override Hierarchy";

    /* In theory we could rely on setting/updating the RNA ID pointer property (as done by calling
     * code) to be enough.
     *
     * However, some rare ID pointers properties (like the "active object in view-layer" one used
     * for the Object templateID in the Object properties) use notifiers that do not enforce a
     * rebuild of outliner trees, leading to crashes.
     *
     * So for now, add some extra notifiers here. */
    WM_event_add_notifier(C, NC_ID | NA_ADDED, nullptr);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
  }
  return id_override;
}

static void template_id_liboverride_hierarchy_make(bContext *C,
                                                   Main *bmain,
                                                   TemplateID *template_ui,
                                                   PointerRNA *idptr,
                                                   const char **r_undo_push_label)
{
  ID *id = static_cast<ID *>(idptr->data);
  ID *owner_id = template_ui->ptr.owner_id;

  ID *id_override = ui_template_id_liboverride_hierarchy_make(
      C, bmain, owner_id, id, r_undo_push_label);

  if (id_override != nullptr) {
    /* `idptr` is re-assigned to owner property to ensure proper updates etc. Here we also use it
     * to ensure remapping of the owner property from the linked data to the newly created
     * liboverride (note that in theory this remapping has already been done by code above), but
     * only in case owner ID was already local ID (override or pure local data).
     *
     * Otherwise, owner ID will also have been overridden, and remapped already to use it's
     * override of the data too. */
    if (!ID_IS_LINKED(owner_id)) {
      *idptr = RNA_id_pointer_create(id_override);
    }
  }
  else {
    WM_global_reportf(RPT_ERROR, "The data-block %s could not be overridden", id->name);
  }
}

static void template_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
  TemplateID *template_ui = (TemplateID *)arg_litem;
  PointerRNA idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
  ID *id = static_cast<ID *>(idptr.data);
  const int event = POINTER_AS_INT(arg_event);
  const char *undo_push_label = nullptr;

  switch (event) {
    case UI_ID_NOP:
      /* Don't do anything, typically set for buttons that execute an operator instead. They may
       * still assign the callback so the button can be identified as part of an ID-template. See
       * #UI_context_active_but_prop_get_templateID(). */
      break;
    case UI_ID_RENAME:
      /* Only for the undo push. */
      undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Data-Block");
      break;
    case UI_ID_BROWSE:
    case UI_ID_PIN:
      RNA_warning("warning, id event %d shouldn't come here", event);
      break;
    case UI_ID_OPEN:
    case UI_ID_ADD_NEW:
      /* these call UI_context_active_but_prop_get_templateID */
      break;
    case UI_ID_DELETE:
      idptr = {};
      RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
      RNA_property_update(C, &template_ui->ptr, template_ui->prop);

      if (id && CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
        /* only way to force-remove data (on save) */
        id_us_clear_real(id);
        id_fake_user_clear(id);
        id->us = 0;
        undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Data-Block");
      }
      else {
        undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Unlink Data-Block");
      }

      break;
    case UI_ID_FAKE_USER:
      if (id) {
        if (id->flag & ID_FLAG_FAKEUSER) {
          id_us_plus(id);
        }
        else {
          id_us_min(id);
        }
        undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Fake User");
      }
      else {
        return;
      }
      break;
    case UI_ID_LOCAL:
      if (id) {
        Main *bmain = CTX_data_main(C);
        if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
          template_id_liboverride_hierarchy_make(C, bmain, template_ui, &idptr, &undo_push_label);
        }
        else {
          if (BKE_lib_id_make_local(bmain, id, LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR)) {
            BKE_id_newptr_and_tag_clear(id);

            /* Reassign to get proper updates/notifiers. */
            idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
            undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Make Local");
          }
        }
        if (undo_push_label != nullptr) {
          RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
          RNA_property_update(C, &template_ui->ptr, template_ui->prop);
        }
      }
      break;
    case UI_ID_OVERRIDE:
      if (id && ID_IS_OVERRIDE_LIBRARY(id)) {
        Main *bmain = CTX_data_main(C);
        if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
          template_id_liboverride_hierarchy_make(C, bmain, template_ui, &idptr, &undo_push_label);
        }
        else {
          BKE_lib_override_library_make_local(bmain, id);
          /* Reassign to get proper updates/notifiers. */
          idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
          RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
          RNA_property_update(C, &template_ui->ptr, template_ui->prop);
          undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Make Local");
        }
      }
      break;
    case UI_ID_ALONE:
      if (id) {
        const bool do_scene_obj = ((GS(id->name) == ID_OB) &&
                                   (template_ui->ptr.type == &RNA_LayerObjects));

        /* make copy */
        if (do_scene_obj) {
          Main *bmain = CTX_data_main(C);
          Scene *scene = CTX_data_scene(C);
          blender::ed::object::object_single_user_make(bmain, scene, (Object *)id);
          WM_event_add_notifier(C, NC_WINDOW, nullptr);
          DEG_relations_tag_update(bmain);
        }
        else {
          Main *bmain = CTX_data_main(C);
          id_single_user(C, id, &template_ui->ptr, template_ui->prop);
          WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
          DEG_relations_tag_update(bmain);
        }
        BKE_main_ensure_invariants(*CTX_data_main(C));
        undo_push_label = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Make Single User");
      }
      break;
#if 0
      case UI_ID_AUTO_NAME:
      break;
#endif
  }

  if (undo_push_label != nullptr) {
    ED_undo_push(C, undo_push_label);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
  }
}

static StringRef template_id_browse_tip(const StructRNA *type)
{
  if (type) {
    switch ((ID_Type)RNA_type_to_ID_code(type)) {
      case ID_SCE:
        return N_("Browse Scene to be linked");
      case ID_OB:
        return N_("Browse Object to be linked");
      case ID_ME:
        return N_("Browse Mesh Data to be linked");
      case ID_CU_LEGACY:
        return N_("Browse Curve Data to be linked");
      case ID_MB:
        return N_("Browse Metaball Data to be linked");
      case ID_MA:
        return N_("Browse Material to be linked");
      case ID_TE:
        return N_("Browse Texture to be linked");
      case ID_IM:
        return N_("Browse Image to be linked");
      case ID_LS:
        return N_("Browse Line Style Data to be linked");
      case ID_LT:
        return N_("Browse Lattice Data to be linked");
      case ID_LA:
        return N_("Browse Light Data to be linked");
      case ID_CA:
        return N_("Browse Camera Data to be linked");
      case ID_WO:
        return N_("Browse World Settings to be linked");
      case ID_SCR:
        return N_("Choose Screen layout");
      case ID_TXT:
        return N_("Browse Text to be linked");
      case ID_SPK:
        return N_("Browse Speaker Data to be linked");
      case ID_SO:
        return N_("Browse Sound to be linked");
      case ID_AR:
        return N_("Browse Armature data to be linked");
      case ID_AC:
        return N_("Browse Action to be linked");
      case ID_NT:
        return N_("Browse Node Tree to be linked");
      case ID_BR:
        return N_("Browse Brush to be linked");
      case ID_PA:
        return N_("Browse Particle Settings to be linked");
      case ID_GD_LEGACY:
        return N_("Browse Grease Pencil Data to be linked");
      case ID_MC:
        return N_("Browse Movie Clip to be linked");
      case ID_MSK:
        return N_("Browse Mask to be linked");
      case ID_PAL:
        return N_("Browse Palette Data to be linked");
      case ID_PC:
        return N_("Browse Paint Curve Data to be linked");
      case ID_CF:
        return N_("Browse Cache Files to be linked");
      case ID_WS:
        return N_("Browse Workspace to be linked");
      case ID_LP:
        return N_("Browse LightProbe to be linked");
      case ID_CV:
        return N_("Browse Curves Data to be linked");
      case ID_PT:
        return N_("Browse Point Cloud Data to be linked");
      case ID_VO:
        return N_("Browse Volume Data to be linked");
      case ID_GP:
        return N_("Browse Grease Pencil Data to be linked");

        /* Use generic text. */
      case ID_LI:
      case ID_KE:
      case ID_VF:
      case ID_GR:
      case ID_WM:
        break;
    }
  }
  return N_("Browse ID data to be linked");
}

/**
 * Add a superimposed extra icon to \a but, for workspace pinning.
 * Rather ugly special handling, but this is really a special case at this point, nothing worth
 * generalizing.
 */
static void template_id_workspace_pin_extra_icon(const TemplateID &template_ui, uiBut *but)
{
  if ((template_ui.idcode != ID_SCE) || (template_ui.ptr.type != &RNA_Window)) {
    return;
  }

  const wmWindow *win = static_cast<const wmWindow *>(template_ui.ptr.data);
  const WorkSpace *workspace = WM_window_get_active_workspace(win);
  UI_but_extra_operator_icon_add(but,
                                 "WORKSPACE_OT_scene_pin_toggle",
                                 blender::wm::OpCallContext::InvokeDefault,
                                 (workspace->flags & WORKSPACE_USE_PIN_SCENE) ? ICON_PINNED :
                                                                                ICON_UNPINNED);
}

/**
 * \return a type-based i18n context, needed e.g. by "New" button.
 * In most languages, this adjective takes different form based on gender of type name...
 */
#ifdef WITH_INTERNATIONAL
static const char *template_id_context(StructRNA *type)
{
  if (type) {
    return BKE_idtype_idcode_to_translation_context(RNA_type_to_ID_code(type));
  }
  return BLT_I18NCONTEXT_DEFAULT;
}
#else
#  define template_id_context(type) 0
#endif

static uiBut *template_id_def_new_but(uiBlock *block,
                                      const ID *id,
                                      const TemplateID &template_ui,
                                      StructRNA *type,
                                      const char *const newop,
                                      const bool editable,
                                      const bool id_open,
                                      const bool use_tab_but,
                                      int but_height)
{
  ID *idfrom = template_ui.ptr.owner_id;
  uiBut *but;
  const ButType but_type = use_tab_but ? ButType::Tab : ButType::But;

  /* i18n markup, does nothing! */
  BLT_I18N_MSGID_MULTI_CTXT("New",
                            BLT_I18NCONTEXT_DEFAULT,
                            BLT_I18NCONTEXT_ID_ACTION,
                            BLT_I18NCONTEXT_ID_ARMATURE,
                            BLT_I18NCONTEXT_ID_BRUSH,
                            BLT_I18NCONTEXT_ID_CAMERA,
                            BLT_I18NCONTEXT_ID_CURVES,
                            BLT_I18NCONTEXT_ID_CURVE_LEGACY,
                            BLT_I18NCONTEXT_ID_FREESTYLELINESTYLE,
                            BLT_I18NCONTEXT_ID_GPENCIL,
                            BLT_I18NCONTEXT_ID_IMAGE,
                            BLT_I18NCONTEXT_ID_LATTICE,
                            BLT_I18NCONTEXT_ID_LIGHT,
                            BLT_I18NCONTEXT_ID_LIGHTPROBE,
                            BLT_I18NCONTEXT_ID_MASK,
                            BLT_I18NCONTEXT_ID_MATERIAL,
                            BLT_I18NCONTEXT_ID_MESH, );
  BLT_I18N_MSGID_MULTI_CTXT("New",
                            BLT_I18NCONTEXT_ID_METABALL,
                            BLT_I18NCONTEXT_ID_NODETREE,
                            BLT_I18NCONTEXT_ID_OBJECT,
                            BLT_I18NCONTEXT_ID_PAINTCURVE,
                            BLT_I18NCONTEXT_ID_PALETTE,
                            BLT_I18NCONTEXT_ID_PARTICLESETTINGS,
                            BLT_I18NCONTEXT_ID_POINTCLOUD,
                            BLT_I18NCONTEXT_ID_SCENE,
                            BLT_I18NCONTEXT_ID_SCREEN,
                            BLT_I18NCONTEXT_ID_SOUND,
                            BLT_I18NCONTEXT_ID_SPEAKER,
                            BLT_I18NCONTEXT_ID_TEXT,
                            BLT_I18NCONTEXT_ID_TEXTURE,
                            BLT_I18NCONTEXT_ID_VOLUME,
                            BLT_I18NCONTEXT_ID_WORKSPACE,
                            BLT_I18NCONTEXT_ID_WORLD, );
  /* NOTE: BLT_I18N_MSGID_MULTI_CTXT takes a maximum number of parameters,
   * check the definition to see if a new call must be added when the limit
   * is exceeded. */

  const char *button_text = (id) ? "" : CTX_IFACE_(template_id_context(type), "New");
  const int icon = (id && !use_tab_but) ? ICON_DUPLICATE : ICON_ADD;
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  int w = id ? UI_UNIT_X : id_open ? UI_UNIT_X * 3 : UI_UNIT_X * 6;
  if (!id) {
    w = std::max(UI_fontstyle_string_width(fstyle, button_text) + int(UI_UNIT_X * 1.5f), w);
  }

  if (newop) {
    but = uiDefIconTextButO(block,
                            but_type,
                            newop,
                            blender::wm::OpCallContext::InvokeDefault,
                            icon,
                            button_text,
                            0,
                            0,
                            w,
                            but_height,
                            std::nullopt);
    UI_but_funcN_set(but,
                     template_id_cb,
                     MEM_new<TemplateID>(__func__, template_ui),
                     POINTER_FROM_INT(UI_ID_ADD_NEW),
                     but_func_argN_free<TemplateID>,
                     but_func_argN_copy<TemplateID>);
  }
  else {
    but = uiDefIconTextBut(
        block, but_type, 0, icon, button_text, 0, 0, w, but_height, nullptr, std::nullopt);
    UI_but_funcN_set(but,
                     template_id_cb,
                     MEM_new<TemplateID>(__func__, template_ui),
                     POINTER_FROM_INT(UI_ID_ADD_NEW),
                     but_func_argN_free<TemplateID>,
                     but_func_argN_copy<TemplateID>);
  }

  if ((idfrom && !ID_IS_EDITABLE(idfrom)) || !editable) {
    UI_but_flag_enable(but, UI_BUT_DISABLED);
  }

#ifndef WITH_INTERNATIONAL
  UNUSED_VARS(type);
#endif

  return but;
}

static void template_ID(const bContext *C,
                        uiLayout *layout,
                        TemplateID &template_ui,
                        StructRNA *type,
                        int flag,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        const std::optional<StringRef> text,
                        const bool live_icon,
                        const bool hide_buttons)
{
  uiBut *but;
  const bool editable = RNA_property_editable(&template_ui.ptr, template_ui.prop);
  const bool use_previews = template_ui.preview = (flag & UI_ID_PREVIEWS) != 0;

  PointerRNA idptr = RNA_property_pointer_get(&template_ui.ptr, template_ui.prop);
  ID *id = static_cast<ID *>(idptr.data);
  ID *idfrom = template_ui.ptr.owner_id;
  // lb = template_ui->idlb;

  /* Allow operators to take the ID from context. */
  layout->context_ptr_set("id", &idptr);

  uiBlock *block = layout->block();
  UI_block_align_begin(block);

  if (idptr.type) {
    type = idptr.type;
  }

  if (text && !text->is_empty()) {
    /* Add label respecting the separated layout property split state. */
    uiItemL_respect_property_split(layout, *text, ICON_NONE);
  }

  if (flag & UI_ID_BROWSE) {
    template_add_button_search_menu(C,
                                    layout,
                                    block,
                                    &template_ui.ptr,
                                    template_ui.prop,
                                    id_search_menu,
                                    MEM_new<TemplateID>(__func__, template_ui),
                                    TIP_(template_id_browse_tip(type)),
                                    use_previews,
                                    editable,
                                    live_icon,
                                    but_func_argN_free<TemplateID>,
                                    but_func_argN_copy<TemplateID>);
  }

  /* text button with name */
  if (id) {
    char name[UI_MAX_NAME_STR];
    const bool user_alert = (id->us <= 0);

    int width = template_search_textbut_width(&idptr, RNA_struct_find_property(&idptr, "name"));

    if ((template_ui.idcode == ID_SCE) && (template_ui.ptr.type == &RNA_Window)) {
      /* More room needed for "pin" icon. */
      width += UI_UNIT_X;
    }

    const int height = template_search_textbut_height();

    // text_idbutton(id, name);
    name[0] = '\0';
    but = uiDefButR(block,
                    ButType::Text,
                    0,
                    name,
                    0,
                    0,
                    width,
                    height,
                    &idptr,
                    "name",
                    -1,
                    0,
                    0,
                    RNA_struct_ui_description(type));
    /* Handle undo through the #template_id_cb set below. Default undo handling from the button
     * code (see #ui_apply_but_undo) would not work here, as the new name is not yet applied to the
     * ID. */
    UI_but_flag_disable(but, UI_BUT_UNDO);
    Main *bmain = CTX_data_main(C);
    UI_but_func_rename_full_set(
        but, [bmain, id](std::string &new_name) { ED_id_rename(*bmain, *id, new_name); });
    UI_but_funcN_set(but,
                     template_id_cb,
                     MEM_new<TemplateID>(__func__, template_ui),
                     POINTER_FROM_INT(UI_ID_RENAME),
                     but_func_argN_free<TemplateID>,
                     but_func_argN_copy<TemplateID>);
    if (user_alert) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }

    template_id_workspace_pin_extra_icon(template_ui, but);

    if (!hide_buttons && !(idfrom && ID_IS_LINKED(idfrom))) {
      if (ID_IS_LINKED(id)) {
        const bool disabled = !BKE_idtype_idcode_is_localizable(GS(id->name));
        if (ID_IS_PACKED(id)) {
          but = uiDefIconBut(block,
                             ButType::But,
                             0,
                             ICON_PACKAGE,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             TIP_("Packed library data-block, click to unpack and make local"));
        }
        else if (id->tag & ID_TAG_INDIRECT) {
          but = uiDefIconBut(block,
                             ButType::But,
                             0,
                             ICON_LIBRARY_DATA_INDIRECT,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             TIP_("Indirect library data-block, cannot be made local, "
                                  "Shift + Click to create a library override hierarchy"));
        }
        else {
          but = uiDefIconBut(block,
                             ButType::But,
                             0,
                             ICON_LIBRARY_DATA_DIRECT,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             TIP_("Direct linked library data-block, click to make local, "
                                  "Shift + Click to create a library override"));
        }
        if (disabled) {
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
        else {
          UI_but_funcN_set(but,
                           template_id_cb,
                           MEM_new<TemplateID>(__func__, template_ui),
                           POINTER_FROM_INT(UI_ID_LOCAL),
                           but_func_argN_free<TemplateID>,
                           but_func_argN_copy<TemplateID>);
        }
      }
      else if (ID_IS_OVERRIDE_LIBRARY(id)) {
        but = uiDefIconBut(
            block,
            ButType::But,
            0,
            ICON_LIBRARY_DATA_OVERRIDE,
            0,
            0,
            UI_UNIT_X,
            UI_UNIT_Y,
            nullptr,
            0,
            0,
            TIP_("Library override of linked data-block, click to make fully local, "
                 "Shift + Click to clear the library override and toggle if it can be edited"));
        UI_but_funcN_set(but,
                         template_id_cb,
                         MEM_new<TemplateID>(__func__, template_ui),
                         POINTER_FROM_INT(UI_ID_OVERRIDE),
                         but_func_argN_free<TemplateID>,
                         but_func_argN_copy<TemplateID>);
      }
    }

    if ((ID_REAL_USERS(id) > 1) && (hide_buttons == false)) {
      char numstr[32];
      short numstr_len;

      numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%d", ID_REAL_USERS(id));

      but = uiDefBut(
          block,
          ButType::But,
          0,
          numstr,
          0,
          0,
          numstr_len * 0.2f * UI_UNIT_X + UI_UNIT_X,
          UI_UNIT_Y,
          nullptr,
          0,
          0,
          TIP_("Display number of users of this data (click to make a single-user copy)"));
      but->flag |= UI_BUT_UNDO;

      UI_but_funcN_set(but,
                       template_id_cb,
                       MEM_new<TemplateID>(__func__, template_ui),
                       POINTER_FROM_INT(UI_ID_ALONE),
                       but_func_argN_free<TemplateID>,
                       but_func_argN_copy<TemplateID>);
      if (!BKE_id_copy_is_allowed(id) || (idfrom && !ID_IS_EDITABLE(idfrom)) || (!editable) ||
          /* object in editmode - don't change data */
          (idfrom && GS(idfrom->name) == ID_OB && (((Object *)idfrom)->mode & OB_MODE_EDIT)))
      {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
    }

    if (user_alert) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }

    if (!ID_IS_LINKED(id)) {
      if (ID_IS_ASSET(id)) {
        uiDefIconButO(block,
                      /* Using `_N` version allows us to get the 'active' state by default. */
                      ButType::IconToggleN,
                      "ASSET_OT_clear_single",
                      blender::wm::OpCallContext::InvokeDefault,
                      /* 'active' state of a toggle button uses icon + 1, so to get proper asset
                       * icon we need to pass its value - 1 here. */
                      ICON_ASSET_MANAGER - 1,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      std::nullopt);
      }
      else if (!ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_OB, ID_WS) && (hide_buttons == false))
      {
        uiDefIconButR(block,
                      ButType::IconToggle,
                      0,
                      ICON_FAKE_USER_OFF,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      &idptr,
                      "use_fake_user",
                      -1,
                      0,
                      0,
                      std::nullopt);
      }
    }
  }

  if ((flag & UI_ID_ADD_NEW) && (hide_buttons == false)) {
    template_id_def_new_but(
        block, id, template_ui, type, newop, editable, flag & UI_ID_OPEN, false, UI_UNIT_X);
  }

  /* Due to space limit in UI - skip the "open" icon for packed data, and allow to unpack.
   * Only for images, sound and fonts */
  if (id && BKE_packedfile_id_check(id)) {
    but = uiDefIconButO(block,
                        ButType::But,
                        "FILE_OT_unpack_item",
                        blender::wm::OpCallContext::InvokeRegionWin,
                        ICON_PACKAGE,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        TIP_("Packed File, click to unpack"));
    UI_but_operator_ptr_ensure(but);

    RNA_string_set(but->opptr, "id_name", id->name + 2);
    RNA_int_set(but->opptr, "id_type", GS(id->name));

    if (!ID_IS_EDITABLE(id)) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }
  else if (flag & UI_ID_OPEN) {
    const char *button_text = (id) ? "" : IFACE_("Open");
    const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

    int w = id ? UI_UNIT_X : (flag & UI_ID_ADD_NEW) ? UI_UNIT_X * 3 : UI_UNIT_X * 6;
    if (!id) {
      w = std::max(UI_fontstyle_string_width(fstyle, button_text) + int(UI_UNIT_X * 1.5f), w);
    }

    if (openop) {
      but = uiDefIconTextButO(block,
                              ButType::But,
                              openop,
                              blender::wm::OpCallContext::InvokeDefault,
                              ICON_FILEBROWSER,
                              (id) ? "" : IFACE_("Open"),
                              0,
                              0,
                              w,
                              UI_UNIT_Y,
                              std::nullopt);
      UI_but_funcN_set(but,
                       template_id_cb,
                       MEM_new<TemplateID>(__func__, template_ui),
                       POINTER_FROM_INT(UI_ID_OPEN),
                       but_func_argN_free<TemplateID>,
                       but_func_argN_copy<TemplateID>);
    }
    else {
      but = uiDefIconTextBut(block,
                             ButType::But,
                             0,
                             ICON_FILEBROWSER,
                             (id) ? "" : IFACE_("Open"),
                             0,
                             0,
                             w,
                             UI_UNIT_Y,
                             nullptr,
                             std::nullopt);
      UI_but_funcN_set(but,
                       template_id_cb,
                       MEM_new<TemplateID>(__func__, template_ui),
                       POINTER_FROM_INT(UI_ID_OPEN),
                       but_func_argN_free<TemplateID>,
                       but_func_argN_copy<TemplateID>);
    }

    if ((idfrom && !ID_IS_EDITABLE(idfrom)) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }

  /* delete button */
  /* don't use RNA_property_is_unlink here */
  if (id && (flag & UI_ID_DELETE) && (hide_buttons == false)) {
    /* allow unlink if 'unlinkop' is passed, even when 'PROP_NEVER_UNLINK' is set */
    but = nullptr;

    if (unlinkop) {
      but = uiDefIconButO(block,
                          ButType::But,
                          unlinkop,
                          blender::wm::OpCallContext::InvokeDefault,
                          ICON_X,
                          0,
                          0,
                          UI_UNIT_X,
                          UI_UNIT_Y,
                          std::nullopt);
      /* so we can access the template from operators, font unlinking needs this */
      UI_but_funcN_set(but,
                       template_id_cb,
                       MEM_new<TemplateID>(__func__, template_ui),
                       POINTER_FROM_INT(UI_ID_NOP),
                       but_func_argN_free<TemplateID>,
                       but_func_argN_copy<TemplateID>);
    }
    else {
      if ((RNA_property_flag(template_ui.prop) & PROP_NEVER_UNLINK) == 0) {
        but = uiDefIconBut(
            block,
            ButType::But,
            0,
            ICON_X,
            0,
            0,
            UI_UNIT_X,
            UI_UNIT_Y,
            nullptr,
            0,
            0,
            TIP_("Unlink data-block "
                 "(Shift + Click to set users to zero, data will then not be saved)"));
        UI_but_funcN_set(but,
                         template_id_cb,
                         MEM_new<TemplateID>(__func__, template_ui),
                         POINTER_FROM_INT(UI_ID_DELETE),
                         but_func_argN_free<TemplateID>,
                         but_func_argN_copy<TemplateID>);

        if (RNA_property_flag(template_ui.prop) & PROP_NEVER_NULL) {
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
      }
    }

    if (but) {
      if ((idfrom && !ID_IS_EDITABLE(idfrom)) || !editable) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
    }
  }

  if (template_ui.idcode == ID_TE) {
    uiTemplateTextureShow(layout, C, &template_ui.ptr, template_ui.prop);
  }
  UI_block_align_end(block);
}

ID *UI_context_active_but_get_tab_ID(bContext *C)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but && but->type == ButType::Tab) {
    return static_cast<ID *>(but->custom_data);
  }
  return nullptr;
}

static void template_ID_tabs(const bContext *C,
                             uiLayout *layout,
                             TemplateID &template_id,
                             StructRNA *type,
                             int flag,
                             const char *newop,
                             const char *menu)
{
  const ARegion *region = CTX_wm_region(C);
  const PointerRNA active_ptr = RNA_property_pointer_get(&template_id.ptr, template_id.prop);
  MenuType *mt = menu ? WM_menutype_find(menu, false) : nullptr;

  /* When horizontal show the tabs as pills, rounded on all corners. */
  const bool horizontal =
      (region->regiontype == RGN_TYPE_HEADER &&
       ELEM(RGN_ALIGN_ENUM_FROM_MASK(region->alignment), RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM));
  const int but_align = horizontal ? 0 : ui_but_align_opposite_to_area_align_get(region);

  const int but_height = UI_UNIT_Y * 1.1;

  uiBlock *block = layout->block();
  const uiStyle *style = UI_style_get_dpi();

  for (ID *id : BKE_id_ordered_list(template_id.idlb)) {
    const int name_width = UI_fontstyle_string_width(&style->widget, id->name + 2);
    const int but_width = name_width + UI_UNIT_X;

    uiButTab *tab = (uiButTab *)uiDefButR_prop(block,
                                               ButType::Tab,
                                               0,
                                               id->name + 2,
                                               0,
                                               0,
                                               but_width,
                                               but_height,
                                               &template_id.ptr,
                                               template_id.prop,
                                               0,
                                               0.0f,
                                               sizeof(id->name) - 2,
                                               "");
    UI_but_funcN_set(tab,
                     template_ID_set_property_exec_fn,
                     MEM_new<TemplateID>(__func__, template_id),
                     id,
                     but_func_argN_free<TemplateID>,
                     but_func_argN_copy<TemplateID>);
    UI_but_drag_set_id(tab, id);
    tab->custom_data = (void *)id;
    tab->menu = mt;

    UI_but_drawflag_enable(tab, but_align);
  }

  if (flag & UI_ID_ADD_NEW) {
    const bool editable = RNA_property_editable(&template_id.ptr, template_id.prop);
    uiBut *but;

    if (active_ptr.type) {
      type = active_ptr.type;
    }

    but = template_id_def_new_but(block,
                                  static_cast<const ID *>(active_ptr.data),
                                  template_id,
                                  type,
                                  newop,
                                  editable,
                                  flag & UI_ID_OPEN,
                                  true,
                                  but_height);
    UI_but_drawflag_enable(but, but_align);
  }
}

static void ui_template_id(uiLayout *layout,
                           const bContext *C,
                           PointerRNA *ptr,
                           const StringRefNull propname,
                           const char *newop,
                           const char *openop,
                           const char *unlinkop,
                           /* Only respected by tabs (use_tabs). */
                           const char *menu,
                           const std::optional<StringRef> text,
                           int flag,
                           int prv_rows,
                           int prv_cols,
                           int filter,
                           bool use_tabs,
                           float scale,
                           const bool live_icon,
                           const bool hide_buttons)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning(
        "pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }

  TemplateID template_ui = {};
  template_ui.ptr = *ptr;
  template_ui.prop = prop;
  template_ui.prv_rows = prv_rows;
  template_ui.prv_cols = prv_cols;
  template_ui.scale = scale;

  if ((flag & UI_ID_PIN) == 0) {
    template_ui.filter = filter;
  }
  else {
    template_ui.filter = 0;
  }

  if (newop) {
    flag |= UI_ID_ADD_NEW;
  }
  if (openop) {
    flag |= UI_ID_OPEN;
  }

  StructRNA *type = RNA_property_pointer_type(ptr, prop);
  short idcode = RNA_type_to_ID_code(type);
  template_ui.idcode = idcode;
  template_ui.idlb = which_libbase(CTX_data_main(C), idcode);

  /* create UI elements for this template
   * - template_ID makes a copy of the template data and assigns it to the relevant buttons
   */
  if (template_ui.idlb) {
    if (use_tabs) {
      layout = &layout->row(true);
      template_ID_tabs(C, layout, template_ui, type, flag, newop, menu);
    }
    else {
      layout = &layout->row(true);
      template_ID(C,
                  layout,
                  template_ui,
                  type,
                  flag,
                  newop,
                  openop,
                  unlinkop,
                  text,
                  live_icon,
                  hide_buttons);
    }
  }
}

void uiTemplateID(uiLayout *layout,
                  const bContext *C,
                  PointerRNA *ptr,
                  const StringRefNull propname,
                  const char *newop,
                  const char *openop,
                  const char *unlinkop,
                  int filter,
                  const bool live_icon,
                  const std::optional<StringRef> text)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 text,
                 UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE,
                 0,
                 0,
                 filter,
                 false,
                 1.0f,
                 live_icon,
                 false);
}

void uiTemplateAction(uiLayout *layout,
                      const bContext *C,
                      ID *id,
                      const char *newop,
                      const char *unlinkop,
                      const std::optional<StringRef> text)
{
  if (!id_can_have_animdata(id)) {
    RNA_warning("Cannot show Action selector for non-animatable ID: %s", id->name + 2);
    return;
  }

  PropertyRNA *adt_action_prop = RNA_struct_type_find_property(&RNA_AnimData, "action");
  BLI_assert(adt_action_prop);
  BLI_assert(RNA_property_type(adt_action_prop) == PROP_POINTER);

  /* Construct a pointer with the animated ID as owner, even when `adt` may be `nullptr`.
   * This way it is possible to use this RNA pointer to get/set `adt->action`, as that RNA property
   * has a `getter` & `setter` that only need the owner ID and are null-safe regarding the `adt`
   * itself.
   * FIXME: This is a very dirty hack, would be good to find a way to not rely on typed-but-null
   * PointerRNA.
   */
  AnimData *adt = BKE_animdata_from_id(id);
  PointerRNA adt_ptr = PointerRNA{id, &RNA_AnimData, adt, RNA_id_pointer_create(id)};

  TemplateID template_ui = {};
  template_ui.ptr = adt_ptr;
  template_ui.prop = adt_action_prop;
  template_ui.prv_rows = 0;
  template_ui.prv_cols = 0;
  template_ui.scale = 1.0f;
  template_ui.filter = UI_TEMPLATE_ID_FILTER_ALL;

  int flag = UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE;
  if (newop) {
    flag |= UI_ID_ADD_NEW;
  }

  template_ui.idcode = ID_AC;
  template_ui.idlb = which_libbase(CTX_data_main(C), ID_AC);
  BLI_assert(template_ui.idlb);

  uiLayout *row = &layout->row(true);
  template_ID(
      C, row, template_ui, &RNA_Action, flag, newop, nullptr, unlinkop, text, false, false);
}

void uiTemplateIDBrowse(uiLayout *layout,
                        bContext *C,
                        PointerRNA *ptr,
                        const StringRefNull propname,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        int filter,
                        const char *text)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 text,
                 UI_ID_BROWSE | UI_ID_RENAME,
                 0,
                 0,
                 filter,
                 false,
                 1.0f,
                 false,
                 false);
}

void uiTemplateIDPreview(uiLayout *layout,
                         bContext *C,
                         PointerRNA *ptr,
                         const StringRefNull propname,
                         const char *newop,
                         const char *openop,
                         const char *unlinkop,
                         int rows,
                         int cols,
                         int filter,
                         const bool hide_buttons)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE | UI_ID_PREVIEWS,
                 rows,
                 cols,
                 filter,
                 false,
                 1.0f,
                 false,
                 hide_buttons);
}

void uiTemplateGpencilColorPreview(uiLayout *layout,
                                   bContext *C,
                                   PointerRNA *ptr,
                                   const StringRefNull propname,
                                   int rows,
                                   int cols,
                                   float scale,
                                   int filter)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 nullptr,
                 nullptr,
                 nullptr,
                 nullptr,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_PREVIEWS | UI_ID_DELETE,
                 rows,
                 cols,
                 filter,
                 false,
                 scale < 0.5f ? 0.5f : scale,
                 false,
                 false);
}

void uiTemplateIDTabs(uiLayout *layout,
                      bContext *C,
                      PointerRNA *ptr,
                      const StringRefNull propname,
                      const char *newop,
                      const char *menu,
                      int filter)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 nullptr,
                 nullptr,
                 menu,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_RENAME,
                 0,
                 0,
                 filter,
                 true,
                 1.0f,
                 false,
                 false);
}

void uiTemplateAnyID(uiLayout *layout,
                     PointerRNA *ptr,
                     const StringRefNull propname,
                     const StringRefNull proptypename,
                     const std::optional<StringRef> text)
{
  /* get properties... */
  PropertyRNA *propID = RNA_struct_find_property(ptr, propname.c_str());
  PropertyRNA *propType = RNA_struct_find_property(ptr, proptypename.c_str());

  if (!propID || RNA_property_type(propID) != PROP_POINTER) {
    RNA_warning(
        "pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }
  if (!propType || RNA_property_type(propType) != PROP_ENUM) {
    RNA_warning("pointer-type property not found: %s.%s",
                RNA_struct_identifier(ptr->type),
                proptypename.c_str());
    return;
  }

  /* Start drawing UI Elements using standard defines */

  /* NOTE: split amount here needs to be synced with normal labels */
  uiLayout *split = &layout->split(0.33f, false);

  /* FIRST PART ................................................ */
  uiLayout *row = &split->row(false);

  /* Label - either use the provided text, or will become "ID-Block:" */
  if (text) {
    if (!text->is_empty()) {
      row->label(*text, ICON_NONE);
    }
  }
  else {
    row->label(IFACE_("ID-Block:"), ICON_NONE);
  }

  /* SECOND PART ................................................ */
  row = &split->row(true);

  /* ID-Type Selector - just have a menu of icons */

  /* HACK: special group just for the enum,
   * otherwise we get ugly layout with text included too... */
  uiLayout *sub = &row->row(true);
  sub->alignment_set(blender::ui::LayoutAlign::Left);

  sub->prop(ptr, propType, 0, 0, UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

  /* ID-Block Selector - just use pointer widget... */

  /* HACK: special group to counteract the effects of the previous enum,
   * which now pushes everything too far right. */
  sub = &row->row(true);
  sub->alignment_set(blender::ui::LayoutAlign::Expand);

  sub->prop(ptr, propID, 0, 0, UI_ITEM_NONE, "", ICON_NONE);
}
