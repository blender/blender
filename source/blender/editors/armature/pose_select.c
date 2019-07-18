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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edarmature
 */

#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_layer.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* utility macros for storing a temp int in the bone (selection flag) */
#define PBONE_PREV_FLAG_GET(pchan) ((void)0, (POINTER_AS_INT((pchan)->temp)))
#define PBONE_PREV_FLAG_SET(pchan, val) ((pchan)->temp = POINTER_FROM_INT(val))

/* ***************** Pose Select Utilities ********************* */

/* Note: SEL_TOGGLE is assumed to have already been handled! */
static void pose_do_bone_select(bPoseChannel *pchan, const int select_mode)
{
  /* select pchan only if selectable, but deselect works always */
  switch (select_mode) {
    case SEL_SELECT:
      if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
        pchan->bone->flag |= BONE_SELECTED;
      }
      break;
    case SEL_DESELECT:
      pchan->bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      break;
    case SEL_INVERT:
      if (pchan->bone->flag & BONE_SELECTED) {
        pchan->bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      }
      else if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
        pchan->bone->flag |= BONE_SELECTED;
      }
      break;
  }
}

void ED_pose_bone_select_tag_update(Object *ob)
{
  BLI_assert(ob->type == OB_ARMATURE);
  bArmature *arm = ob->data;
  WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, ob);
  WM_main_add_notifier(NC_GEOM | ND_DATA, ob);

  if (arm->flag & ARM_HAS_VIZ_DEPS) {
    /* mask modifier ('armature' mode), etc. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
}

/* Utility method for changing the selection status of a bone */
void ED_pose_bone_select(Object *ob, bPoseChannel *pchan, bool select)
{
  bArmature *arm;

  /* sanity checks */
  // XXX: actually, we can probably still get away with no object - at most we have no updates
  if (ELEM(NULL, ob, ob->pose, pchan, pchan->bone)) {
    return;
  }

  arm = ob->data;

  /* can only change selection state if bone can be modified */
  if (PBONE_SELECTABLE(arm, pchan->bone)) {
    /* change selection state - activate too if selected */
    if (select) {
      pchan->bone->flag |= BONE_SELECTED;
      arm->act_bone = pchan->bone;
    }
    else {
      pchan->bone->flag &= ~BONE_SELECTED;
      arm->act_bone = NULL;
    }

    // TODO: select and activate corresponding vgroup?
    ED_pose_bone_select_tag_update(ob);
  }
}

/* called from editview.c, for mode-less pose selection */
/* assumes scene obact and basact is still on old situation */
bool ED_armature_pose_select_pick_with_buffer(ViewLayer *view_layer,
                                              View3D *v3d,
                                              Base *base,
                                              const unsigned int *buffer,
                                              short hits,
                                              bool extend,
                                              bool deselect,
                                              bool toggle,
                                              bool do_nearest)
{
  Object *ob = base->object;
  Bone *nearBone;

  if (!ob || !ob->pose) {
    return 0;
  }

  Object *ob_act = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);

  /* Callers happen to already get the active base */
  Base *base_dummy = NULL;
  nearBone = get_bone_from_selectbuffer(
      &base, 1, obedit != NULL, buffer, hits, 1, do_nearest, &base_dummy);

  /* if the bone cannot be affected, don't do anything */
  if ((nearBone) && !(nearBone->flag & BONE_UNSELECTABLE)) {
    bArmature *arm = ob->data;

    /* since we do unified select, we don't shift+select a bone if the
     * armature object was not active yet.
     * note, special exception for armature mode so we can do multi-select
     * we could check for multi-select explicitly but think its fine to
     * always give predictable behavior in weight paint mode - campbell */
    if ((ob_act == NULL) || ((ob_act != ob) && (ob_act->mode & OB_MODE_WEIGHT_PAINT) == 0)) {
      /* when we are entering into posemode via toggle-select,
       * from another active object - always select the bone. */
      if (!extend && !deselect && toggle) {
        /* re-select below */
        nearBone->flag &= ~BONE_SELECTED;
      }
    }

    if (!extend && !deselect && !toggle) {
      {
        /* Don't use 'BKE_object_pose_base_array_get_unique'
         * because we may be selecting from object mode. */
        FOREACH_VISIBLE_BASE_BEGIN (view_layer, v3d, base_iter) {
          Object *ob_iter = base_iter->object;
          if ((ob_iter->type == OB_ARMATURE) && (ob_iter->mode & OB_MODE_POSE)) {
            if (ED_pose_deselect_all(ob_iter, SEL_DESELECT, true)) {
              ED_pose_bone_select_tag_update(ob_iter);
            }
          }
        }
        FOREACH_VISIBLE_BASE_END;
      }
      nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      arm->act_bone = nearBone;
    }
    else {
      if (extend) {
        nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
        arm->act_bone = nearBone;
      }
      else if (deselect) {
        nearBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      }
      else if (toggle) {
        if (nearBone->flag & BONE_SELECTED) {
          /* if not active, we make it active */
          if (nearBone != arm->act_bone) {
            arm->act_bone = nearBone;
          }
          else {
            nearBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
          }
        }
        else {
          nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
          arm->act_bone = nearBone;
        }
      }
    }

    if (ob_act) {
      /* in weightpaint we select the associated vertex group too */
      if (ob_act->mode & OB_MODE_WEIGHT_PAINT) {
        if (nearBone == arm->act_bone) {
          ED_vgroup_select_by_name(ob_act, nearBone->name);
          DEG_id_tag_update(&ob_act->id, ID_RECALC_GEOMETRY);
        }
      }
      /* if there are some dependencies for visualizing armature state
       * (e.g. Mask Modifier in 'Armature' mode), force update
       */
      else if (arm->flag & ARM_HAS_VIZ_DEPS) {
        /* NOTE: ob not ob_act here is intentional - it's the source of the
         *       bones being selected  [T37247]
         */
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      /* tag armature for copy-on-write update (since act_bone is in armature not object) */
      DEG_id_tag_update(&arm->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  return nearBone != NULL;
}

/**
 * While in weight-paint mode, a single pose may be active as well.
 * While not common, it's possible we have multiple armatures deforming a mesh.
 *
 * This function de-selects all other objects, and selects the new base.
 * It can't be set to the active object because we need
 * to keep this set to the weight paint object.
 */
void ED_armature_pose_select_in_wpaint_mode(ViewLayer *view_layer, Base *base_select)
{
  BLI_assert(base_select && (base_select->object->type == OB_ARMATURE));
  Object *ob_active = OBACT(view_layer);
  BLI_assert(ob_active && (ob_active->mode & OB_MODE_WEIGHT_PAINT));
  VirtualModifierData virtualModifierData;
  ModifierData *md = modifiers_getVirtualModifierList(ob_active, &virtualModifierData);
  for (; md; md = md->next) {
    if (md->type == eModifierType_Armature) {
      ArmatureModifierData *amd = (ArmatureModifierData *)md;
      Object *ob_arm = amd->object;
      if (ob_arm != NULL) {
        Base *base_arm = BKE_view_layer_base_find(view_layer, ob_arm);
        if ((base_arm != NULL) && (base_arm != base_select) && (base_arm->flag & BASE_SELECTED)) {
          ED_object_base_select(base_arm, BA_DESELECT);
        }
      }
    }
  }
  if ((base_select->flag & BASE_SELECTED) == 0) {
    ED_object_base_select(base_select, BA_SELECT);
  }
}

/* 'select_mode' is usual SEL_SELECT/SEL_DESELECT/SEL_TOGGLE/SEL_INVERT.
 * When true, 'ignore_visibility' makes this func also affect invisible bones
 * (hidden or on hidden layers). */
bool ED_pose_deselect_all(Object *ob, int select_mode, const bool ignore_visibility)
{
  bArmature *arm = ob->data;
  bPoseChannel *pchan;

  /* we call this from outliner too */
  if (ob->pose == NULL) {
    return false;
  }

  /* Determine if we're selecting or deselecting */
  if (select_mode == SEL_TOGGLE) {
    select_mode = SEL_SELECT;
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (ignore_visibility || PBONE_VISIBLE(arm, pchan->bone)) {
        if (pchan->bone->flag & BONE_SELECTED) {
          select_mode = SEL_DESELECT;
          break;
        }
      }
    }
  }

  /* Set the flags accordingly */
  bool changed = false;
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    /* ignore the pchan if it isn't visible or if its selection cannot be changed */
    if (ignore_visibility || PBONE_VISIBLE(arm, pchan->bone)) {
      int flag_prev = pchan->bone->flag;
      pose_do_bone_select(pchan, select_mode);
      changed = (changed || flag_prev != pchan->bone->flag);
    }
  }
  return changed;
}

static bool ed_pose_is_any_selected(Object *ob, bool ignore_visibility)
{
  bArmature *arm = ob->data;
  for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (ignore_visibility || PBONE_VISIBLE(arm, pchan->bone)) {
      if (pchan->bone->flag & BONE_SELECTED) {
        return true;
      }
    }
  }
  return false;
}

static bool ed_pose_is_any_selected_multi(Base **bases, uint bases_len, bool ignore_visibility)
{
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *ob_iter = bases[base_index]->object;
    if (ed_pose_is_any_selected(ob_iter, ignore_visibility)) {
      return true;
    }
  }
  return false;
}

bool ED_pose_deselect_all_multi_ex(Base **bases,
                                   uint bases_len,
                                   int select_mode,
                                   const bool ignore_visibility)
{
  if (select_mode == SEL_TOGGLE) {
    select_mode = ed_pose_is_any_selected_multi(bases, bases_len, ignore_visibility) ?
                      SEL_DESELECT :
                      SEL_SELECT;
  }

  bool changed_multi = false;
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *ob_iter = bases[base_index]->object;
    if (ED_pose_deselect_all(ob_iter, select_mode, ignore_visibility)) {
      ED_pose_bone_select_tag_update(ob_iter);
      changed_multi = true;
    }
  }
  return changed_multi;
}

bool ED_pose_deselect_all_multi(bContext *C, int select_mode, const bool ignore_visibility)
{
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_mode(vc.view_layer,
                                                         vc.v3d,
                                                         &bases_len,
                                                         {
                                                             .object_mode = OB_MODE_POSE,
                                                         });
  bool changed_multi = ED_pose_deselect_all_multi_ex(
      bases, bases_len, select_mode, ignore_visibility);
  MEM_freeN(bases);
  return changed_multi;
}

/* ***************** Selections ********************** */

static void selectconnected_posebonechildren(Object *ob, Bone *bone, int extend)
{
  Bone *curBone;

  /* stop when unconnected child is encountered, or when unselectable bone is encountered */
  if (!(bone->flag & BONE_CONNECTED) || (bone->flag & BONE_UNSELECTABLE)) {
    return;
  }

  if (extend) {
    bone->flag &= ~BONE_SELECTED;
  }
  else {
    bone->flag |= BONE_SELECTED;
  }

  for (curBone = bone->childbase.first; curBone; curBone = curBone->next) {
    selectconnected_posebonechildren(ob, curBone, extend);
  }
}

/* within active object context */
/* previously known as "selectconnected_posearmature" */
static int pose_select_connected_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Bone *bone, *curBone, *next = NULL;
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  view3d_operator_needs_opengl(C);

  Base *base = NULL;
  bone = get_nearest_bone(C, event->mval, !extend, &base);

  if (!bone) {
    return OPERATOR_CANCELLED;
  }

  /* Select parents */
  for (curBone = bone; curBone; curBone = next) {
    /* ignore bone if cannot be selected */
    if ((curBone->flag & BONE_UNSELECTABLE) == 0) {
      if (extend) {
        curBone->flag &= ~BONE_SELECTED;
      }
      else {
        curBone->flag |= BONE_SELECTED;
      }

      if (curBone->flag & BONE_CONNECTED) {
        next = curBone->parent;
      }
      else {
        next = NULL;
      }
    }
    else {
      next = NULL;
    }
  }

  /* Select children */
  for (curBone = bone->childbase.first; curBone; curBone = next) {
    selectconnected_posebonechildren(base->object, curBone, extend);
  }

  ED_pose_bone_select_tag_update(base->object);

  return OPERATOR_FINISHED;
}

static bool pose_select_linked_poll(bContext *C)
{
  return (ED_operator_view3d_active(C) && ED_operator_posemode(C));
}

void POSE_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Connected";
  ot->idname = "POSE_OT_select_linked";
  ot->description = "Select bones related to selected ones by parent/child relationships";

  /* callbacks */
  /* leave 'exec' unset */
  ot->invoke = pose_select_connected_invoke;
  ot->poll = pose_select_linked_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting everything first");
}

/* -------------------------------------- */

static int pose_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  Scene *scene = CTX_data_scene(C);
  int multipaint = scene->toolsettings->multipaint;

  if (action == SEL_TOGGLE) {
    action = CTX_DATA_COUNT(C, selected_pose_bones) ? SEL_DESELECT : SEL_SELECT;
  }

  Object *ob_prev = NULL;

  /*  Set the flags */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    bArmature *arm = ob->data;
    pose_do_bone_select(pchan, action);

    if (ob_prev != ob) {
      /* weightpaint or mask modifiers need depsgraph updates */
      if (multipaint || (arm->flag & ARM_HAS_VIZ_DEPS)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      /* need to tag armature for cow updates, or else selection doesn't update */
      DEG_id_tag_update(&arm->id, ID_RECALC_COPY_ON_WRITE);
      ob_prev = ob;
    }
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, NULL);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->idname = "POSE_OT_select_all";
  ot->description = "Toggle selection status of all bones";

  /* api callbacks */
  ot->exec = pose_de_select_all_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/* -------------------------------------- */

static int pose_select_parent_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bArmature *arm = (bArmature *)ob->data;
  bPoseChannel *pchan, *parent;

  /* Determine if there is an active bone */
  pchan = CTX_data_active_pose_bone(C);
  if (pchan) {
    parent = pchan->parent;
    if ((parent) && !(parent->bone->flag & (BONE_HIDDEN_P | BONE_UNSELECTABLE))) {
      parent->bone->flag |= BONE_SELECTED;
      arm->act_bone = parent->bone;
    }
    else {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    return OPERATOR_CANCELLED;
  }

  ED_pose_bone_select_tag_update(ob);
  return OPERATOR_FINISHED;
}

void POSE_OT_select_parent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Parent Bone";
  ot->idname = "POSE_OT_select_parent";
  ot->description = "Select bones that are parents of the currently selected bones";

  /* api callbacks */
  ot->exec = pose_select_parent_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

static int pose_select_constraint_target_exec(bContext *C, wmOperator *UNUSED(op))
{
  bConstraint *con;
  int found = 0;

  CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones) {
    if (pchan->bone->flag & BONE_SELECTED) {
      for (con = pchan->constraints.first; con; con = con->next) {
        const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
        ListBase targets = {NULL, NULL};
        bConstraintTarget *ct;

        if (cti && cti->get_constraint_targets) {
          cti->get_constraint_targets(con, &targets);

          for (ct = targets.first; ct; ct = ct->next) {
            Object *ob = ct->tar;

            /* Any armature that is also in pose mode should be selected. */
            if ((ct->subtarget[0] != '\0') && (ob != NULL) && (ob->type == OB_ARMATURE) &&
                (ob->mode == OB_MODE_POSE)) {
              bPoseChannel *pchanc = BKE_pose_channel_find_name(ob->pose, ct->subtarget);
              if ((pchanc) && !(pchanc->bone->flag & BONE_UNSELECTABLE)) {
                pchanc->bone->flag |= BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
                ED_pose_bone_select_tag_update(ob);
                found = 1;
              }
            }
          }

          if (cti->flush_constraint_targets) {
            cti->flush_constraint_targets(con, &targets, 1);
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (!found) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void POSE_OT_select_constraint_target(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Constraint Target";
  ot->idname = "POSE_OT_select_constraint_target";
  ot->description = "Select bones used as targets for the currently selected bones";

  /* api callbacks */
  ot->exec = pose_select_constraint_target_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

/* No need to convert to multi-objects. Just like we keep the non-active bones
 * selected we then keep the non-active objects untouched (selected/unselected). */
static int pose_select_hierarchy_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bArmature *arm = ob->data;
  bPoseChannel *pchan_act;
  int direction = RNA_enum_get(op->ptr, "direction");
  const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  pchan_act = BKE_pose_channel_active(ob);
  if (pchan_act == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (direction == BONE_SELECT_PARENT) {
    if (pchan_act->parent) {
      Bone *bone_parent;
      bone_parent = pchan_act->parent->bone;

      if (PBONE_SELECTABLE(arm, bone_parent)) {
        if (!add_to_sel) {
          pchan_act->bone->flag &= ~BONE_SELECTED;
        }
        bone_parent->flag |= BONE_SELECTED;
        arm->act_bone = bone_parent;

        changed = true;
      }
    }
  }
  else { /* direction == BONE_SELECT_CHILD */
    bPoseChannel *pchan_iter;
    Bone *bone_child = NULL;
    int pass;

    /* first pass, only connected bones (the logical direct child) */
    for (pass = 0; pass < 2 && (bone_child == NULL); pass++) {
      for (pchan_iter = ob->pose->chanbase.first; pchan_iter; pchan_iter = pchan_iter->next) {
        /* possible we have multiple children, some invisible */
        if (PBONE_SELECTABLE(arm, pchan_iter->bone)) {
          if (pchan_iter->parent == pchan_act) {
            if ((pass == 1) || (pchan_iter->bone->flag & BONE_CONNECTED)) {
              bone_child = pchan_iter->bone;
              break;
            }
          }
        }
      }
    }

    if (bone_child) {
      arm->act_bone = bone_child;

      if (!add_to_sel) {
        pchan_act->bone->flag &= ~BONE_SELECTED;
      }
      bone_child->flag |= BONE_SELECTED;

      changed = true;
    }
  }

  if (changed == false) {
    return OPERATOR_CANCELLED;
  }

  ED_pose_bone_select_tag_update(ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_hierarchy(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
      {BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Select Hierarchy";
  ot->idname = "POSE_OT_select_hierarchy";
  ot->description = "Select immediate parent/children of selected bones";

  /* api callbacks */
  ot->exec = pose_select_hierarchy_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(
      ot->srna, "direction", direction_items, BONE_SELECT_PARENT, "Direction", "");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/* -------------------------------------- */

/* modes for select same */
typedef enum ePose_SelectSame_Mode {
  POSE_SEL_SAME_LAYER = 0,
  POSE_SEL_SAME_GROUP = 1,
  POSE_SEL_SAME_KEYINGSET = 2,
} ePose_SelectSame_Mode;

static bool pose_select_same_group(bContext *C, bool extend)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool *group_flags_array;
  bool *group_flags = NULL;
  int groups_len = 0;
  bool changed = false, tagged = false;
  Object *ob_prev = NULL;
  uint ob_index;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len, OB_MODE_POSE);
  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = BKE_object_pose_armature_get(objects[ob_index]);
    bArmature *arm = (ob) ? ob->data : NULL;
    bPose *pose = (ob) ? ob->pose : NULL;

    /* Sanity checks. */
    if (ELEM(NULL, ob, pose, arm)) {
      continue;
    }

    ob->id.tag &= ~LIB_TAG_DOIT;
    groups_len = MAX2(groups_len, BLI_listbase_count(&pose->agroups));
  }

  /* Nothing to do here. */
  if (groups_len == 0) {
    MEM_freeN(objects);
    return false;
  }

  /* alloc a small array to keep track of the groups to use
   * - each cell stores on/off state for whether group should be used
   * - size is (groups_len + 1), since (index = 0) is used for no-group
   */
  groups_len++;
  group_flags_array = MEM_callocN(objects_len * groups_len * sizeof(bool),
                                  "pose_select_same_group");

  group_flags = NULL;
  ob_index = -1;
  ob_prev = NULL;
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object, *ob) {
    if (ob != ob_prev) {
      ob_index++;
      group_flags = group_flags_array + (ob_index * groups_len);
      ob_prev = ob;
    }

    /* keep track of group as group to use later? */
    if (pchan->bone->flag & BONE_SELECTED) {
      group_flags[pchan->agrp_index] = true;
      tagged = true;
    }

    /* deselect all bones before selecting new ones? */
    if ((extend == false) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
      pchan->bone->flag &= ~BONE_SELECTED;
    }
  }
  CTX_DATA_END;

  /* small optimization: only loop through bones a second time if there are any groups tagged */
  if (tagged) {
    group_flags = NULL;
    ob_index = -1;
    ob_prev = NULL;
    /* only if group matches (and is not selected or current bone) */
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
      if (ob != ob_prev) {
        ob_index++;
        group_flags = group_flags_array + (ob_index * groups_len);
        ob_prev = ob;
      }

      if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
        /* check if the group used by this bone is counted */
        if (group_flags[pchan->agrp_index]) {
          pchan->bone->flag |= BONE_SELECTED;
          ob->id.tag |= LIB_TAG_DOIT;
        }
      }
    }
    CTX_DATA_END;
  }

  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    if (ob->id.tag & LIB_TAG_DOIT) {
      ED_pose_bone_select_tag_update(ob);
      changed = true;
    }
  }

  /* Cleanup. */
  MEM_freeN(group_flags_array);
  MEM_freeN(objects);

  return changed;
}

static bool pose_select_same_layer(bContext *C, bool extend)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int *layers_array, *layers = NULL;
  Object *ob_prev = NULL;
  uint ob_index;
  bool changed = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len, OB_MODE_POSE);
  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    ob->id.tag &= ~LIB_TAG_DOIT;
  }

  layers_array = MEM_callocN(objects_len * sizeof(*layers_array), "pose_select_same_layer");

  /* Figure out what bones are selected. */
  layers = NULL;
  ob_prev = NULL;
  ob_index = -1;
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    if (ob != ob_prev) {
      layers = &layers_array[++ob_index];
      ob_prev = ob;
    }

    /* Keep track of layers to use later? */
    if (pchan->bone->flag & BONE_SELECTED) {
      *layers |= pchan->bone->layer;
    }

    /* Deselect all bones before selecting new ones? */
    if ((extend == false) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
      pchan->bone->flag &= ~BONE_SELECTED;
    }
  }
  CTX_DATA_END;

  bool any_layer = false;
  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    if (layers_array[ob_index]) {
      any_layer = true;
      break;
    }
  }

  if (!any_layer) {
    goto cleanup;
  }

  /* Select bones that are on same layers as layers flag. */
  ob_prev = NULL;
  ob_index = -1;
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    if (ob != ob_prev) {
      layers = &layers_array[++ob_index];
      ob_prev = ob;
    }

    /* if bone is on a suitable layer, and the bone can have its selection changed, select it */
    if ((*layers & pchan->bone->layer) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
      pchan->bone->flag |= BONE_SELECTED;
      ob->id.tag |= LIB_TAG_DOIT;
    }
  }
  CTX_DATA_END;

  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    if (ob->id.tag & LIB_TAG_DOIT) {
      ED_pose_bone_select_tag_update(ob);
      changed = true;
    }
  }

cleanup:
  /* Cleanup. */
  MEM_freeN(layers_array);
  MEM_freeN(objects);

  return changed;
}

static bool pose_select_same_keyingset(bContext *C, ReportList *reports, bool extend)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool changed_multi = false;
  KeyingSet *ks = ANIM_scene_get_active_keyingset(CTX_data_scene(C));
  KS_Path *ksp;

  /* sanity checks: validate Keying Set and object */
  if (ks == NULL) {
    BKE_report(reports, RPT_ERROR, "No active Keying Set to use");
    return false;
  }
  else if (ANIM_validate_keyingset(C, NULL, ks) != 0) {
    if (ks->paths.first == NULL) {
      if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
        BKE_report(reports,
                   RPT_ERROR,
                   "Use another Keying Set, as the active one depends on the currently "
                   "selected items or cannot find any targets due to unsuitable context");
      }
      else {
        BKE_report(reports, RPT_ERROR, "Keying Set does not contain any paths");
      }
    }
    return false;
  }

  /* if not extending selection, deselect all selected first */
  if (extend == false) {
    CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones) {
      if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
        pchan->bone->flag &= ~BONE_SELECTED;
      }
    }
    CTX_DATA_END;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len, OB_MODE_POSE);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = BKE_object_pose_armature_get(objects[ob_index]);
    bArmature *arm = (ob) ? ob->data : NULL;
    bPose *pose = (ob) ? ob->pose : NULL;
    bool changed = false;

    /* Sanity checks. */
    if (ELEM(NULL, ob, pose, arm)) {
      continue;
    }

    /* iterate over elements in the Keying Set, setting selection depending on whether
     * that bone is visible or not...
     */
    for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
      /* only items related to this object will be relevant */
      if ((ksp->id == &ob->id) && (ksp->rna_path != NULL)) {
        if (strstr(ksp->rna_path, "bones")) {
          char *boneName = BLI_str_quoted_substrN(ksp->rna_path, "bones[");

          if (boneName) {
            bPoseChannel *pchan = BKE_pose_channel_find_name(pose, boneName);

            if (pchan) {
              /* select if bone is visible and can be affected */
              if (PBONE_SELECTABLE(arm, pchan->bone)) {
                pchan->bone->flag |= BONE_SELECTED;
                changed = true;
              }
            }

            /* free temp memory */
            MEM_freeN(boneName);
          }
        }
      }
    }

    if (changed || !extend) {
      ED_pose_bone_select_tag_update(ob);
      changed_multi = true;
    }
  }
  MEM_freeN(objects);

  return changed_multi;
}

static int pose_select_grouped_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  const ePose_SelectSame_Mode type = RNA_enum_get(op->ptr, "type");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  /* sanity check */
  if (ob->pose == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* selection types */
  switch (type) {
    case POSE_SEL_SAME_LAYER: /* layer */
      changed = pose_select_same_layer(C, extend);
      break;

    case POSE_SEL_SAME_GROUP: /* group */
      changed = pose_select_same_group(C, extend);
      break;

    case POSE_SEL_SAME_KEYINGSET: /* Keying Set */
      changed = pose_select_same_keyingset(C, op->reports, extend);
      break;

    default:
      printf("pose_select_grouped() - Unknown selection type %u\n", type);
      break;
  }

  /* report done status */
  if (changed) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void POSE_OT_select_grouped(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_select_grouped_types[] = {
      {POSE_SEL_SAME_LAYER, "LAYER", 0, "Layer", "Shared layers"},
      {POSE_SEL_SAME_GROUP, "GROUP", 0, "Group", "Shared group"},
      {POSE_SEL_SAME_KEYINGSET,
       "KEYINGSET",
       0,
       "Keying Set",
       "All bones affected by active Keying Set"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Select Grouped";
  ot->description = "Select all visible bones grouped by similar properties";
  ot->idname = "POSE_OT_select_grouped";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_select_grouped_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting everything first");
  ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

/* -------------------------------------- */

/**
 * \note clone of #armature_select_mirror_exec keep in sync
 */
static int pose_select_mirror_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_active_object(C);

  const bool is_weight_paint = (ob_active->mode & OB_MODE_WEIGHT_PAINT) != 0;
  const bool active_only = RNA_boolean_get(op->ptr, "only_active");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len, OB_MODE_POSE);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bPoseChannel *pchan, *pchan_mirror_act = NULL;

    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      const int flag = (pchan->bone->flag & BONE_SELECTED);
      PBONE_PREV_FLAG_SET(pchan, flag);
    }

    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (PBONE_SELECTABLE(arm, pchan->bone)) {
        bPoseChannel *pchan_mirror;
        int flag_new = extend ? PBONE_PREV_FLAG_GET(pchan) : 0;

        if ((pchan_mirror = BKE_pose_channel_get_mirrored(ob->pose, pchan->name)) &&
            (PBONE_VISIBLE(arm, pchan_mirror->bone))) {
          const int flag_mirror = PBONE_PREV_FLAG_GET(pchan_mirror);
          flag_new |= flag_mirror;

          if (pchan->bone == arm->act_bone) {
            pchan_mirror_act = pchan_mirror;
          }

          /* Skip all but the active or its mirror. */
          if (active_only && !ELEM(arm->act_bone, pchan->bone, pchan_mirror->bone)) {
            continue;
          }
        }

        pchan->bone->flag = (pchan->bone->flag & ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) |
                            flag_new;
      }
    }

    if (pchan_mirror_act) {
      arm->act_bone = pchan_mirror_act->bone;

      /* In weightpaint we select the associated vertex group too. */
      if (is_weight_paint) {
        ED_vgroup_select_by_name(ob, pchan_mirror_act->name);
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

    /* Need to tag armature for cow updates, or else selection doesn't update. */
    DEG_id_tag_update(&arm->id, ID_RECALC_COPY_ON_WRITE);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Active/Selected Bone";
  ot->idname = "POSE_OT_select_mirror";
  ot->description = "Mirror the bone selection";

  /* api callbacks */
  ot->exec = pose_select_mirror_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "only_active", false, "Active Only", "Only operate on the active bone");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}
