/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Implementation of Bone Collection operators and editing API's.
 */

#include <cstring>

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"

#include "BKE_action.hh"
#include "BKE_context.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "armature_intern.hh"

struct wmOperator;

/* ********************************************** */
/* Bone collections */

static bool bone_collection_add_poll(bContext *C)
{
  bArmature *armature = ED_armature_context(C);
  if (armature == nullptr) {
    return false;
  }

  if (!ID_IS_EDITABLE(&armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot add bone collections to a linked Armature without an "
                                 "override on the Armature Data");
    return false;
  }

  if (BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot add bone collections to a linked Armature with a system "
                                 "override; explicitly create an override on the Armature Data");
    return false;
  }

  return true;
}

/** Allow edits of local bone collection only (full local or local override). */
static bool active_bone_collection_poll(bContext *C)
{
  bArmature *armature = ED_armature_context(C);
  if (armature == nullptr) {
    return false;
  }

  if (BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot update a linked Armature with a system override; "
                                 "explicitly create an override on the Armature Data");
    return false;
  }

  BoneCollection *bcoll = armature->runtime.active_collection;
  if (bcoll == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Armature has no active bone collection, select one first");
    return false;
  }

  if (!ANIM_armature_bonecoll_is_editable(armature, bcoll)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit bone collections that are linked from another blend file");
    return false;
  }
  return true;
}

static wmOperatorStatus bone_collection_add_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::animrig;

  bArmature *armature = ED_armature_context(C);

  /* If there is an active bone collection, create the new one as a sibling. */
  const int parent_index = armature_bonecoll_find_parent_index(
      armature, armature->runtime.active_collection_index);

  BoneCollection *bcoll = ANIM_armature_bonecoll_new(armature, nullptr, parent_index);

  if (armature->runtime.active_collection) {
    const int active_child_index = armature_bonecoll_child_number_find(
        armature, armature->runtime.active_collection);
    armature_bonecoll_child_number_set(armature, bcoll, active_child_index + 1);
  }

  ANIM_armature_bonecoll_active_set(armature, bcoll);
  /* TODO: ensure the ancestors of the new bone collection are all expanded. */

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Bone Collection";
  ot->idname = "ARMATURE_OT_collection_add";
  ot->description = "Add a new bone collection";

  /* API callbacks. */
  ot->exec = bone_collection_add_exec;
  ot->poll = bone_collection_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus bone_collection_remove_exec(bContext *C, wmOperator * /*op*/)
{
  /* The poll function ensures armature->active_collection is not NULL. */
  bArmature *armature = ED_armature_context(C);
  ANIM_armature_bonecoll_remove(armature, armature->runtime.active_collection);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
  DEG_id_tag_update(&armature->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Bone Collection";
  ot->idname = "ARMATURE_OT_collection_remove";
  ot->description = "Remove the active bone collection";

  /* API callbacks. */
  ot->exec = bone_collection_remove_exec;
  ot->poll = active_bone_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus bone_collection_move_exec(bContext *C, wmOperator *op)
{
  const int direction = RNA_enum_get(op->ptr, "direction");

  /* Poll function makes sure this is valid. */
  bArmature *armature = ED_armature_context(C);

  const bool ok = ANIM_armature_bonecoll_move(
      armature, armature->runtime.active_collection, direction);
  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  ANIM_armature_bonecoll_active_runtime_refresh(armature);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, nullptr);
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

  /* API callbacks. */
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

static BoneCollection *get_bonecoll_named_or_active(bContext * /*C*/, wmOperator *op, Object *ob)
{
  bArmature *armature = static_cast<bArmature *>(ob->data);

  char bcoll_name[MAX_NAME];
  RNA_string_get(op->ptr, "name", bcoll_name);

  if (bcoll_name[0] == '\0') {
    return armature->runtime.active_collection;
  }

  BoneCollection *bcoll = ANIM_armature_bonecoll_get_by_name(armature, bcoll_name);
  if (!bcoll) {
    BKE_reportf(op->reports, RPT_ERROR, "No bone collection named '%s'", bcoll_name);
    return nullptr;
  }

  return bcoll;
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
    if (!EBONE_EDITABLE(ebone) || !blender::animrig::bone_is_visible(arm, ebone)) {
      continue;
    }
    *made_any_changes |= assign_func(bcoll, ebone);
    *had_bones_to_assign = true;
  }

  ED_armature_edit_sync_selection(arm->edbo);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
}

/**
 * Assign or unassign all selected bones to/from the given bone collection.
 *
 * \return whether the current mode is actually supported.
 */
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
      bone_collection_assign_editbones(
          C, ob, bcoll, assign_ebone_func, made_any_changes, had_bones_to_assign);

      ED_outliner_select_sync_from_edit_bone_tag(C);
      return true;
    }

    default:
      return false;
  }
}

/**
 * Assign or unassign the named bone to/from the given bone collection.
 *
 * \return whether the current mode is actually supported.
 */
static bool bone_collection_assign_named_mode_specific(bContext *C,
                                                       Object *ob,
                                                       BoneCollection *bcoll,
                                                       const char *bone_name,
                                                       assign_bone_func assign_bone_func,
                                                       assign_ebone_func assign_ebone_func,
                                                       bool *made_any_changes,
                                                       bool *had_bones_to_assign)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);

  switch (CTX_data_mode_enum(C)) {
    case CTX_MODE_POSE: {
      bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
      if (!pchan) {
        return true;
      }

      *had_bones_to_assign = true;
      *made_any_changes |= assign_bone_func(bcoll, pchan->bone);

      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, ob);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT); /* Recreate the draw buffers. */
      return true;
    }

    case CTX_MODE_EDIT_ARMATURE: {
      EditBone *ebone = ED_armature_ebone_find_name(arm->edbo, bone_name);
      if (!ebone) {
        return true;
      }

      *had_bones_to_assign = true;
      *made_any_changes |= assign_ebone_func(bcoll, ebone);

      ED_armature_edit_sync_selection(arm->edbo);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
      return true;
    }

    default:
      return false;
  }
}

static bool bone_collection_assign_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  if (ob == nullptr) {
    return false;
  }

  if (ob->type != OB_ARMATURE) {
    CTX_wm_operator_poll_msg_set(C, "Bone collections can only be edited on an Armature");
    return false;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  if (armature != ED_armature_context(C)) {
    CTX_wm_operator_poll_msg_set(C, "Pinned armature is not active in the 3D viewport");
    return false;
  }

  if (!ID_IS_EDITABLE(armature) && !ID_IS_OVERRIDE_LIBRARY(armature)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit bone collections on linked Armatures without override");
    return false;
  }
  if (BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot edit bone collections on a linked Armature with a system "
                                 "override; explicitly create an override on the Armature Data");
    return false;
  }

  CTX_wm_operator_poll_msg_set(C, "Linked bone collections are not editable");

  /* The target bone collection can be specified by name in an operator property, but that's not
   * available here. So just allow in the poll function, and do the final check in the execute. */
  return true;
}

/* Assign selected pchans to the bone collection that the user selects */
static wmOperatorStatus bone_collection_assign_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  if (!ANIM_armature_bonecoll_is_editable(armature, bcoll)) {
    BKE_reportf(op->reports, RPT_ERROR, "Cannot assign to linked bone collection %s", bcoll->name);
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
    BKE_report(
        op->reports, RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_assign) {
    BKE_report(
        op->reports, RPT_WARNING, "No bones selected, nothing to assign to bone collection");
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    BKE_report(
        op->reports, RPT_WARNING, "All selected bones were already part of this collection");
    return OPERATOR_CANCELLED;
  }

  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, &ob->id);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Selected Bones to Collection";
  ot->idname = "ARMATURE_OT_collection_assign";
  ot->description = "Add selected bones to the chosen bone collection";

  /* API callbacks. */
  ot->exec = bone_collection_assign_exec;
  ot->poll = bone_collection_assign_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to assign this bone to; empty to assign to the "
                 "active bone collection");
}

static bool bone_collection_create_and_assign_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob == nullptr) {
    return false;
  }

  if (ob->type != OB_ARMATURE) {
    CTX_wm_operator_poll_msg_set(C, "Bone collections can only be edited on an Armature");
    return false;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  if (!ID_IS_EDITABLE(armature) && !ID_IS_OVERRIDE_LIBRARY(armature)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit bone collections on linked Armatures without override");
    return false;
  }
  if (BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot edit bone collections on a linked Armature with a system "
                                 "override; explicitly create an override on the Armature Data");
    return false;
  }

  return true;
}

/* Assign selected pchans to the bone collection that the user selects */
static wmOperatorStatus bone_collection_create_and_assign_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);

  char bcoll_name[MAX_NAME];
  RNA_string_get(op->ptr, "name", bcoll_name);

  /* Note that this bone collection can be removed later on, if the assignment part of this
   * operation failed. */
  BoneCollection *bcoll = ANIM_armature_bonecoll_new(armature, bcoll_name);

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
    BKE_report(
        op->reports, RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    ANIM_armature_bonecoll_remove(armature, bcoll);
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_assign) {
    BKE_report(
        op->reports, RPT_WARNING, "No bones selected, nothing to assign to bone collection");
    return OPERATOR_FINISHED;
  }
  /* Not checking for `made_any_changes`, as if there were any bones to assign, they never could
   * have already been assigned to this brand new bone collection. */

  ANIM_armature_bonecoll_active_set(armature, bcoll);
  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, &ob->id);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_create_and_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Selected Bones to New Collection";
  ot->idname = "ARMATURE_OT_collection_create_and_assign";
  ot->description = "Create a new bone collection and assign all selected bones";

  /* API callbacks. */
  ot->exec = bone_collection_create_and_assign_exec;
  ot->poll = bone_collection_create_and_assign_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to create");
}

static wmOperatorStatus bone_collection_unassign_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob);
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
    BKE_reportf(
        op->reports, RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_unassign) {
    BKE_reportf(
        op->reports, RPT_WARNING, "No bones selected, nothing to unassign from bone collection");
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    BKE_reportf(
        op->reports, RPT_WARNING, "None of the selected bones were assigned to this collection");
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

  /* API callbacks. */
  ot->exec = bone_collection_unassign_exec;
  ot->poll = bone_collection_assign_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to unassign this bone from; empty to unassign from "
                 "the active bone collection");
}

static wmOperatorStatus bone_collection_unassign_named_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = get_bonecoll_named_or_active(C, op, ob);
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char bone_name[MAX_NAME];
  RNA_string_get(op->ptr, "bone_name", bone_name);
  if (!bone_name[0]) {
    BKE_reportf(op->reports, RPT_ERROR, "Missing bone name");
    return OPERATOR_CANCELLED;
  }

  bool made_any_changes = false;
  bool had_bones_to_unassign = false;
  const bool mode_is_supported = bone_collection_assign_named_mode_specific(
      C,
      ob,
      bcoll,
      bone_name,
      ANIM_armature_bonecoll_unassign,
      ANIM_armature_bonecoll_unassign_editbone,
      &made_any_changes,
      &had_bones_to_unassign);

  if (!mode_is_supported) {
    BKE_reportf(
        op->reports, RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_unassign) {
    BKE_reportf(op->reports, RPT_WARNING, "Could not find bone '%s'", bone_name);
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Bone '%s' was not assigned to collection '%s'",
                bone_name,
                bcoll->name);
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_unassign_named(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Bone from Bone Collection";
  ot->idname = "ARMATURE_OT_collection_unassign_named";
  ot->description = "Unassign the named bone from this bone collection";

  /* API callbacks. */
  ot->exec = bone_collection_unassign_named_exec;
  ot->poll = bone_collection_assign_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 MAX_NAME,
                 "Bone Collection",
                 "Name of the bone collection to unassign this bone from; empty to unassign from "
                 "the active bone collection");
  RNA_def_string(ot->srna,
                 "bone_name",
                 nullptr,
                 MAX_NAME,
                 "Bone Name",
                 "Name of the bone to unassign from the collection; empty to use the active bone");
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

static bool armature_bone_select_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob && ob->type == OB_ARMATURE) {

    /* For bone selection, at least the pose should be editable to actually store
     * the selection state. */
    if (!ID_IS_EDITABLE(ob) && !ID_IS_OVERRIDE_LIBRARY(ob)) {
      CTX_wm_operator_poll_msg_set(
          C, "Cannot (de)select bones on linked object, that would need an override");
      return false;
    }
  }

  const bArmature *armature = ED_armature_context(C);
  if (armature == nullptr) {
    return false;
  }

  const bool is_editmode = armature->edbo != nullptr;
  if (!is_editmode) {
    Object *active_object = blender::ed::object::context_active_object(C);
    if (!active_object || active_object->type != OB_ARMATURE || active_object->data != armature) {
      /* There has to be an active object in order to hide a pose bone that points to the correct
       * armature. With pinning, the active object may not be an armature. */
      CTX_wm_operator_poll_msg_set(C, "The active object does not match the armature");
      return false;
    }
  }

  if (armature->runtime.active_collection == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "No active bone collection");
    return false;
  }
  return true;
}

static void bone_collection_select(bContext *C,
                                   bArmature *armature,
                                   BoneCollection *bcoll,
                                   const bool select)
{
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
    Object *active_object = blender::ed::object::context_active_object(C);
    if (!active_object || active_object->type != OB_ARMATURE || active_object->data != armature) {
      /* This is covered by the poll function. */
      BLI_assert_unreachable();
      return;
    }
    LISTBASE_FOREACH (BoneCollectionMember *, member, &bcoll->bones) {
      Bone *bone = member->bone;
      bPoseChannel *pose_bone = BKE_pose_channel_find_name(active_object->pose, bone->name);
      BLI_assert_msg(pose_bone != nullptr, "The pose bones and armature bones are out of sync");
      if (!blender::animrig::bone_is_visible(armature, pose_bone)) {
        continue;
      }
      if (bone->flag & BONE_UNSELECTABLE) {
        continue;
      }

      if (select) {
        pose_bone->flag |= POSE_SELECTED;
      }
      else {
        pose_bone->flag &= ~POSE_SELECTED;
      }
    }
    DEG_id_tag_update(&active_object->id, ID_RECALC_SELECT);
  }

  DEG_id_tag_update(&armature->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  if (is_editmode) {
    ED_outliner_select_sync_from_edit_bone_tag(C);
  }
  else {
    ED_outliner_select_sync_from_pose_bone_tag(C);
  }
}

static wmOperatorStatus bone_collection_select_exec(bContext *C, wmOperator * /*op*/)
{
  bArmature *armature = ED_armature_context(C);
  if (armature == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = armature->runtime.active_collection;
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bone_collection_select(C, armature, bcoll, true);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Bones of Bone Collection";
  ot->idname = "ARMATURE_OT_collection_select";
  ot->description = "Select bones in active Bone Collection";

  /* API callbacks. */
  ot->exec = bone_collection_select_exec;
  ot->poll = armature_bone_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus bone_collection_deselect_exec(bContext *C, wmOperator * /*op*/)
{
  bArmature *armature = ED_armature_context(C);
  if (armature == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BoneCollection *bcoll = armature->runtime.active_collection;
  if (bcoll == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bone_collection_select(C, armature, bcoll, false);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_collection_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Bone Collection";
  ot->idname = "ARMATURE_OT_collection_deselect";
  ot->description = "Deselect bones of active Bone Collection";

  /* API callbacks. */
  ot->exec = bone_collection_deselect_exec;
  ot->poll = armature_bone_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------- */

static BoneCollection *add_or_move_to_collection_bcoll(wmOperator *op, bArmature *arm)
{
  const int collection_index = RNA_int_get(op->ptr, "collection_index");
  BoneCollection *target_bcoll;

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "new_collection_name");
  if (RNA_property_is_set(op->ptr, prop) ||
      /* Neither properties can be used, the operator may have been called with defaults.
       * In this case add a root collection, the default name will be used. */
      (collection_index < 0))
  {
    /* TODO: check this with linked, non-overridden armatures. */
    char new_collection_name[MAX_NAME];
    RNA_string_get(op->ptr, "new_collection_name", new_collection_name);
    target_bcoll = ANIM_armature_bonecoll_new(arm, new_collection_name, collection_index);
    BLI_assert_msg(target_bcoll,
                   "It should always be possible to create a new bone collection on an armature");
    ANIM_armature_bonecoll_active_set(arm, target_bcoll);
  }
  else {
    if (collection_index >= arm->collection_array_num) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Bone collection with index %d not found on Armature %s",
                  collection_index,
                  arm->id.name + 2);
      return nullptr;
    }
    target_bcoll = arm->collection_array[collection_index];
  }

  if (!ANIM_armature_bonecoll_is_editable(arm, target_bcoll)) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Bone collection %s is not editable, maybe add an override on the armature Data?",
                target_bcoll->name);
    return nullptr;
  }

  return target_bcoll;
}

static wmOperatorStatus add_or_move_to_collection_exec(bContext *C,
                                                       wmOperator *op,
                                                       const assign_bone_func assign_func_bone,
                                                       const assign_ebone_func assign_func_ebone)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob->mode == OB_MODE_POSE) {
    ob = ED_pose_object_from_context(C);
  }
  if (!ob) {
    BKE_reportf(op->reports, RPT_ERROR, "No object found to operate on");
    return OPERATOR_CANCELLED;
  }

  bArmature *arm = static_cast<bArmature *>(ob->data);
  BoneCollection *target_bcoll = add_or_move_to_collection_bcoll(op, arm);
  if (!target_bcoll) {
    /* add_or_move_to_collection_bcoll() already reported the reason. */
    return OPERATOR_CANCELLED;
  }

  bool made_any_changes = false;
  bool had_bones_to_assign = false;
  const bool mode_is_supported = bone_collection_assign_mode_specific(C,
                                                                      ob,
                                                                      target_bcoll,
                                                                      assign_func_bone,
                                                                      assign_func_ebone,
                                                                      &made_any_changes,
                                                                      &had_bones_to_assign);

  if (!mode_is_supported) {
    BKE_report(
        op->reports, RPT_ERROR, "This operator only works in pose mode and armature edit mode");
    return OPERATOR_CANCELLED;
  }
  if (!had_bones_to_assign) {
    BKE_report(
        op->reports, RPT_WARNING, "No bones selected, nothing to assign to bone collection");
    return OPERATOR_CANCELLED;
  }
  if (!made_any_changes) {
    BKE_report(
        op->reports, RPT_WARNING, "All selected bones were already part of this collection");
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT); /* Recreate the draw buffers. */

  WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus move_to_collection_exec(bContext *C, wmOperator *op)
{
  return add_or_move_to_collection_exec(C,
                                        op,
                                        ANIM_armature_bonecoll_assign_and_move,
                                        ANIM_armature_bonecoll_assign_and_move_editbone);
}

static wmOperatorStatus assign_to_collection_exec(bContext *C, wmOperator *op)
{
  return add_or_move_to_collection_exec(
      C, op, ANIM_armature_bonecoll_assign, ANIM_armature_bonecoll_assign_editbone);
}

static bool move_to_collection_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob == nullptr) {
    return false;
  }

  if (ob->type != OB_ARMATURE) {
    CTX_wm_operator_poll_msg_set(C, "Bone collections can only be edited on an Armature");
    return false;
  }

  const bArmature *armature = static_cast<bArmature *>(ob->data);
  if (!ID_IS_EDITABLE(armature) && !ID_IS_OVERRIDE_LIBRARY(armature)) {
    CTX_wm_operator_poll_msg_set(C, "This needs a local Armature or an override");
    return false;
  }

  if (BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Cannot update a linked Armature with a system override; "
                                 "explicitly create an override on the Armature Data");
    return false;
  }

  CTX_wm_operator_poll_msg_set(C, "Linked bone collections are not editable");

  /* Ideally this would also check the target bone collection to move/assign to.
   * However, that requires access to the operator properties, and those are not
   * available in the poll function. */
  return true;
}

/**
 * Encode the parameters into an integer, and return as void*.
 *
 * NOTE(@sybren): This makes it possible to use these values and pass them directly as
 * 'custom data' pointer to `uiLayout::menu_fn()`. This makes it possible to give every menu a
 * unique bone collection index for which it should show the child collections, without having to
 * allocate memory or use static variables.  See `move_to_collection_invoke()` in `object_edit.cc`
 * for the alternative that I wanted to avoid.
 */
static void *menu_custom_data_encode(const int bcoll_index, const bool is_move_operation)
{
  /* Add 1 to the index, so that it's never negative (it can be -1 to indicate 'all roots'). */
  const uintptr_t index_and_move_bit = ((bcoll_index + 1) << 1) | (is_move_operation << 0);
  return reinterpret_cast<void *>(index_and_move_bit);
}

/**
 * Decode the `void*` back into a bone collection index and a boolean `is_move_operation`.
 *
 * \see menu_custom_data_encode for rationale.
 */
static std::pair<int, bool> menu_custom_data_decode(void *menu_custom_data)
{
  const uintptr_t index_and_move_bit = reinterpret_cast<intptr_t>(menu_custom_data);
  const bool is_move_operation = (index_and_move_bit & 1) == 1;
  const int bcoll_index = int(index_and_move_bit >> 1) - 1;
  return std::make_pair(bcoll_index, is_move_operation);
}

static int icon_for_bone_collection(const bool collection_contains_active_bone)
{
  return collection_contains_active_bone ? ICON_REMOVE : ICON_ADD;
}

static void menu_add_item_for_move_assign_unassign(uiLayout *layout,
                                                   const bArmature *arm,
                                                   const BoneCollection *bcoll,
                                                   const int bcoll_index,
                                                   const bool is_move_operation)
{
  if (is_move_operation) {
    PointerRNA op_ptr = layout->op("ARMATURE_OT_move_to_collection", bcoll->name, ICON_NONE);
    RNA_int_set(&op_ptr, "collection_index", bcoll_index);
    return;
  }

  const bool contains_active_bone = ANIM_armature_bonecoll_contains_active_bone(arm, bcoll);
  const int icon = icon_for_bone_collection(contains_active_bone);

  if (contains_active_bone) {
    PointerRNA op_ptr = layout->op("ARMATURE_OT_collection_unassign", bcoll->name, icon);
    RNA_string_set(&op_ptr, "name", bcoll->name);
  }
  else {
    PointerRNA op_ptr = layout->op("ARMATURE_OT_collection_assign", bcoll->name, icon);
    RNA_string_set(&op_ptr, "name", bcoll->name);
  }
}

/**
 * Add menu items to the layout, for a set of bone collections.
 *
 * \param menu_custom_data: Contains two values, encoded as void* to match the signature required
 * by `uiLayout::menu_fn`. It contains the parent bone collection index (either -1 to show all
 * roots, or another value to show the children of that collection), as well as a boolean that
 * indicates whether the menu is created for the "move to collection" or "assign to collection"
 * operator.
 *
 * \see menu_custom_data_encode
 */
static void move_to_collection_menu_create(bContext *C, uiLayout *layout, void *menu_custom_data)
{
  int parent_bcoll_index;
  bool is_move_operation;
  std::tie(parent_bcoll_index, is_move_operation) = menu_custom_data_decode(menu_custom_data);

  const Object *ob = blender::ed::object::context_object(C);
  const bArmature *arm = static_cast<bArmature *>(ob->data);

  /* The "Create a new collection" mode of this operator has its own menu, and should thus be
   * invoked. */
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);
  PointerRNA op_ptr = layout->op(
      is_move_operation ? "ARMATURE_OT_move_to_collection" : "ARMATURE_OT_assign_to_collection",
      CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "New Bone Collection"),
      ICON_ADD);
  RNA_int_set(&op_ptr, "collection_index", parent_bcoll_index);

  layout->separator();

  /* The remaining operators in this menu should be executed on click. Invoking
   * them would show this same menu again. */
  layout->operator_context_set(blender::wm::OpCallContext::ExecDefault);

  int child_index, child_count;
  if (parent_bcoll_index == -1) {
    child_index = 0;
    child_count = arm->collection_root_count;
  }
  else {
    /* Add a menu item to assign to the parent first, before listing the children.
     * The parent is assumed to be editable, because otherwise the menu would
     * have been disabled already one recursion level higher. */
    const BoneCollection *parent = arm->collection_array[parent_bcoll_index];
    menu_add_item_for_move_assign_unassign(
        layout, arm, parent, parent_bcoll_index, is_move_operation);
    layout->separator();

    child_index = parent->child_index;
    child_count = parent->child_count;
  }

  /* Loop over the children. There should be at least one, otherwise this parent
   * bone collection wouldn't have been drawn as a menu. */
  for (int index = child_index; index < child_index + child_count; index++) {
    const BoneCollection *bcoll = arm->collection_array[index];

    /* Avoid assigning/moving to a linked bone collection. */
    if (!ANIM_armature_bonecoll_is_editable(arm, bcoll)) {
      uiLayout *sub = &layout->row(false);
      sub->enabled_set(false);

      menu_add_item_for_move_assign_unassign(sub, arm, bcoll, index, is_move_operation);
      continue;
    }

    if (blender::animrig::bonecoll_has_children(bcoll)) {
      layout->menu_fn(bcoll->name,
                      ICON_NONE,
                      move_to_collection_menu_create,
                      menu_custom_data_encode(index, is_move_operation));
    }
    else {
      menu_add_item_for_move_assign_unassign(layout, arm, bcoll, index, is_move_operation);
    }
  }
}

static wmOperatorStatus move_to_collection_regular_invoke(bContext *C, wmOperator *op)
{
  const char *title = CTX_IFACE_(op->type->translation_context, op->type->name);
  uiPopupMenu *pup = UI_popup_menu_begin(C, title, ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  const bool is_move_operation = STREQ(op->type->idname, "ARMATURE_OT_move_to_collection");
  move_to_collection_menu_create(C, layout, menu_custom_data_encode(-1, is_move_operation));

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static wmOperatorStatus move_to_new_collection_invoke(bContext *C, wmOperator *op)
{
  RNA_string_set(op->ptr, "new_collection_name", IFACE_("Bones"));
  return WM_operator_props_dialog_popup(
      C, op, 200, IFACE_("Move to New Bone Collection"), IFACE_("Create"));
}

static wmOperatorStatus move_to_collection_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  /* Invoking with `collection_index` set has a special meaning: show the menu to create a new bone
   * collection as the child of this one. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection_index");
  if (RNA_property_is_set(op->ptr, prop)) {
    return move_to_new_collection_invoke(C, op);
  }

  return move_to_collection_regular_invoke(C, op);
}

void ARMATURE_OT_move_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move to Collection";
  ot->description = "Move bones to a collection";
  ot->idname = "ARMATURE_OT_move_to_collection";

  /* API callbacks. */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* Flags don't include OPTYPE_REGISTER, as the redo panel doesn't make much sense for this
   * operator. The visibility of the RNA properties is determined by the needs of the 'New Catalog'
   * popup, so that a name can be entered. This means that the redo panel would also only show the
   * 'Name' property, without any choice for another collection. */
  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_int(
      ot->srna,
      "collection_index",
      -1,
      -1,
      INT_MAX,
      "Collection Index",
      "Index of the collection to move selected bones to. When the operator should create a new "
      "bone collection, do not include this parameter and pass new_collection_name",
      -1,
      INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_string(
      ot->srna,
      "new_collection_name",
      nullptr,
      MAX_NAME,
      "Name",
      "Name of a to-be-added bone collection. Only pass this if you want to create a new bone "
      "collection and move the selected bones to it. To move to an existing collection, do not "
      "include this parameter and use collection_index");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

void ARMATURE_OT_assign_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Assign to Collection";
  ot->description =
      "Assign all selected bones to a collection, or unassign them, depending on whether the "
      "active bone is already assigned or not";
  ot->idname = "ARMATURE_OT_assign_to_collection";

  /* API callbacks. */
  ot->exec = assign_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* Flags don't include OPTYPE_REGISTER, as the redo panel doesn't make much sense for this
   * operator. The visibility of the RNA properties is determined by the needs of the 'New Catalog'
   * popup, so that a name can be entered. This means that the redo panel would also only show the
   * 'Name' property, without any choice for another collection. */
  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_int(
      ot->srna,
      "collection_index",
      -1,
      -1,
      INT_MAX,
      "Collection Index",
      "Index of the collection to assign selected bones to. When the operator should create a new "
      "bone collection, use new_collection_name to define the collection name, and set this "
      "parameter to the parent index of the new bone collection",
      -1,
      INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_string(
      ot->srna,
      "new_collection_name",
      nullptr,
      MAX_NAME,
      "Name",
      "Name of a to-be-added bone collection. Only pass this if you want to create a new bone "
      "collection and assign the selected bones to it. To assign to an existing collection, do "
      "not include this parameter and use collection_index");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

/* ********************************************** */
