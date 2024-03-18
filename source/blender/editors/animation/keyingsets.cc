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
#include "BKE_report.h"

#include "DEG_depsgraph.hh"

#include "ANIM_keyframing.hh"
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

#include "anim_intern.h"

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

  const eInsertKeyFlags keyingflag = ANIM_get_keyframing_flags(scene);

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
  PointerRNA ptr = {nullptr};
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

    const eInsertKeyFlags keyingflag = ANIM_get_keyframing_flags(scene);

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
  PointerRNA ptr = {nullptr};
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
/* REGISTERED KEYING SETS */

/* Keying Set Type Info declarations. */
static ListBase keyingset_type_infos = {nullptr, nullptr};

ListBase builtin_keyingsets = {nullptr, nullptr};

/* --------------- */

KeyingSetInfo *ANIM_keyingset_info_find_name(const char name[])
{
  if ((name == nullptr) || (name[0] == 0)) {
    return nullptr;
  }

  /* Search by comparing names. */
  return static_cast<KeyingSetInfo *>(
      BLI_findstring(&keyingset_type_infos, name, offsetof(KeyingSetInfo, idname)));
}

KeyingSet *ANIM_builtin_keyingset_get_named(const char name[])
{
  if (name[0] == 0) {
    return nullptr;
  }

  /* Loop over KeyingSets checking names. */
  LISTBASE_FOREACH (KeyingSet *, keyingset, &builtin_keyingsets) {
    if (STREQ(name, keyingset->idname)) {
      return keyingset;
    }
  }

/* Complain about missing keying sets on debug builds. */
#ifndef NDEBUG
  printf("%s: '%s' not found\n", __func__, name);
#endif

  /* no matches found */
  return nullptr;
}

/* --------------- */

void ANIM_keyingset_info_register(KeyingSetInfo *keyingset_info)
{
  /* Create a new KeyingSet
   * - inherit name and keyframing settings from the typeinfo
   */
  KeyingSet *keyingset = BKE_keyingset_add(&builtin_keyingsets,
                                           keyingset_info->idname,
                                           keyingset_info->name,
                                           1,
                                           keyingset_info->keyingflag);

  /* Link this KeyingSet with its typeinfo. */
  memcpy(&keyingset->typeinfo, keyingset_info->idname, sizeof(keyingset->typeinfo));

  /* Copy description. */
  STRNCPY(keyingset->description, keyingset_info->description);

  /* Add type-info to the list. */
  BLI_addtail(&keyingset_type_infos, keyingset_info);
}

void ANIM_keyingset_info_unregister(Main *bmain, KeyingSetInfo *keyingset_info)
{
  /* Find relevant builtin KeyingSets which use this, and remove them. */
  /* TODO: this isn't done now, since unregister is really only used at the moment when we
   * reload the scripts, which kind of defeats the purpose of "builtin"? */
  LISTBASE_FOREACH_MUTABLE (KeyingSet *, keyingset, &builtin_keyingsets) {
    /* Remove if matching typeinfo name. */
    if (!STREQ(keyingset->typeinfo, keyingset_info->idname)) {
      continue;
    }
    Scene *scene;
    BKE_keyingset_free_paths(keyingset);
    BLI_remlink(&builtin_keyingsets, keyingset);

    for (scene = static_cast<Scene *>(bmain->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      BLI_remlink_safe(&scene->keyingsets, keyingset);
    }

    MEM_freeN(keyingset);
  }

  BLI_freelinkN(&keyingset_type_infos, keyingset_info);
}

void ANIM_keyingset_infos_exit()
{
  /* Free type infos. */
  LISTBASE_FOREACH_MUTABLE (KeyingSetInfo *, keyingset_info, &keyingset_type_infos) {
    /* Free extra RNA data, and remove from list. */
    if (keyingset_info->rna_ext.free) {
      keyingset_info->rna_ext.free(keyingset_info->rna_ext.data);
    }
    BLI_freelinkN(&keyingset_type_infos, keyingset_info);
  }

  BKE_keyingsets_free(&builtin_keyingsets);
}

bool ANIM_keyingset_find_id(KeyingSet *keyingset, ID *id)
{
  if (ELEM(nullptr, keyingset, id)) {
    return false;
  }

  return BLI_findptr(&keyingset->paths, id, offsetof(KS_Path, id)) != nullptr;
}

/* ******************************************* */
/* KEYING SETS API (for UI) */

/* Getters for Active/Indices ----------------------------- */

KeyingSet *ANIM_scene_get_active_keyingset(const Scene *scene)
{
  /* If no scene, we've got no hope of finding the Keying Set. */
  if (scene == nullptr) {
    return nullptr;
  }

  /* Currently, there are several possibilities here:
   * -   0: no active keying set
   * - > 0: one of the user-defined Keying Sets, but indices start from 0 (hence the -1)
   * - < 0: a builtin keying set
   */
  if (scene->active_keyingset > 0) {
    return static_cast<KeyingSet *>(BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));
  }
  return static_cast<KeyingSet *>(
      BLI_findlink(&builtin_keyingsets, (-scene->active_keyingset) - 1));
}

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

KeyingSet *ANIM_get_keyingset_for_autokeying(const Scene *scene, const char *transformKSName)
{
  /* Get KeyingSet to use
   * - use the active KeyingSet if defined (and user wants to use it for all autokeying),
   *   or otherwise key transforms only
   */
  if (blender::animrig::is_keying_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) &&
      (scene->active_keyingset))
  {
    return ANIM_scene_get_active_keyingset(scene);
  }

  if (blender::animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
    return ANIM_builtin_keyingset_get_named(ANIM_KS_AVAILABLE_ID);
  }

  return ANIM_builtin_keyingset_get_named(transformKSName);
}

static void anim_keyingset_visit_for_search_impl(const bContext *C,
                                                 StringPropertySearchVisitFunc visit_fn,
                                                 void *visit_user_data,
                                                 const bool use_poll)
{
  /* Poll requires context. */
  if (use_poll && (C == nullptr)) {
    return;
  }

  Scene *scene = C ? CTX_data_scene(C) : nullptr;

  /* Active Keying Set. */
  if (!use_poll || (scene && scene->active_keyingset)) {
    StringPropertySearchVisitParams visit_params = {nullptr};
    visit_params.text = "__ACTIVE__";
    visit_params.info = "Active Keying Set";
    visit_fn(visit_user_data, &visit_params);
  }

  /* User-defined Keying Sets. */
  if (scene && scene->keyingsets.first) {
    LISTBASE_FOREACH (KeyingSet *, keyingset, &scene->keyingsets) {
      if (use_poll && !ANIM_keyingset_context_ok_poll((bContext *)C, keyingset)) {
        continue;
      }
      StringPropertySearchVisitParams visit_params = {nullptr};
      visit_params.text = keyingset->idname;
      visit_params.info = keyingset->name;
      visit_fn(visit_user_data, &visit_params);
    }
  }

  /* Builtin Keying Sets. */
  LISTBASE_FOREACH (KeyingSet *, keyingset, &builtin_keyingsets) {
    if (use_poll && !ANIM_keyingset_context_ok_poll((bContext *)C, keyingset)) {
      continue;
    }
    StringPropertySearchVisitParams visit_params = {nullptr};
    visit_params.text = keyingset->idname;
    visit_params.info = keyingset->name;
    visit_fn(visit_user_data, &visit_params);
  }
}

void ANIM_keyingset_visit_for_search(const bContext *C,
                                     PointerRNA * /*ptr*/,
                                     PropertyRNA * /*prop*/,
                                     const char * /*edit_text*/,
                                     StringPropertySearchVisitFunc visit_fn,
                                     void *visit_user_data)
{
  anim_keyingset_visit_for_search_impl(C, visit_fn, visit_user_data, false);
}

void ANIM_keyingset_visit_for_search_no_poll(const bContext *C,
                                             PointerRNA * /*ptr*/,
                                             PropertyRNA * /*prop*/,
                                             const char * /*edit_text*/,
                                             StringPropertySearchVisitFunc visit_fn,
                                             void *visit_user_data)
{
  anim_keyingset_visit_for_search_impl(C, visit_fn, visit_user_data, true);
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

  KeyingSetInfo *keyingset_info = ANIM_keyingset_info_find_name(keyingset->typeinfo);

  /* Get the associated 'type info' for this KeyingSet. */
  if (keyingset_info == nullptr) {
    return false;
  }
  /* TODO: check for missing callbacks! */

  /* Check if it can be used in the current context. */
  return keyingset_info->poll(keyingset_info, C);
}

/* Special 'Overrides' Iterator for Relative KeyingSets ------ */

/* Iterator used for overriding the behavior of iterators defined for
 * relative Keying Sets, with the main usage of this being operators
 * requiring Auto Keyframing. Internal Use Only!
 */
static void RKS_ITER_overrides_list(KeyingSetInfo *keyingset_info,
                                    bContext *C,
                                    KeyingSet *keyingset,
                                    blender::Vector<PointerRNA> &sources)
{
  for (PointerRNA ptr : sources) {
    /* Run generate callback on this data. */
    keyingset_info->generate(keyingset_info, C, keyingset, &ptr);
  }
}

void ANIM_relative_keyingset_add_source(blender::Vector<PointerRNA> &sources,
                                        ID *id,
                                        StructRNA *srna,
                                        void *data)
{
  if (ELEM(nullptr, srna, data, id)) {
    return;
  }
  sources.append(RNA_pointer_create(id, srna, data));
}

void ANIM_relative_keyingset_add_source(blender::Vector<PointerRNA> &sources, ID *id)
{
  if (id == nullptr) {
    return;
  }
  sources.append(RNA_id_pointer_create(id));
}

/* KeyingSet Operations (Insert/Delete Keyframes) ------------ */

eModifyKey_Returns ANIM_validate_keyingset(bContext *C,
                                           blender::Vector<PointerRNA> *sources,
                                           KeyingSet *keyingset)
{
  if (keyingset == nullptr) {
    return MODIFYKEY_SUCCESS;
  }

  /* If relative Keying Sets, poll and build up the paths. */
  if (keyingset->flag & KEYINGSET_ABSOLUTE) {
    return MODIFYKEY_SUCCESS;
  }

  KeyingSetInfo *keyingset_info = ANIM_keyingset_info_find_name(keyingset->typeinfo);

  /* Clear all existing paths
   * NOTE: BKE_keyingset_free_paths() frees all of the paths for the KeyingSet, but not the set
   * itself.
   */
  BKE_keyingset_free_paths(keyingset);

  /* Get the associated 'type info' for this KeyingSet. */
  if (keyingset_info == nullptr) {
    return MODIFYKEY_MISSING_TYPEINFO;
  }
  /* TODO: check for missing callbacks! */

  /* Check if it can be used in the current context. */
  if (!keyingset_info->poll(keyingset_info, C)) {
    /* Poll callback tells us that KeyingSet is useless in current context. */
    /* FIXME: the poll callback needs to give us more info why. */
    return MODIFYKEY_INVALID_CONTEXT;
  }

  /* If a list of data sources are provided, run a special iterator over them,
   * otherwise, just continue per normal.
   */
  if (sources != nullptr) {
    RKS_ITER_overrides_list(keyingset_info, C, keyingset, *sources);
  }
  else {
    keyingset_info->iter(keyingset_info, C, keyingset);
  }

  /* If we don't have any paths now, then this still qualifies as invalid context. */
  /* FIXME: we need some error conditions (to be retrieved from the iterator why this failed!)
   */
  if (BLI_listbase_is_empty(&keyingset->paths)) {
    return MODIFYKEY_INVALID_CONTEXT;
  }

  return MODIFYKEY_SUCCESS;
}

/* Determine which keying flags apply based on the override flags. */
static eInsertKeyFlags keyingset_apply_keying_flags(const eInsertKeyFlags base_flags,
                                                    const eInsertKeyFlags overrides,
                                                    const eInsertKeyFlags own_flags)
{
  /* Pass through all flags by default (i.e. even not explicitly listed ones). */
  eInsertKeyFlags result = base_flags;

/* The logic for whether a keying flag applies is as follows:
 * - If the flag in question is set in "overrides", that means that the
 *   status of that flag in "own_flags" is used
 * - If however the flag isn't set, then its value in "base_flags" is used
 *   instead (i.e. no override)
 */
#define APPLY_KEYINGFLAG_OVERRIDE(kflag) \
  if (overrides & kflag) { \
    result &= ~kflag; \
    result |= (own_flags & kflag); \
  }

  /* Apply the flags one by one...
   * (See rna_def_common_keying_flags() for the supported flags)
   */
  APPLY_KEYINGFLAG_OVERRIDE(INSERTKEY_NEEDED)
  APPLY_KEYINGFLAG_OVERRIDE(INSERTKEY_MATRIX)

#undef APPLY_KEYINGFLAG_OVERRIDE

  return result;
}

static int insert_key_to_keying_set_path(bContext *C,
                                         KS_Path *keyingset_path,
                                         KeyingSet *keyingset,
                                         const eInsertKeyFlags insert_key_flags,
                                         const eModifyKey_Modes mode,
                                         const float frame)
{
  /* Since keying settings can be defined on the paths too,
   * apply the settings for this path first. */
  const eInsertKeyFlags path_insert_key_flags = keyingset_apply_keying_flags(
      insert_key_flags,
      eInsertKeyFlags(keyingset_path->keyingoverride),
      eInsertKeyFlags(keyingset_path->keyingflag));

  const char *groupname = nullptr;
  /* Get pointer to name of group to add channels to. */
  if (keyingset_path->groupmode == KSP_GROUP_NONE) {
    groupname = nullptr;
  }
  else if (keyingset_path->groupmode == KSP_GROUP_KSNAME) {
    groupname = keyingset->name;
  }
  else {
    groupname = keyingset_path->group;
  }

  /* Init - array_length should be greater than array_index so that
   * normal non-array entries get keyframed correctly.
   */
  int array_index = keyingset_path->array_index;
  int array_length = array_index;

  /* Get length of array if whole array option is enabled. */
  if (keyingset_path->flag & KSP_FLAG_WHOLE_ARRAY) {
    PointerRNA ptr;
    PropertyRNA *prop;

    PointerRNA id_ptr = RNA_id_pointer_create(keyingset_path->id);
    if (RNA_path_resolve_property(&id_ptr, keyingset_path->rna_path, &ptr, &prop)) {
      array_length = RNA_property_array_length(&ptr, prop);
      /* Start from start of array, instead of the previously specified index - #48020 */
      array_index = 0;
    }
  }

  /* We should do at least one step. */
  if (array_length == array_index) {
    array_length++;
  }

  Main *bmain = CTX_data_main(C);
  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  const eBezTriple_KeyframeType keytype = eBezTriple_KeyframeType(
      scene->toolsettings->keyframe_type);
  /* For each possible index, perform operation
   * - Assume that array-length is greater than index. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    frame);
  int keyed_channels = 0;
  for (; array_index < array_length; array_index++) {
    if (mode == MODIFYKEY_MODE_INSERT) {
      keyed_channels += blender::animrig::insert_keyframe(bmain,
                                                          reports,
                                                          keyingset_path->id,
                                                          nullptr,
                                                          groupname,
                                                          keyingset_path->rna_path,
                                                          array_index,
                                                          &anim_eval_context,
                                                          keytype,
                                                          path_insert_key_flags);
    }
    else if (mode == MODIFYKEY_MODE_DELETE) {
      keyed_channels += blender::animrig::delete_keyframe(bmain,
                                                          reports,
                                                          keyingset_path->id,
                                                          nullptr,
                                                          keyingset_path->rna_path,
                                                          array_index,
                                                          frame);
    }
  }

  switch (GS(keyingset_path->id->name)) {
    case ID_OB: /* Object (or Object-Related) Keyframes */
    {
      Object *ob = (Object *)keyingset_path->id;

      /* XXX: only object transforms? */
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      break;
    }
    default:
      DEG_id_tag_update(keyingset_path->id, ID_RECALC_ANIMATION_NO_FLUSH);
      break;
  }

  /* Send notifiers for updates (this doesn't require context to work!). */
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return keyed_channels;
}

int ANIM_apply_keyingset(bContext *C,
                         blender::Vector<PointerRNA> *sources,
                         KeyingSet *keyingset,
                         short mode,
                         float cfra)
{
  if (keyingset == nullptr) {
    return 0;
  }

  Scene *scene = CTX_data_scene(C);
  const eInsertKeyFlags base_kflags = ANIM_get_keyframing_flags(scene);
  eInsertKeyFlags kflag = INSERTKEY_NOFLAGS;
  if (mode == MODIFYKEY_MODE_INSERT) {
    /* use context settings as base */
    kflag = keyingset_apply_keying_flags(base_kflags,
                                         eInsertKeyFlags(keyingset->keyingoverride),
                                         eInsertKeyFlags(keyingset->keyingflag));
  }
  else if (mode == MODIFYKEY_MODE_DELETE) {
    kflag = INSERTKEY_NOFLAGS;
  }

  /* If relative Keying Sets, poll and build up the paths. */
  {
    const eModifyKey_Returns error = ANIM_validate_keyingset(C, sources, keyingset);
    if (error != MODIFYKEY_SUCCESS) {
      BLI_assert(error < 0);
      return error;
    }
  }

  ReportList *reports = CTX_wm_reports(C);
  int keyed_channels = 0;

  /* Apply the paths as specified in the KeyingSet now. */
  LISTBASE_FOREACH (KS_Path *, keyingset_path, &keyingset->paths) {
    /* Skip path if no ID pointer is specified. */
    if (keyingset_path->id == nullptr) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Skipping path in keying set, as it has no ID (KS = '%s', path = '%s[%d]')",
                  keyingset->name,
                  keyingset_path->rna_path,
                  keyingset_path->array_index);
      continue;
    }

    keyed_channels += insert_key_to_keying_set_path(
        C, keyingset_path, keyingset, kflag, eModifyKey_Modes(mode), cfra);
  }

  /* Return the number of channels successfully affected. */
  BLI_assert(keyed_channels >= 0);
  return keyed_channels;
}

/* ************************************************** */
