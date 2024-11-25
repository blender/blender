/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "ED_keyframing.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"

#include "anim_intern.hh"

/* ************************************************** */
/* KEYING SETS - OPERATORS (for use in UI panels) */
/* These operators are really duplication of existing functionality, but just for completeness,
 * they're here too, and will give the basic data needed...
 */

/* poll callback for adding default KeyingSet */
static bool keyingset_poll_default_add(bContext *C)
{
  /* As long as there's an active Scene, it's fine. */
  return (CTX_data_scene(C) != nullptr);
}

/* Poll callback for editing active KeyingSet. */
static bool keyingset_poll_active_edit(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  if (scene == nullptr) {
    return false;
  }

  /* There must be an active KeyingSet (and KeyingSets). */
  return ((scene->active_keyingset > 0) && (scene->keyingsets.first));
}

/* poll callback for editing active KeyingSet Path */
static bool keyingset_poll_activePath_edit(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  if (scene == nullptr) {
    return false;
  }
  if (scene->active_keyingset <= 0) {
    return false;
  }

  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));

  /* there must be an active KeyingSet and an active path */
  return ((keyingset) && (keyingset->paths.first) && (keyingset->active_path > 0));
}

/* Add a Default (Empty) Keying Set ------------------------- */

static int add_default_keyingset_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  /* Validate flags
   * - absolute KeyingSets should be created by default.
   */
  const eKS_Settings flag = KEYINGSET_ABSOLUTE;

  const eInsertKeyFlags keyingflag = blender::animrig::get_keyframing_flags(scene);

  /* Call the API func, and set the active keyingset index. */
  BKE_keyingset_add(&scene->keyingsets, nullptr, nullptr, flag, keyingflag);

  scene->active_keyingset = BLI_listbase_count(&scene->keyingsets);

  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

  return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_add(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Add Empty Keying Set";
  ot->idname = "ANIM_OT_keying_set_add";
  ot->description = "Add a new (empty) keying set to the active Scene";

  /* Callbacks. */
  ot->exec = add_default_keyingset_exec;
  ot->poll = keyingset_poll_default_add;
}

/* Remove 'Active' Keying Set ------------------------- */

static int remove_active_keyingset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* Verify the Keying Set to use:
   * - use the active one
   * - return error if it doesn't exist
   */
  if (scene->active_keyingset == 0) {
    BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove");
    return OPERATOR_CANCELLED;
  }

  if (scene->active_keyingset < 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot remove built in keying set");
    return OPERATOR_CANCELLED;
  }

  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));

  /* Free KeyingSet's data, then remove it from the scene. */
  BKE_keyingset_free_paths(keyingset);
  BLI_freelinkN(&scene->keyingsets, keyingset);

  /* The active one should now be the previously second-to-last one. */
  scene->active_keyingset--;

  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

  return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_remove(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Remove Active Keying Set";
  ot->idname = "ANIM_OT_keying_set_remove";
  ot->description = "Remove the active keying set";

  /* Callbacks. */
  ot->exec = remove_active_keyingset_exec;
  ot->poll = keyingset_poll_active_edit;
}

/* Add Empty Keying Set Path ------------------------- */

static int add_empty_ks_path_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* Verify the Keying Set to use:
   * - use the active one
   * - return error if it doesn't exist
   */
  if (scene->active_keyingset == 0) {
    BKE_report(op->reports, RPT_ERROR, "No active Keying Set to add empty path to");
    return OPERATOR_CANCELLED;
  }

  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));

  /* Don't use the API method for this, since that checks on values... */
  KS_Path *keyingset_path = static_cast<KS_Path *>(
      MEM_callocN(sizeof(KS_Path), "KeyingSetPath Empty"));
  BLI_addtail(&keyingset->paths, keyingset_path);
  keyingset->active_path = BLI_listbase_count(&keyingset->paths);

  keyingset_path->groupmode = KSP_GROUP_KSNAME; /* XXX? */
  keyingset_path->idtype = ID_OB;
  keyingset_path->flag = KSP_FLAG_WHOLE_ARRAY;

  return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_path_add(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Add Empty Keying Set Path";
  ot->idname = "ANIM_OT_keying_set_path_add";
  ot->description = "Add empty path to active keying set";

  /* Callbacks. */
  ot->exec = add_empty_ks_path_exec;
  ot->poll = keyingset_poll_active_edit;
}

/* Remove Active Keying Set Path ------------------------- */

static int remove_active_ks_path_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));

  /* If there is a KeyingSet, find the nominated path to remove. */
  if (!keyingset) {
    BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove a path from");
    return OPERATOR_CANCELLED;
  }

  KS_Path *keyingset_path = static_cast<KS_Path *>(
      BLI_findlink(&keyingset->paths, keyingset->active_path - 1));
  if (!keyingset_path) {
    BKE_report(op->reports, RPT_ERROR, "No active Keying Set path to remove");
    return OPERATOR_CANCELLED;
  }

  /* Remove the active path from the KeyingSet. */
  BKE_keyingset_free_path(keyingset, keyingset_path);

  /* The active path should now be the previously second-to-last active one. */
  keyingset->active_path--;

  return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_path_remove(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Remove Active Keying Set Path";
  ot->idname = "ANIM_OT_keying_set_path_remove";
  ot->description = "Remove active Path from active keying set";

  /* Callbacks. */
  ot->exec = remove_active_ks_path_exec;
  ot->poll = keyingset_poll_activePath_edit;
}

/* ************************************************** */
/* KEYING SETS - OPERATORS (for use in UI menus) */

/* Add to KeyingSet Button Operator ------------------------ */

static int add_keyingset_button_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = nullptr;
  PointerRNA ptr = {};
  int index = 0, pflag = 0;

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* Pass event on if no active button found. */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  /* Verify the Keying Set to use:
   * - use the active one for now (more control over this can be added later)
   * - add a new one if it doesn't exist
   */
  KeyingSet *keyingset = nullptr;
  Scene *scene = CTX_data_scene(C);
  if (scene->active_keyingset == 0) {
    /* Validate flags
     * - absolute KeyingSets should be created by default
     */
    const eKS_Settings flag = KEYINGSET_ABSOLUTE;

    const eInsertKeyFlags keyingflag = blender::animrig::get_keyframing_flags(scene);

    /* Call the API func, and set the active keyingset index. */
    keyingset = BKE_keyingset_add(
        &scene->keyingsets, "ButtonKeyingSet", "Button Keying Set", flag, keyingflag);

    scene->active_keyingset = BLI_listbase_count(&scene->keyingsets);
  }
  else if (scene->active_keyingset < 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot add property to built in keying set");
    return OPERATOR_CANCELLED;
  }
  else {
    keyingset = static_cast<KeyingSet *>(
        BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));
  }

  /* Check if property is able to be added. */
  const bool all = RNA_boolean_get(op->ptr, "all");
  bool changed = false;
  if (ptr.owner_id && ptr.data && prop && RNA_property_anim_editable(&ptr, prop)) {
    if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
      if (all) {
        pflag |= KSP_FLAG_WHOLE_ARRAY;

        /* We need to set the index for this to 0, even though it may break in some cases, this is
         * necessary if we want the entire array for most cases to get included without the user
         * having to worry about where they clicked.
         */
        index = 0;
      }

      /* Add path to this setting. */
      BKE_keyingset_add_path(
          keyingset, ptr.owner_id, nullptr, path->c_str(), index, pflag, KSP_GROUP_KSNAME);
      keyingset->active_path = BLI_listbase_count(&keyingset->paths);
      changed = true;
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

    /* Show notification/report header, so that users notice that something changed. */
    BKE_reportf(op->reports, RPT_INFO, "Property added to Keying Set: '%s'", keyingset->name);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyingset_button_add(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Add to Keying Set";
  ot->idname = "ANIM_OT_keyingset_button_add";
  ot->description = "Add current UI-active property to current keying set";

  /* Callbacks. */
  ot->exec = add_keyingset_button_exec;
  // op->poll = ???

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_boolean(ot->srna, "all", true, "All", "Add all elements of the array to a Keying Set");
}

/* Remove from KeyingSet Button Operator ------------------------ */

static int remove_keyingset_button_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = nullptr;
  PointerRNA ptr = {};
  int index = 0;

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* Pass event on if no active button found. */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  /* Verify the Keying Set to use:
   * - use the active one for now (more control over this can be added later)
   * - return error if it doesn't exist
   */
  Scene *scene = CTX_data_scene(C);
  if (scene->active_keyingset == 0) {
    BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove property from");
    return OPERATOR_CANCELLED;
  }

  if (scene->active_keyingset < 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot remove property from built in keying set");
    return OPERATOR_CANCELLED;
  }

  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));

  bool changed = false;
  if (ptr.owner_id && ptr.data && prop) {
    if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
      /* Try to find a path matching this description. */
      KS_Path *keyingset_path = BKE_keyingset_find_path(
          keyingset, ptr.owner_id, keyingset->name, path->c_str(), index, KSP_GROUP_KSNAME);

      if (keyingset_path) {
        BKE_keyingset_free_path(keyingset, keyingset_path);
        changed = true;
      }
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

    /* Show warning. */
    BKE_report(op->reports, RPT_INFO, "Property removed from keying set");
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyingset_button_remove(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Remove from Keying Set";
  ot->idname = "ANIM_OT_keyingset_button_remove";
  ot->description = "Remove current UI-active property from current keying set";

  /* Callbacks. */
  ot->exec = remove_keyingset_button_exec;
  // op->poll = ???

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************************************* */

/* Change Active KeyingSet Operator ------------------------ */
/* This operator checks if a menu should be shown
 * for choosing the KeyingSet to make the active one. */

static int keyingset_active_menu_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  uiPopupMenu *pup;
  uiLayout *layout;

  /* Call the menu, which will call this operator again, hence the canceled. */
  pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
  layout = UI_popup_menu_layout(pup);
  uiItemsEnumO(layout, "ANIM_OT_keying_set_active_set", "type");
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int keyingset_active_menu_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const int type = RNA_enum_get(op->ptr, "type");

  /* If type == 0, it will deselect any active keying set. */
  scene->active_keyingset = type;

  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

  return OPERATOR_FINISHED;
}

/* Build the enum for all keyingsets except the active keyingset. */
static void build_keyingset_enum(bContext *C, EnumPropertyItem **item, int *totitem, bool *r_free)
{
  /* user-defined Keying Sets
   * - these are listed in the order in which they were defined for the active scene
   */
  EnumPropertyItem item_tmp = {0};

  Scene *scene = CTX_data_scene(C);
  KeyingSet *keyingset;
  int enum_index = 1;
  if (scene->keyingsets.first) {
    for (keyingset = static_cast<KeyingSet *>(scene->keyingsets.first); keyingset;
         keyingset = keyingset->next, enum_index++)
    {
      if (ANIM_keyingset_context_ok_poll(C, keyingset)) {
        item_tmp.identifier = keyingset->idname;
        item_tmp.name = keyingset->name;
        item_tmp.description = keyingset->description;
        item_tmp.value = enum_index;
        RNA_enum_item_add(item, totitem, &item_tmp);
      }
    }

    RNA_enum_item_add_separator(item, totitem);
  }

  /* Builtin Keying Sets. */
  enum_index = -1;
  for (keyingset = static_cast<KeyingSet *>(builtin_keyingsets.first); keyingset;
       keyingset = keyingset->next, enum_index--)
  {
    /* Only show KeyingSet if context is suitable. */
    if (ANIM_keyingset_context_ok_poll(C, keyingset)) {
      item_tmp.identifier = keyingset->idname;
      item_tmp.name = keyingset->name;
      item_tmp.description = keyingset->description;
      item_tmp.value = enum_index;
      RNA_enum_item_add(item, totitem, &item_tmp);
    }
  }

  RNA_enum_item_end(item, totitem);
  *r_free = true;
}

static const EnumPropertyItem *keyingset_set_active_enum_itemf(bContext *C,
                                                               PointerRNA * /*ptr*/,
                                                               PropertyRNA * /*prop*/,
                                                               bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Active Keying Set.
   * - only include entry if it exists
   */
  Scene *scene = CTX_data_scene(C);
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  if (scene->active_keyingset) {
    /* Active Keying Set. */
    item_tmp.identifier = "__ACTIVE__";
    item_tmp.name = "Clear Active Keying Set";
    item_tmp.value = 0;
    RNA_enum_item_add(&item, &totitem, &item_tmp);

    RNA_enum_item_add_separator(&item, &totitem);
  }

  build_keyingset_enum(C, &item, &totitem, r_free);

  return item;
}

void ANIM_OT_keying_set_active_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Set Active Keying Set";
  ot->idname = "ANIM_OT_keying_set_active_set";
  ot->description = "Set a new active keying set";

  /* Callbacks. */
  ot->invoke = keyingset_active_menu_invoke;
  ot->exec = keyingset_active_menu_exec;
  ot->poll = ED_operator_areaactive;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Keyingset to use (dynamic enum). */
  prop = RNA_def_enum(
      ot->srna, "type", rna_enum_dummy_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, keyingset_set_active_enum_itemf);
}

/* ******************************************* */
/* KEYING SETS API (for UI) */

/* Getters for Active/Indices ----------------------------- */

int ANIM_scene_get_keyingset_index(Scene *scene, KeyingSet *keyingset)
{
  int index;

  /* If no KeyingSet provided, have none. */
  if (keyingset == nullptr) {
    return 0;
  }

  /* Check if the KeyingSet exists in scene list. */
  if (scene) {
    /* Get index and if valid, return
     * - (absolute) Scene KeyingSets are from (>= 1)
     */
    index = BLI_findindex(&scene->keyingsets, keyingset);
    if (index != -1) {
      return (index + 1);
    }
  }

  /* Still here, so try built-ins list too:
   * - Built-ins are from (<= -1).
   * - None/Invalid is (= 0).
   */
  index = BLI_findindex(&builtin_keyingsets, keyingset);
  if (index != -1) {
    return -(index + 1);
  }
  return 0;
}

static void anim_keyingset_visit_for_search_impl(
    const bContext *C,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn,
    const bool use_poll)
{
  /* Poll requires context. */
  if (use_poll && (C == nullptr)) {
    return;
  }

  Scene *scene = C ? CTX_data_scene(C) : nullptr;

  /* Active Keying Set. */
  if (!use_poll || (scene && scene->active_keyingset)) {
    StringPropertySearchVisitParams visit_params{};
    visit_params.text = "__ACTIVE__";
    visit_params.info = "Active Keying Set";
    visit_fn(visit_params);
  }

  /* User-defined Keying Sets. */
  if (scene && scene->keyingsets.first) {
    LISTBASE_FOREACH (KeyingSet *, keyingset, &scene->keyingsets) {
      if (use_poll && !ANIM_keyingset_context_ok_poll((bContext *)C, keyingset)) {
        continue;
      }
      StringPropertySearchVisitParams visit_params{};
      visit_params.text = keyingset->idname;
      visit_params.info = keyingset->name;
      visit_fn(visit_params);
    }
  }

  /* Builtin Keying Sets. */
  LISTBASE_FOREACH (KeyingSet *, keyingset, &builtin_keyingsets) {
    if (use_poll && !ANIM_keyingset_context_ok_poll((bContext *)C, keyingset)) {
      continue;
    }
    StringPropertySearchVisitParams visit_params{};
    visit_params.text = keyingset->idname;
    visit_params.info = keyingset->name;
    visit_fn(visit_params);
  }
}

void ANIM_keyingset_visit_for_search(
    const bContext *C,
    PointerRNA * /*ptr*/,
    PropertyRNA * /*prop*/,
    const char * /*edit_text*/,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  anim_keyingset_visit_for_search_impl(C, visit_fn, false);
}

void ANIM_keyingset_visit_for_search_no_poll(
    const bContext *C,
    PointerRNA * /*ptr*/,
    PropertyRNA * /*prop*/,
    const char * /*edit_text*/,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  anim_keyingset_visit_for_search_impl(C, visit_fn, true);
}

/* Menu of All Keying Sets ----------------------------- */

const EnumPropertyItem *ANIM_keying_sets_enum_itemf(bContext *C,
                                                    PointerRNA * /*ptr*/,
                                                    PropertyRNA * /*prop*/,
                                                    bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Active Keying Set
   * - only include entry if it exists
   */
  Scene *scene = CTX_data_scene(C);
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  if (scene->active_keyingset) {
    /* Active Keying Set. */
    item_tmp.identifier = "__ACTIVE__";
    item_tmp.name = "Active Keying Set";
    item_tmp.value = 0;
    RNA_enum_item_add(&item, &totitem, &item_tmp);

    RNA_enum_item_add_separator(&item, &totitem);
  }

  build_keyingset_enum(C, &item, &totitem, r_free);

  return item;
}

KeyingSet *ANIM_keyingset_get_from_enum_type(Scene *scene, int type)
{
  if (type == 0) {
    type = scene->active_keyingset;
  }

  if (type > 0) {
    return static_cast<KeyingSet *>(BLI_findlink(&scene->keyingsets, type - 1));
  }
  else {
    return static_cast<KeyingSet *>(BLI_findlink(&builtin_keyingsets, -type - 1));
  }
  return nullptr;
}

KeyingSet *ANIM_keyingset_get_from_idname(Scene *scene, const char *idname)
{
  KeyingSet *keyingset = static_cast<KeyingSet *>(
      BLI_findstring(&scene->keyingsets, idname, offsetof(KeyingSet, idname)));
  if (keyingset == nullptr) {
    keyingset = static_cast<KeyingSet *>(
        BLI_findstring(&builtin_keyingsets, idname, offsetof(KeyingSet, idname)));
  }
  return keyingset;
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* Polling API ----------------------------------------------- */

bool ANIM_keyingset_context_ok_poll(bContext *C, KeyingSet *keyingset)
{
  if (keyingset->flag & KEYINGSET_ABSOLUTE) {
    return true;
  }

  KeyingSetInfo *keyingset_info = blender::animrig::keyingset_info_find_name(keyingset->typeinfo);

  /* Get the associated 'type info' for this KeyingSet. */
  if (keyingset_info == nullptr) {
    return false;
  }
  /* TODO: check for missing callbacks! */

  /* Check if it can be used in the current context. */
  return keyingset_info->poll(keyingset_info, C);
}
