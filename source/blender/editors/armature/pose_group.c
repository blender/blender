/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup edarmature
 * Implementation of Bone Groups operators and editing API's.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "armature_intern.h"

/* ********************************************** */
/* Bone Groups */

static bool pose_group_poll(bContext *C)
{
  if (!ED_operator_posemode_context(C)) {
    CTX_wm_operator_poll_msg_set(C, "Bone groups can only be edited in pose mode");
    return false;
  }

  Object *obpose = ED_pose_object_from_context(C);
  if (ID_IS_OVERRIDE_LIBRARY(obpose)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit bone groups for library overrides");
    return false;
  }

  return true;
}

static int pose_group_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);

  /* only continue if there's an object and pose */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* for now, just call the API function for this */
  BKE_pose_add_group(ob->pose, NULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_group_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Bone Group";
  ot->idname = "POSE_OT_group_add";
  ot->description = "Add a new bone group";

  /* api callbacks */
  ot->exec = pose_group_add_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int pose_group_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);

  /* only continue if there's an object and pose */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* for now, just call the API function for this */
  BKE_pose_remove_group_index(ob->pose, ob->pose->active_group);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

void POSE_OT_group_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Bone Group";
  ot->idname = "POSE_OT_group_remove";
  ot->description = "Remove the active bone group";

  /* api callbacks */
  ot->exec = pose_group_remove_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ------------ */

/* invoke callback which presents a list of bone-groups for the user to choose from */
static int pose_groups_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = ED_pose_object_from_context(C);
  bPose *pose;
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "type");

  uiPopupMenu *pup;
  uiLayout *layout;
  bActionGroup *grp;
  int i;

  /* only continue if there's an object, and a pose there too */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }
  pose = ob->pose;

  /* If group index is set, try to use it! */
  if (RNA_property_is_set(op->ptr, prop)) {
    const int num_groups = BLI_listbase_count(&pose->agroups);
    const int group = RNA_property_int_get(op->ptr, prop);

    /* just use the active group index, and call the exec callback for the calling operator */
    if (group > 0 && group <= num_groups) {
      return op->type->exec(C, op);
    }
  }

  /* if there's no active group (or active is invalid), create a new menu to find it */
  if (pose->active_group <= 0) {
    /* create a new menu, and start populating it with group names */
    pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
    layout = UI_popup_menu_layout(pup);

    /* special entry - allow creating a new group, then using that
     * (not to be used for removing though)
     */
    if (strstr(op->idname, "assign")) {
      uiItemIntO(layout, "New Group", ICON_NONE, op->idname, "type", 0);
      uiItemS(layout);
    }

    /* add entries for each group */
    for (grp = pose->agroups.first, i = 1; grp; grp = grp->next, i++) {
      uiItemIntO(layout, grp->name, ICON_NONE, op->idname, "type", i);
    }

    /* finish building the menu, and process it (should result in calling self again) */
    UI_popup_menu_end(C, pup);

    return OPERATOR_INTERFACE;
  }

  /* just use the active group index, and call the exec callback for the calling operator */
  RNA_int_set(op->ptr, "type", pose->active_group);
  return op->type->exec(C, op);
}

/* Assign selected pchans to the bone group that the user selects */
static int pose_group_assign_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_pose_object_from_context(C);
  bPose *pose;
  bool done = false;

  /* only continue if there's an object, and a pose there too */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  pose = ob->pose;

  /* set the active group number to the one from operator props
   * - if 0 after this, make a new group...
   */
  pose->active_group = RNA_int_get(op->ptr, "type");
  if (pose->active_group == 0) {
    BKE_pose_add_group(ob->pose, NULL);
  }

  /* add selected bones to group then */
  FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob, pchan) {
    pchan->agrp_index = pose->active_group;
    done = true;
  }
  FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

  /* report done status */
  if (done) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void POSE_OT_group_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Selected to Bone Group";
  ot->idname = "POSE_OT_group_assign";
  ot->description = "Add selected bones to the chosen bone group";

  /* api callbacks */
  ot->invoke = pose_groups_menu_invoke;
  ot->exec = pose_group_assign_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna, "type", 0, 0, INT_MAX, "Bone Group Index", "", 0, 10);
}

static int pose_group_unassign_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);
  bool done = false;

  /* only continue if there's an object, and a pose there too */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* find selected bones to remove from all bone groups */
  FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob, pchan) {
    if (pchan->agrp_index) {
      pchan->agrp_index = 0;
      done = true;
    }
  }
  FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

  /* report done status */
  if (done) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void POSE_OT_group_unassign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Selected from Bone Groups";
  ot->idname = "POSE_OT_group_unassign";
  ot->description = "Remove selected bones from all bone groups";

  /* api callbacks */
  ot->exec = pose_group_unassign_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int group_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_pose_object_from_context(C);
  bPose *pose = (ob) ? ob->pose : NULL;
  bPoseChannel *pchan;
  bActionGroup *grp;
  int dir = RNA_enum_get(op->ptr, "direction");

  if (ELEM(NULL, ob, pose)) {
    return OPERATOR_CANCELLED;
  }
  if (pose->active_group <= 0) {
    return OPERATOR_CANCELLED;
  }

  /* get group to move */
  grp = BLI_findlink(&pose->agroups, pose->active_group - 1);
  if (grp == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* move bone group */
  if (BLI_listbase_link_move(&pose->agroups, grp, dir)) {
    int grpIndexA = pose->active_group;
    int grpIndexB = grpIndexA + dir;

    pose->active_group += dir;
    /* fix changed bone group indices in bones (swap grpIndexA with grpIndexB) */
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->agrp_index == grpIndexB) {
        pchan->agrp_index = grpIndexA;
      }
      else if (pchan->agrp_index == grpIndexA) {
        pchan->agrp_index = grpIndexB;
      }
    }

    /* notifiers for updates */
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }

  return OPERATOR_FINISHED;
}

void POSE_OT_group_move(wmOperatorType *ot)
{
  static const EnumPropertyItem group_slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Bone Group";
  ot->idname = "POSE_OT_group_move";
  ot->description = "Change position of active Bone Group in list of Bone Groups";

  /* api callbacks */
  ot->exec = group_move_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "direction",
               group_slot_move,
               0,
               "Direction",
               "Direction to move the active Bone Group towards");
}

/* bone group sort element */
typedef struct tSortActionGroup {
  bActionGroup *agrp;
  int index;
} tSortActionGroup;

/* compare bone groups by name */
static int compare_agroup(const void *sgrp_a_ptr, const void *sgrp_b_ptr)
{
  const tSortActionGroup *sgrp_a = sgrp_a_ptr;
  const tSortActionGroup *sgrp_b = sgrp_b_ptr;

  return strcmp(sgrp_a->agrp->name, sgrp_b->agrp->name);
}

static int group_sort_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);
  bPose *pose = (ob) ? ob->pose : NULL;
  bPoseChannel *pchan;
  tSortActionGroup *agrp_array;
  bActionGroup *agrp;

  if (ELEM(NULL, ob, pose)) {
    return OPERATOR_CANCELLED;
  }
  if (pose->active_group <= 0) {
    return OPERATOR_CANCELLED;
  }

  /* create temporary array with bone groups and indices */
  int agrp_count = BLI_listbase_count(&pose->agroups);
  agrp_array = MEM_mallocN(sizeof(tSortActionGroup) * agrp_count, "sort bone groups");
  int i;
  for (agrp = pose->agroups.first, i = 0; agrp; agrp = agrp->next, i++) {
    BLI_assert(i < agrp_count);
    agrp_array[i].agrp = agrp;
    agrp_array[i].index = i + 1;
  }

  /* sort bone groups by name */
  qsort(agrp_array, agrp_count, sizeof(tSortActionGroup), compare_agroup);

  /* create sorted bone group list from sorted array */
  BLI_listbase_clear(&pose->agroups);
  for (i = 0; i < agrp_count; i++) {
    BLI_addtail(&pose->agroups, agrp_array[i].agrp);
  }

  /* Fix changed bone group indices in bones. */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    for (i = 0; i < agrp_count; i++) {
      if (pchan->agrp_index == agrp_array[i].index) {
        pchan->agrp_index = i + 1;
        break;
      }
    }
  }

  /* free temp resources */
  MEM_freeN(agrp_array);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

void POSE_OT_group_sort(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sort Bone Groups";
  ot->idname = "POSE_OT_group_sort";
  ot->description = "Sort Bone Groups by their names in ascending order";

  /* api callbacks */
  ot->exec = group_sort_exec;
  ot->poll = pose_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void pose_group_select(Object *ob, bool select)
{
  bPose *pose = ob->pose;

  FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN (ob, pchan) {
    if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
      if (select) {
        if (pchan->agrp_index == pose->active_group) {
          pchan->bone->flag |= BONE_SELECTED;
        }
      }
      else {
        if (pchan->agrp_index == pose->active_group) {
          pchan->bone->flag &= ~BONE_SELECTED;
        }
      }
    }
  }
  FOREACH_PCHAN_VISIBLE_IN_OBJECT_END;
}

static int pose_group_select_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);

  /* only continue if there's an object, and a pose there too */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  pose_group_select(ob, 1);

  /* notifiers for updates */
  bArmature *arm = ob->data;
  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  ED_outliner_select_sync_from_pose_bone_tag(C);

  return OPERATOR_FINISHED;
}

void POSE_OT_group_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Bones of Bone Group";
  ot->idname = "POSE_OT_group_select";
  ot->description = "Select bones in active Bone Group";

  /* api callbacks */
  ot->exec = pose_group_select_exec;
  ot->poll = ED_operator_posemode_context;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int pose_group_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_pose_object_from_context(C);

  /* only continue if there's an object, and a pose there too */
  if (ELEM(NULL, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  pose_group_select(ob, 0);

  /* notifiers for updates */
  bArmature *arm = ob->data;
  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  ED_outliner_select_sync_from_pose_bone_tag(C);

  return OPERATOR_FINISHED;
}

void POSE_OT_group_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Bone Group";
  ot->idname = "POSE_OT_group_deselect";
  ot->description = "Deselect bones of active Bone Group";

  /* api callbacks */
  ot->exec = pose_group_deselect_exec;
  ot->poll = ED_operator_posemode_context;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
