/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Implementation of Bone Collection operators and editing API's.
 */

#include <cstring>

#include "ANIM_bone_collections.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "armature_intern.h"

struct wmOperator;

/* ********************************************** */
/* Bone collections */

static bool bone_collection_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return false;
  }

  if (ID_IS_OVERRIDE_LIBRARY(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit bone collections for library overrides");
    return false;
  }

  if (ob->type != OB_ARMATURE) {
    CTX_wm_operator_poll_msg_set(C, "Bone collections can only be edited on an Armature");
    return false;
  }
  return true;
}

static bool active_bone_collection_poll(bContext *C)
{
  if (!bone_collection_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return false;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  if (armature->active_collection == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Armature has no active bone collection, select one first");
    return false;
  }
  return true;
}

static int bone_collection_add_exec(bContext *C, wmOperator * /* op */)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  BoneCollection *bcoll = ANIM_armature_bonecoll_new(armature, nullptr);
  ANIM_armature_bonecoll_active_set(armature, bcoll);

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Bone Collection";
  ot->idname = "ARMATURE_OT_collection_add";
  ot->description = "Add a new bone collection";

  /* api callbacks */
  ot->exec = bone_collection_add_exec;
  ot->poll = bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int bone_collection_remove_exec(bContext *C, wmOperator * /* op */)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* The poll function ensures armature->active_collection is not NULL. */
  bArmature *armature = static_cast<bArmature *>(ob->data);
  ANIM_armature_bonecoll_remove(armature, armature->active_collection);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&armature->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Bone Collection";
  ot->idname = "ARMATURE_OT_collection_remove";
  ot->description = "Remove the active bone collection";

  /* api callbacks */
  ot->exec = bone_collection_remove_exec;
  ot->poll = active_bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int bone_collection_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const int direction = RNA_enum_get(op->ptr, "direction");

  /* Poll function makes sure this is valid. */
  bArmature *armature = static_cast<bArmature *>(ob->data);

  const bool ok = ANIM_armature_bonecoll_move(armature, armature->active_collection, direction);
  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_move(wmOperatorType *ot)
{
  static const EnumPropertyItem bcoll_slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Bone Collection";
  ot->idname = "ARMATURE_OT_collection_move";
  ot->description = "Change position of active Bone Collection in list of Bone collections";

  /* api callbacks */
  ot->exec = bone_collection_move_exec;
  ot->poll = active_bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "direction",
               bcoll_slot_move,
               0,
               "Direction",
               "Direction to move the active Bone Collection towards");
}

typedef enum eMayCreate {
  FAIL_IF_MISSING = 0,
  CREATE_IF_MISSING = 1,
} eMayCreate;

static BoneCollection *get_bonecoll_named_or_active(bContext * /*C*/,
                                                    wmOperator *op,
                                                    Object *ob,
                                                    const eMayCreate may_create)
{
  bArmature *armature = static_cast<bArmature *>(ob->data);

  char bcoll_name[MAX_NAME];
  RNA_string_get(op->ptr, "name", bcoll_name);

  if (bcoll_name[0] == '\0') {
    return armature->active_collection;
  }

  BoneCollection *bcoll = ANIM_armature_bonecoll_get_by_name(armature, bcoll_name);
  if (bcoll) {
    return bcoll;
  }

  switch (may_create) {
    case CREATE_IF_MISSING:
      bcoll = ANIM_armature_bonecoll_new(armature, bcoll_name);
      ANIM_armature_bonecoll_active_set(armature, bcoll);
      return bcoll;
    case FAIL_IF_MISSING:
      WM_reportf(RPT_ERROR, "No bone collection named '%s'", bcoll_name);
      return nullptr;
  }

  return nullptr;
}

using assign_bone_func = bool (*)(BoneCollection *bcoll, Bone *bone);
using assign_ebone_func = bool (*)(BoneCollection *bcoll, EditBone *ebone);

/* The following 3 functions either assign or unassign, depending on the
 * 'assign_bone_func'/'assign_ebone_func' they get passed. */

static void bone_collection_assign_pchans(bContext *C,
                                          Object *ob,
                                          BoneCollection *bcoll,
                                          assign_bone_func assign_func,
                                          bool *made_any_changes,
                                          bool *had_bones_to_assign)
{
  /* TODO: support multi-object pose mode. */
  FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob, pchan) {
    *made_any_changes |= assign_func(bcoll, pchan->bone);
    *had_bones_to_assign = true;
  }
  FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  bArmature *arm = static_cast<bArmature *>(ob->data);
  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT); /* Recreate the draw buffers. */
}

static void bone_collection_assign_editbones(bContext *C,
                                             Object *ob,
                                             BoneCollection *bcoll,
                                             assign_ebone_func assign_func,
                                             bool *made_any_changes,
                                             bool *had_bones_to_assign)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);
  ED_armature_edit_sync_selection(arm->edbo);

  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (!EBONE_EDITABLE(ebone)) {
      continue;
    }
    *made_any_changes |= assign_func(bcoll, ebone);
    *had_bones_to_assign = true;
  }

  ED_armature_edit_sync_selection(arm->edbo);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

/* Returns whether the current mode is actually supported. */
static bool bone_collection_assign_mode_specific(bContext *C,
                                                 Object *ob,
                                                 BoneCollection *bcoll,
                                                 assign_bone_func assign_bone_func,
                                                 assign_ebone_func assign_ebone_func,
                                                 bool *made_any_changes,
                                                 bool *had_bones_to_assign)
{
  switch (CTX_data_mode_enum(C)) {
    case CTX_MODE_POSE: {
      bone_collection_assign_pchans(
          C, ob, bcoll, assign_bone_func, made_any_changes, had_bones_to_assign);
      return true;
    }

    case CTX_MODE_EDIT_ARMATURE: {
      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C), &objects_len);

      for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *ob = objects[ob_index];
        bone_collection_assign_editbones(
            C, ob, bcoll, assign_ebone_func, made_any_changes, had_bones_to_assign);
      }

      MEM_freeN(objects);
      ED_outliner_select_sync_from_edit_bone_tag(C);
      return true;
    }

    default:
      return false;
  }
}

/* Assign selected pchans to the bone collection that the user selects */
static int bone_collection_assign_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob, CREATE_IF_MISSING);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bool made_any_changes = false;
  bool had_bones_to_assign = false;
  const bool mode_is_supported = bone_collection_assign_mode_specific(
      C,
      ob,
      bcoll,
      ANIM_armature_bonecoll_assign,
      ANIM_armature_bonecoll_assign_editbone,
      &made_any_changes,
      &had_bones_to_assign);

  if (!mode_is_supported) {
    WM_report(RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_assign) {
    WM_report(RPT_WARNING, "No bones selected, nothing to assign to bone collection");
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    WM_report(RPT_WARNING, "All selected bones were already part of this collection");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Selected Bones to Collection";
  ot->idname = "ARMATURE_OT_collection_assign";
  ot->description = "Add selected bones to the chosen bone collection";

  /* api callbacks */
  // TODO: reinstate the menu?
  // ot->invoke = bone_collections_menu_invoke;
  ot->exec = bone_collection_assign_exec;
  ot->poll = bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to assign this bone to; empty to assign to the "
                 "active bone collection");
}

static int bone_collection_unassign_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob, FAIL_IF_MISSING);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bool made_any_changes = false;
  bool had_bones_to_unassign = false;
  const bool mode_is_supported = bone_collection_assign_mode_specific(
      C,
      ob,
      bcoll,
      ANIM_armature_bonecoll_unassign,
      ANIM_armature_bonecoll_unassign_editbone,
      &made_any_changes,
      &had_bones_to_unassign);

  if (!mode_is_supported) {
    WM_report(RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_unassign) {
    WM_report(RPT_WARNING, "No bones selected, nothing to unassign from bone collection");
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    WM_report(RPT_WARNING, "None of the selected bones were assigned to this collection");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_unassign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Selected from Bone collections";
  ot->idname = "ARMATURE_OT_collection_unassign";
  ot->description = "Remove selected bones from the active bone collection";

  /* api callbacks */
  ot->exec = bone_collection_unassign_exec;
  ot->poll = bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to unassign this bone from; empty to unassign from "
                 "the active bone collection");
}

static bool editbone_is_member(const EditBone *ebone, const BoneCollection *bcoll)
{
  LISTBASE_FOREACH (BoneCollectionReference *, ref, &ebone->bone_collections) {
    if (ref->bcoll == bcoll) {
      return true;
    }
  }
  return false;
}

static void bone_collection_select(bContext *C,
                                   Object *ob,
                                   BoneCollection *bcoll,
                                   const bool select)
{
  bArmature *armature = static_cast<bArmature *>(ob->data);
  const bool is_editmode = armature->edbo != nullptr;

  if (is_editmode) {
    LISTBASE_FOREACH (EditBone *, ebone, armature->edbo) {
      if (!EBONE_SELECTABLE(armature, ebone)) {
        continue;
      }
      if (!editbone_is_member(ebone, bcoll)) {
        continue;
      }
      ED_armature_ebone_select_set(ebone, select);
    }
  }
  else {
    LISTBASE_FOREACH (BoneCollectionMember *, member, &bcoll->bones) {
      Bone *bone = member->bone;
      if (!ANIM_bone_is_visible(armature, bone)) {
        continue;
      }
      if (bone->flag & BONE_UNSELECTABLE) {
        continue;
      }

      if (select) {
        bone->flag |= BONE_SELECTED;
      }
      else {
        bone->flag &= ~BONE_SELECTED;
      }
    }
  }

  DEG_id_tag_update(&armature->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, ob);

  if (is_editmode) {
    ED_outliner_select_sync_from_edit_bone_tag(C);
  }
  else {
    ED_outliner_select_sync_from_pose_bone_tag(C);
  }
}

static int bone_collection_select_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob, FAIL_IF_MISSING);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bone_collection_select(C, ob, bcoll, true);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Bones of Bone Collection";
  ot->idname = "ARMATURE_OT_collection_select";
  ot->description = "Select bones in active Bone Collection";

  /* api callbacks */
  ot->exec = bone_collection_select_exec;
  ot->poll = bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(
      ot->srna,
      "name",
      nullptr,
      MAX_NAME,
      "Bone Collection",
      "Name of the bone collection to select bones from; empty use the active bone collection");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int bone_collection_deselect_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob, FAIL_IF_MISSING);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bone_collection_select(C, ob, bcoll, false);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Bone Collection";
  ot->idname = "ARMATURE_OT_collection_deselect";
  ot->description = "Deselect bones of active Bone Collection";

  /* api callbacks */
  ot->exec = bone_collection_deselect_exec;
  ot->poll = bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(
      ot->srna,
      "name",
      nullptr,
      MAX_NAME,
      "Bone Collection",
      "Name of the bone collection to deselect bones from; empty use the active bone collection");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* -------------------------- */

using assign_func = bool (*)(BoneCollection *, Bone *);

static int add_or_move_to_collection_exec(bContext *C,
                                          wmOperator *op,
                                          const assign_func assign_func)
{
  Object *obpose = ED_pose_object_from_context(C);
  bArmature *arm = static_cast<bArmature *>(obpose->data);
  const int collection_index = RNA_enum_get(op->ptr, "collection");
  BoneCollection *target_bcoll;

  if (collection_index < 0) {
    char new_collection_name[MAX_NAME];
    RNA_string_get(op->ptr, "new_collection_name", new_collection_name);
    target_bcoll = ANIM_armature_bonecoll_new(arm, new_collection_name);
    BLI_assert_msg(target_bcoll,
                   "It should always be possible to create a new bone collection on an armature");
    ANIM_armature_bonecoll_active_set(arm, target_bcoll);
  }
  else {
    target_bcoll = static_cast<BoneCollection *>(
        BLI_findlink(&arm->collections, collection_index));
    if (target_bcoll == nullptr) {
      WM_reportf(RPT_ERROR,
                 "Bone collection with index %d not found on Armature %s",
                 collection_index,
                 arm->id.name + 2);
      return OPERATOR_CANCELLED;
    }
  }

  FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (obpose, pchan) {
    assign_func(target_bcoll, pchan->bone);
  }
  FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT); /* Recreate the draw buffers. */

  WM_event_add_notifier(C, NC_OBJECT | ND_DATA, obpose);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obpose);
  return OPERATOR_FINISHED;
}

static int move_to_collection_exec(bContext *C, wmOperator *op)
{
  return add_or_move_to_collection_exec(C, op, ANIM_armature_bonecoll_assign_and_move);
}

static int assign_to_collection_exec(bContext *C, wmOperator *op)
{
  return add_or_move_to_collection_exec(C, op, ANIM_armature_bonecoll_assign);
}

static bool move_to_collection_poll(bContext *C)
{
  /* TODO: add outliner support.
  if (CTX_wm_space_outliner(C) != nullptr) {
    return ED_outliner_collections_editor_poll(C);
  }
  */
  // TODO: add armature edit mode support.
  return ED_operator_object_active_local_editable_posemode_exclusive(C);
}

static const EnumPropertyItem *bone_collection_enum_itemf(bContext *C,
                                                          PointerRNA * /*ptr*/,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;

  if (C) {
    if (Object *obpose = ED_pose_object_from_context(C)) {
      bArmature *arm = static_cast<bArmature *>(obpose->data);

      int bcoll_index = 0;
      LISTBASE_FOREACH_INDEX (BoneCollection *, bcoll, &arm->collections, bcoll_index) {
        item_tmp.identifier = bcoll->name;
        item_tmp.name = bcoll->name;
        item_tmp.value = bcoll_index;
        RNA_enum_item_add(&item, &totitem, &item_tmp);
      }

      RNA_enum_item_add_separator(&item, &totitem);
    }
  }

  /* New Collection. */
  item_tmp.identifier = "__NEW__";
  item_tmp.name = "New Collection";
  item_tmp.value = -1;
  RNA_enum_item_add(&item, &totitem, &item_tmp);

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int move_to_collection_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection");
  if (RNA_property_is_set(op->ptr, prop)) {
    const int collection_index = RNA_property_enum_get(op->ptr, prop);
    if (collection_index < 0) {
      return WM_operator_props_dialog_popup(C, op, 200);
    }
    /* Either call move_to_collection_exec() or assign_to_collection_exec(), depending on which
     * operator got invoked. */
    return op->type->exec(C, op);
  }

  uiPopupMenu *pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
  uiItemsEnumO(layout, op->idname, "collection");
  UI_popup_menu_end(C, pup);
  return OPERATOR_INTERFACE;
}

void ARMATURE_OT_move_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move to Collection";
  ot->description = "Move bones to a collection";
  ot->idname = "ARMATURE_OT_move_to_collection";

  /* api callbacks */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* Flags don't include OPTYPE_REGISTER, as the redo panel doesn't make much sense for this
   * operator. The visibility of the RNA properties is determined by the needs of the 'New Catalog'
   * popup, so that a name can be entered. This means that the redo panel would also only show the
   * 'Name' property, without any choice for another collection. */
  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna,
                      "collection",
                      rna_enum_dummy_DEFAULT_items,
                      0,
                      "Collection",
                      "The bone collection to move the selected bones to");
  RNA_def_enum_funcs(prop, bone_collection_enum_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        nullptr,
                        MAX_NAME,
                        "Name",
                        "Name of the newly added bone collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

void ARMATURE_OT_assign_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Assign to Collection";
  ot->description = "Assign bones to a collection";
  ot->idname = "ARMATURE_OT_assign_to_collection";

  /* api callbacks */
  ot->exec = assign_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* Flags don't include OPTYPE_REGISTER, as the redo panel doesn't make much sense for this
   * operator. The visibility of the RNA properties is determined by the needs of the 'New Catalog'
   * popup, so that a name can be entered. This means that the redo panel would also only show the
   * 'Name' property, without any choice for another collection. */
  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna,
                      "collection",
                      rna_enum_dummy_DEFAULT_items,
                      0,
                      "Collection",
                      "The bone collection to move the selected bones to");
  RNA_def_enum_funcs(prop, bone_collection_enum_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        nullptr,
                        MAX_NAME,
                        "Name",
                        "Name of the newly added bone collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

/* ********************************************** */
