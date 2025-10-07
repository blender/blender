/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include <cstring>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_keyframing.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "ANIM_armature.hh"
#include "ANIM_bonecolor.hh"
#include "ANIM_keyingsets.hh"

#include "armature_intern.hh"

using blender::Span;
using blender::Vector;

/* ***************** Pose Select Utilities ********************* */

/* NOTE: SEL_TOGGLE is assumed to have already been handled! */
static void pose_do_bone_select(bPoseChannel *pchan, const int select_mode)
{
  /* select pchan only if selectable, but deselect works always */
  switch (select_mode) {
    case SEL_SELECT:
      if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
        pchan->flag |= POSE_SELECTED;
      }
      break;
    case SEL_DESELECT:
      pchan->flag &= ~POSE_SELECTED;
      break;
    case SEL_INVERT:
      if (pchan->flag & POSE_SELECTED) {
        pchan->flag &= ~POSE_SELECTED;
      }
      else if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
        pchan->flag |= POSE_SELECTED;
      }
      break;
  }
}

void ED_pose_bone_select_tag_update(Object *ob)
{
  BLI_assert(ob->type == OB_ARMATURE);
  bArmature *arm = static_cast<bArmature *>(ob->data);
  WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, ob);
  WM_main_add_notifier(NC_GEOM | ND_DATA, ob);

  if (arm->flag & ARM_HAS_VIZ_DEPS) {
    /* mask modifier ('armature' mode), etc. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
}

void ED_pose_bone_select(Object *ob, bPoseChannel *pchan, bool select, bool change_active)
{
  bArmature *arm;

  /* sanity checks */
  /* XXX: actually, we can probably still get away with no object - at most we have no updates */
  if (ELEM(nullptr, ob, ob->pose, pchan, pchan->bone)) {
    return;
  }

  arm = static_cast<bArmature *>(ob->data);

  /* can only change selection state if bone can be modified */
  if (blender::animrig::bone_is_selectable(arm, pchan)) {
    /* change selection state - activate too if selected */
    if (select) {
      pchan->flag |= POSE_SELECTED;
      if (change_active) {
        arm->act_bone = pchan->bone;
      }
    }
    else {
      pchan->flag &= ~POSE_SELECTED;
      if (change_active) {
        arm->act_bone = nullptr;
      }
    }

    /* TODO: select and activate corresponding vgroup? */
    ED_pose_bone_select_tag_update(ob);
  }
}

bool ED_armature_pose_select_pick_bone(const Scene *scene,
                                       ViewLayer *view_layer,
                                       View3D *v3d,
                                       Object *ob,
                                       bPoseChannel *pchan,
                                       const SelectPick_Params &params)
{
  bool found = false;
  bool changed = false;

  if (ob->pose) {
    if (pchan && pchan->bone && ((pchan->bone->flag & BONE_UNSELECTABLE) == 0)) {
      found = true;
    }
  }

  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) && (pchan->flag & POSE_SELECTED)) {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      /* Don't use #BKE_object_pose_base_array_get_unique
       * because we may be selecting from object mode. */
      FOREACH_VISIBLE_BASE_BEGIN (scene, view_layer, v3d, base_iter) {
        Object *ob_iter = base_iter->object;
        if ((ob_iter->type == OB_ARMATURE) && (ob_iter->mode & OB_MODE_POSE)) {
          if (ED_pose_deselect_all(ob_iter, SEL_DESELECT, true)) {
            ED_pose_bone_select_tag_update(ob_iter);
          }
        }
      }
      FOREACH_VISIBLE_BASE_END;
      changed = true;
    }
  }

  if (found) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob_act = BKE_view_layer_active_object_get(view_layer);
    BLI_assert(BKE_view_layer_edit_object_get(view_layer) == nullptr);

    /* If the bone cannot be affected, don't do anything. */
    bArmature *arm = static_cast<bArmature *>(ob->data);

    /* Since we do unified select, we don't shift+select a bone if the
     * armature object was not active yet.
     * NOTE(@ideasman42): special exception for armature mode so we can do multi-select
     * we could check for multi-select explicitly but think its fine to
     * always give predictable behavior in weight paint mode. */
    if ((ob_act == nullptr) || ((ob_act != ob) && (ob_act->mode & OB_MODE_ALL_WEIGHT_PAINT) == 0))
    {
      /* When we are entering into posemode via toggle-select,
       * from another active object - always select the bone. */
      if (params.sel_op == SEL_OP_SET) {
        /* Re-select the bone again later in this function. */
        pchan->flag &= ~POSE_SELECTED;
      }
    }

    switch (params.sel_op) {
      case SEL_OP_ADD: {
        pchan->flag |= POSE_SELECTED;
        arm->act_bone = pchan->bone;
        break;
      }
      case SEL_OP_SUB: {
        pchan->flag &= ~POSE_SELECTED;
        break;
      }
      case SEL_OP_XOR: {
        if (pchan->flag & POSE_SELECTED) {
          /* If not active, we make it active. */
          if (pchan->bone != arm->act_bone) {
            arm->act_bone = pchan->bone;
          }
          else {
            pchan->flag &= ~POSE_SELECTED;
          }
        }
        else {
          pchan->flag |= POSE_SELECTED;
          arm->act_bone = pchan->bone;
        }
        break;
      }
      case SEL_OP_SET: {
        pchan->flag |= POSE_SELECTED;
        arm->act_bone = pchan->bone;
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    if (ob_act) {
      /* In weight-paint we select the associated vertex group too. */
      if (ob_act->mode & OB_MODE_ALL_WEIGHT_PAINT) {
        if (pchan->bone && pchan->bone == arm->act_bone) {
          blender::ed::object::vgroup_select_by_name(ob_act, pchan->bone->name);
          DEG_id_tag_update(&ob_act->id, ID_RECALC_GEOMETRY);
        }
      }
      /* If there are some dependencies for visualizing armature state
       * (e.g. Mask Modifier in 'Armature' mode), force update.
       */
      else if (arm->flag & ARM_HAS_VIZ_DEPS) {
        /* NOTE: ob not ob_act here is intentional - it's the source of the
         *       bones being selected [#37247].
         */
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      /* Tag armature for copy-on-evaluation update (since act_bone is in armature not object). */
      DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
    }

    changed = true;
  }

  return changed || found;
}

bool ED_armature_pose_select_pick_with_buffer(const Scene *scene,
                                              ViewLayer *view_layer,
                                              View3D *v3d,
                                              Base *base,
                                              const GPUSelectResult *hit_results,
                                              const int hits,
                                              const SelectPick_Params &params,
                                              bool do_nearest)
{
  Object *ob = base->object;
  bPoseChannel *nearBone;

  if (!ob || !ob->pose) {
    return false;
  }

  /* Callers happen to already get the active base */
  Base *base_dummy = nullptr;
  nearBone = ED_armature_pick_pchan_from_selectbuffer(
      {base}, hit_results, hits, true, do_nearest, &base_dummy);

  return ED_armature_pose_select_pick_bone(scene, view_layer, v3d, ob, nearBone, params);
}

void ED_armature_pose_select_in_wpaint_mode(const Scene *scene,
                                            ViewLayer *view_layer,
                                            Base *base_select)
{
  BLI_assert(base_select && (base_select->object->type == OB_ARMATURE));
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob_active = BKE_view_layer_active_object_get(view_layer);
  BLI_assert(ob_active && (ob_active->mode & OB_MODE_ALL_WEIGHT_PAINT));

  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob_active, &virtual_modifier_data);
  for (; md; md = md->next) {
    if (md->type == eModifierType_Armature) {
      ArmatureModifierData *amd = reinterpret_cast<ArmatureModifierData *>(md);
      Object *ob_arm = amd->object;
      if (ob_arm != nullptr) {
        Base *base_arm = BKE_view_layer_base_find(view_layer, ob_arm);
        if ((base_arm != nullptr) && (base_arm != base_select) && (base_arm->flag & BASE_SELECTED))
        {
          blender::ed::object::base_select(base_arm, blender::ed::object::BA_DESELECT);
        }
      }
    }
  }
  if ((base_select->flag & BASE_SELECTED) == 0) {
    blender::ed::object::base_select(base_select, blender::ed::object::BA_SELECT);
  }
}

bool ED_pose_deselect_all(Object *ob, int select_mode, const bool ignore_visibility)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);

  /* we call this from outliner too */
  if (ob->pose == nullptr) {
    return false;
  }

  /* Determine if we're selecting or deselecting */
  if (select_mode == SEL_TOGGLE) {
    select_mode = SEL_SELECT;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if (ignore_visibility || blender::animrig::bone_is_visible(arm, pchan)) {
        if (pchan->flag & POSE_SELECTED) {
          select_mode = SEL_DESELECT;
          break;
        }
      }
    }
  }

  /* Set the flags accordingly */
  bool changed = false;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    /* ignore the pchan if it isn't visible or if its selection cannot be changed */
    if (ignore_visibility || blender::animrig::bone_is_visible(arm, pchan)) {
      int flag_prev = pchan->flag;
      pose_do_bone_select(pchan, select_mode);
      changed = (changed || flag_prev != pchan->flag);
    }
  }
  return changed;
}

static bool ed_pose_is_any_selected(Object *ob, bool ignore_visibility)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if (ignore_visibility || blender::animrig::bone_is_visible(arm, pchan)) {
      if (pchan->flag & POSE_SELECTED) {
        return true;
      }
    }
  }
  return false;
}

static bool ed_pose_is_any_selected_multi(const Span<Base *> bases, bool ignore_visibility)
{
  for (Base *base : bases) {
    Object *ob_iter = base->object;
    if (ed_pose_is_any_selected(ob_iter, ignore_visibility)) {
      return true;
    }
  }
  return false;
}

bool ED_pose_deselect_all_multi_ex(const Span<Base *> bases,
                                   int select_mode,
                                   const bool ignore_visibility)
{
  if (select_mode == SEL_TOGGLE) {
    select_mode = ed_pose_is_any_selected_multi(bases, ignore_visibility) ? SEL_DESELECT :
                                                                            SEL_SELECT;
  }

  bool changed_multi = false;
  for (Base *base : bases) {
    Object *ob_iter = base->object;
    if (ED_pose_deselect_all(ob_iter, select_mode, ignore_visibility)) {
      ED_pose_bone_select_tag_update(ob_iter);
      changed_multi = true;
    }
  }
  return changed_multi;
}

bool ED_pose_deselect_all_multi(bContext *C, int select_mode, const bool ignore_visibility)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Vector<Base *> bases = BKE_object_pose_base_array_get_unique(vc.scene, vc.view_layer, vc.v3d);
  return ED_pose_deselect_all_multi_ex(bases, select_mode, ignore_visibility);
}

/* ***************** Selections ********************** */

static void selectconnected_posebonechildren(Object &ob,
                                             bPoseChannel &pose_bone,
                                             const bool extend)
{
  blender::animrig::pose_bone_descendent_depth_iterator(
      *ob.pose, pose_bone, [extend](bPoseChannel &child) {
        if (!child.bone) {
          BLI_assert_unreachable();
          return false;
        }
        /* Stop when unconnected child is encountered, or when unselectable bone is encountered. */
        if (!(child.bone->flag & BONE_CONNECTED) || (child.bone->flag & BONE_UNSELECTABLE)) {
          return false;
        }

        if (extend) {
          child.flag &= ~POSE_SELECTED;
        }
        else {
          child.flag |= POSE_SELECTED;
        }
        return true;
      });
}

/* within active object context */
/* previously known as "selectconnected_posearmature" */
static wmOperatorStatus pose_select_connected_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  bPoseChannel *pchan, *curBone, *next = nullptr;
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  view3d_operator_needs_gpu(C);

  Base *base = nullptr;
  pchan = ED_armature_pick_pchan(C, event->mval, !extend, &base);

  if (!pchan) {
    return OPERATOR_CANCELLED;
  }

  /* Select parents */
  for (curBone = pchan; curBone; curBone = next) {
    /* ignore bone if cannot be selected */
    if ((curBone->flag & BONE_UNSELECTABLE) == 0) {
      if (extend) {
        curBone->flag &= ~POSE_SELECTED;
      }
      else {
        curBone->flag |= POSE_SELECTED;
      }

      if (curBone->bone->flag & BONE_CONNECTED) {
        next = curBone->parent;
      }
      else {
        next = nullptr;
      }
    }
    else {
      next = nullptr;
    }
  }

  /* Select children */
  selectconnected_posebonechildren(*base->object, *pchan, extend);

  ED_outliner_select_sync_from_pose_bone_tag(C);

  ED_pose_bone_select_tag_update(base->object);

  return OPERATOR_FINISHED;
}

static bool pose_select_linked_pick_poll(bContext *C)
{
  return (ED_operator_view3d_active(C) && ED_operator_posemode(C));
}

void POSE_OT_select_linked_pick(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Connected";
  ot->idname = "POSE_OT_select_linked_pick";
  ot->description = "Select bones linked by parent/child connections under the mouse cursor";

  /* callbacks */
  /* leave 'exec' unset */
  ot->invoke = pose_select_connected_invoke;
  ot->poll = pose_select_linked_pick_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus pose_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  bPoseChannel *curBone, *next = nullptr;

  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    if ((pchan->flag & POSE_SELECTED) == 0) {
      continue;
    }

    bArmature *arm = static_cast<bArmature *>(ob->data);

    /* Select parents */
    for (curBone = pchan; curBone; curBone = next) {
      if (blender::animrig::bone_is_selectable(arm, curBone)) {
        curBone->flag |= POSE_SELECTED;

        if (curBone->bone->flag & BONE_CONNECTED) {
          next = curBone->parent;
        }
        else {
          next = nullptr;
        }
      }
      else {
        next = nullptr;
      }
    }

    /* Select children */
    selectconnected_posebonechildren(*ob, *pchan, false);
    ED_pose_bone_select_tag_update(ob);
  }
  CTX_DATA_END;

  ED_outliner_select_sync_from_pose_bone_tag(C);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Connected";
  ot->idname = "POSE_OT_select_linked";
  ot->description = "Select all bones linked by parent/child connections to the current selection";

  /* callbacks */
  ot->exec = pose_select_linked_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

static wmOperatorStatus pose_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  Scene *scene = CTX_data_scene(C);
  int multipaint = scene->toolsettings->multipaint;

  if (action == SEL_TOGGLE) {
    action = CTX_DATA_COUNT(C, selected_pose_bones) ? SEL_DESELECT : SEL_SELECT;
  }

  Object *ob_prev = nullptr;

  /* Set the flags. */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    pose_do_bone_select(pchan, action);

    if (ob_prev != ob) {
      /* Weight-paint or mask modifiers need depsgraph updates. */
      if (multipaint || (arm->flag & ARM_HAS_VIZ_DEPS)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      /* need to tag armature for cow updates, or else selection doesn't update */
      DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
      ob_prev = ob;
    }
  }
  CTX_DATA_END;

  ED_outliner_select_sync_from_pose_bone_tag(C);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, nullptr);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->idname = "POSE_OT_select_all";
  ot->description = "Toggle selection status of all bones";

  /* API callbacks. */
  ot->exec = pose_de_select_all_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/* -------------------------------------- */

static wmOperatorStatus pose_select_parent_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan, *parent;

  /* Determine if there is an active bone */
  pchan = CTX_data_active_pose_bone(C);
  if (pchan) {
    parent = pchan->parent;
    if ((parent) && !(parent->drawflag & PCHAN_DRAW_HIDDEN) &&
        !(parent->bone->flag & BONE_UNSELECTABLE))
    {
      parent->flag |= POSE_SELECTED;
      arm->act_bone = parent->bone;
    }
    else {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_pose_bone_tag(C);

  ED_pose_bone_select_tag_update(ob);
  return OPERATOR_FINISHED;
}

void POSE_OT_select_parent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Parent Bone";
  ot->idname = "POSE_OT_select_parent";
  ot->description = "Select bones that are parents of the currently selected bones";

  /* API callbacks. */
  ot->exec = pose_select_parent_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

static wmOperatorStatus pose_select_constraint_target_exec(bContext *C, wmOperator * /*op*/)
{
  bool found = false;

  CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones) {
    if (pchan->flag & POSE_SELECTED) {
      LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
        ListBase targets = {nullptr, nullptr};
        if (BKE_constraint_targets_get(con, &targets)) {
          LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
            Object *ob = ct->tar;

            /* Any armature that is also in pose mode should be selected. */
            if ((ct->subtarget[0] != '\0') && (ob != nullptr) && (ob->type == OB_ARMATURE) &&
                (ob->mode == OB_MODE_POSE))
            {
              bPoseChannel *pchanc = BKE_pose_channel_find_name(ob->pose, ct->subtarget);
              if ((pchanc) && !(pchanc->bone->flag & BONE_UNSELECTABLE)) {
                pchanc->flag |= POSE_SELECTED;
                ED_pose_bone_select_tag_update(ob);
                found = true;
              }
            }
          }

          BKE_constraint_targets_flush(con, &targets, true);
        }
      }
    }
  }
  CTX_DATA_END;

  if (!found) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_pose_bone_tag(C);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_constraint_target(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Constraint Target";
  ot->idname = "POSE_OT_select_constraint_target";
  ot->description = "Select bones used as targets for the currently selected bones";

  /* API callbacks. */
  ot->exec = pose_select_constraint_target_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

/* No need to convert to multi-objects. Just like we keep the non-active bones
 * selected we then keep the non-active objects untouched (selected/unselected). */
static wmOperatorStatus pose_select_hierarchy_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan_act;
  int direction = RNA_enum_get(op->ptr, "direction");
  const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  pchan_act = BKE_pose_channel_active_if_bonecoll_visible(ob);
  if (pchan_act == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (direction == BONE_SELECT_PARENT) {
    if (pchan_act->parent) {
      Bone *bone_parent;
      bone_parent = pchan_act->parent->bone;

      if (blender::animrig::bone_is_selectable(arm, bone_parent)) {
        if (!add_to_sel) {
          pchan_act->flag &= ~POSE_SELECTED;
        }
        pchan_act->parent->flag |= POSE_SELECTED;
        arm->act_bone = bone_parent;

        changed = true;
      }
    }
  }
  else { /* direction == BONE_SELECT_CHILD */
    bPoseChannel *bone_child = nullptr;
    int pass;

    /* first pass, only connected bones (the logical direct child) */
    for (pass = 0; pass < 2 && (bone_child == nullptr); pass++) {
      LISTBASE_FOREACH (bPoseChannel *, pchan_iter, &ob->pose->chanbase) {
        /* possible we have multiple children, some invisible */
        if (blender::animrig::bone_is_selectable(arm, pchan_iter)) {
          if (pchan_iter->parent == pchan_act) {
            if ((pass == 1) || (pchan_iter->bone->flag & BONE_CONNECTED)) {
              bone_child = pchan_iter;
              break;
            }
          }
        }
      }
    }

    if (bone_child) {
      arm->act_bone = bone_child->bone;

      if (!add_to_sel) {
        pchan_act->flag &= ~POSE_SELECTED;
      }
      bone_child->flag |= POSE_SELECTED;

      changed = true;
    }
  }

  if (changed == false) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_pose_bone_tag(C);

  ED_pose_bone_select_tag_update(ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_hierarchy(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
      {BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Select Hierarchy";
  ot->idname = "POSE_OT_select_hierarchy";
  ot->description = "Select immediate parent/children of selected bones";

  /* API callbacks. */
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

/* Modes for the `select_grouped` operator. */
enum class SelectRelatedMode {
  SAME_COLLECTION = 0,
  SAME_COLOR,
  SAME_KEYINGSET,
  CHILDREN,
  IMMEDIATE_CHILDREN,
  PARENT,
  SIBLINGS,
};

static bool pose_select_same_color(bContext *C, const bool extend)
{
  /* Get a set of all the colors of the selected bones. */
  blender::Set<blender::animrig::BoneColor> used_colors;
  blender::Set<Object *> updated_objects;
  bool changed_any_selection = false;

  /* Old approach that we may want to reinstate behind some option at some point. This will match
   * against the colors of all selected bones, instead of just the active one. It also explains why
   * there is a set of colors to begin with.
   *
   * CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones) {
   *   auto color = blender::animrig::ANIM_bonecolor_posebone_get(pchan);
   *   used_colors.add(color);
   * }
   * CTX_DATA_END;
   */
  if (!extend) {
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
      pchan->flag &= ~POSE_SELECTED;
      updated_objects.add(ob);
      changed_any_selection = true;
    }
    CTX_DATA_END;
  }

  /* Use the color of the active pose bone. */
  bPoseChannel *active_pose_bone = CTX_data_active_pose_bone(C);
  auto color = blender::animrig::ANIM_bonecolor_posebone_get(active_pose_bone);
  used_colors.add(color);

  /* Select all visible bones that have the same color. */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    Bone *bone = pchan->bone;
    if ((bone->flag & BONE_UNSELECTABLE) && (pchan->flag & POSE_SELECTED)) {
      /* Skip bones that are unselectable or already selected. */
      continue;
    }

    auto color = blender::animrig::ANIM_bonecolor_posebone_get(pchan);
    if (!used_colors.contains(color)) {
      continue;
    }

    pchan->flag |= POSE_SELECTED;
    changed_any_selection = true;
    updated_objects.add(ob);
  }
  CTX_DATA_END;

  if (!changed_any_selection) {
    return false;
  }

  for (Object *ob : updated_objects) {
    ED_pose_bone_select_tag_update(ob);
  }
  return true;
}

static bool pose_select_same_collection(bContext *C, const bool extend)
{
  bool changed_any_selection = false;
  blender::Set<Object *> updated_objects;

  /* Refuse to do anything if there is no active pose bone. */
  bPoseChannel *active_pchan = CTX_data_active_pose_bone(C);
  if (!active_pchan) {
    return false;
  }

  if (!extend) {
    /* Deselect all the bones. */
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
      pchan->flag &= ~POSE_SELECTED;
      updated_objects.add(ob);
      changed_any_selection = true;
    }
    CTX_DATA_END;
  }

  /* Build a set of bone collection names, to allow cross-Armature selection. */
  blender::Set<std::string> collection_names;
  LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &active_pchan->bone->runtime.collections)
  {
    collection_names.add(bcoll_ref->bcoll->name);
  }

  /* Select all bones that match any of the collection names. */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
    Bone *bone = pchan->bone;
    if ((pchan->flag & POSE_SELECTED) && bone->flag & BONE_UNSELECTABLE) {
      continue;
    }

    LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &bone->runtime.collections) {
      if (!collection_names.contains(bcoll_ref->bcoll->name)) {
        continue;
      }

      pchan->flag |= POSE_SELECTED;
      changed_any_selection = true;
      updated_objects.add(ob);
    }
  }
  CTX_DATA_END;

  for (Object *ob : updated_objects) {
    ED_pose_bone_select_tag_update(ob);
  }

  return changed_any_selection;
}

/* Useful to get the selection before modifying it. */
static blender::Set<bPoseChannel *> get_selected_pose_bones(Object *pose_object)
{
  blender::Set<bPoseChannel *> selected_pose_bones;
  bArmature *arm = static_cast<bArmature *>((pose_object) ? pose_object->data : nullptr);
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose_object->pose->chanbase) {
    if (blender::animrig::bone_is_selected(arm, pchan)) {
      selected_pose_bones.add(pchan);
    }
  }
  return selected_pose_bones;
}

static bool pose_bone_is_below_one_of(bPoseChannel &bone,
                                      const blender::Set<bPoseChannel *> &potential_parents)
{
  bPoseChannel *bone_iter = &bone;
  while (bone_iter) {
    if (potential_parents.contains(bone_iter)) {
      return true;
    }
    bone_iter = bone_iter->parent;
  }
  return false;
}

static void deselect_pose_bones(const blender::Set<bPoseChannel *> &pose_bones)
{
  for (bPoseChannel *pose_bone : pose_bones) {
    if (!pose_bone) {
      /* There may be a nullptr in the set if selecting siblings of root bones. */
      continue;
    }
    pose_bone->flag &= ~POSE_SELECTED;
  }
}

/* Selects children of currently selected bones in all objects in pose mode. If `all` is true, a
 * bone will be selected if any bone in it's parent hierarchy is selected. If false, only bones
 * whose direct parent is selected are changed. */
static bool pose_select_children(bContext *C, const bool all, const bool extend)
{
  Vector<Object *> objects = BKE_object_pose_array_get_unique(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C));

  bool changed_any_selection = false;

  for (Object *pose_object : objects) {
    bArmature *arm = static_cast<bArmature *>(pose_object->data);
    BLI_assert(arm);
    blender::Set<bPoseChannel *> selected_pose_bones = get_selected_pose_bones(pose_object);
    if (!extend) {
      deselect_pose_bones(selected_pose_bones);
    }
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose_object->pose->chanbase) {
      if (!blender::animrig::bone_is_selectable(arm, pchan)) {
        continue;
      }
      if (all) {
        if (pose_bone_is_below_one_of(*pchan, selected_pose_bones)) {
          pose_do_bone_select(pchan, SEL_SELECT);
          changed_any_selection = true;
        }
      }
      else {
        if (selected_pose_bones.contains(pchan->parent)) {
          pose_do_bone_select(pchan, SEL_SELECT);
          changed_any_selection = true;
        }
      }
    }
    ED_pose_bone_select_tag_update(pose_object);
  }

  return changed_any_selection;
}

static bool pose_select_parents(bContext *C, const bool extend)
{
  Vector<Object *> objects = BKE_object_pose_array_get_unique(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C));

  bool changed_any_selection = false;
  for (Object *pose_object : objects) {
    bArmature *arm = static_cast<bArmature *>(pose_object->data);
    BLI_assert(arm);
    blender::Set<bPoseChannel *> selected_pose_bones = get_selected_pose_bones(pose_object);
    if (!extend) {
      deselect_pose_bones(selected_pose_bones);
    }
    for (bPoseChannel *pchan : selected_pose_bones) {
      if (!pchan->parent) {
        continue;
      }
      if (!blender::animrig::bone_is_selectable(arm, pchan->parent->bone)) {
        continue;
      }
      pose_do_bone_select(pchan->parent, SEL_SELECT);
      changed_any_selection = true;
    }
    ED_pose_bone_select_tag_update(pose_object);
  }
  return changed_any_selection;
}

static bool pose_select_siblings(bContext *C, const bool extend)
{
  Vector<Object *> objects = BKE_object_pose_array_get_unique(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C));

  bool changed_any_selection = false;
  for (Object *pose_object : objects) {
    bArmature *arm = static_cast<bArmature *>(pose_object->data);
    BLI_assert(arm);
    blender::Set<bPoseChannel *> parents_of_selected;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose_object->pose->chanbase) {
      if (blender::animrig::bone_is_selected(arm, pchan)) {
        parents_of_selected.add(pchan->parent);
      }
    }
    if (!extend) {
      deselect_pose_bones(parents_of_selected);
    }
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose_object->pose->chanbase) {
      if (!blender::animrig::bone_is_selectable(arm, pchan)) {
        continue;
      }
      /* Checking if the bone is already selected so `changed_any_selection` stays true to its
       * word. */
      if (parents_of_selected.contains(pchan->parent) &&
          !blender::animrig::bone_is_selected(arm, pchan))
      {
        pose_do_bone_select(pchan, SEL_SELECT);
        changed_any_selection = true;
      }
    }
    ED_pose_bone_select_tag_update(pose_object);
  }
  return changed_any_selection;
}

static bool pose_select_same_keyingset(bContext *C, ReportList *reports, bool extend)
{
  using namespace blender::animrig;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool changed_multi = false;
  KeyingSet *ks = scene_get_active_keyingset(CTX_data_scene(C));

  /* sanity checks: validate Keying Set and object */
  if (ks == nullptr) {
    BKE_report(reports, RPT_ERROR, "No active Keying Set to use");
    return false;
  }
  if (validate_keyingset(C, nullptr, ks) != ModifyKeyReturn::SUCCESS) {
    if (ks->paths.first == nullptr) {
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
        pchan->flag &= ~POSE_SELECTED;
      }
    }
    CTX_DATA_END;
  }

  Vector<Object *> objects = BKE_object_pose_array_get_unique(scene, view_layer, CTX_wm_view3d(C));

  for (const int ob_index : objects.index_range()) {
    Object *ob = BKE_object_pose_armature_get(objects[ob_index]);
    bArmature *arm = static_cast<bArmature *>((ob) ? ob->data : nullptr);
    bPose *pose = (ob) ? ob->pose : nullptr;
    bool changed = false;

    /* Sanity checks. */
    if (ELEM(nullptr, ob, pose, arm)) {
      continue;
    }

    /* iterate over elements in the Keying Set, setting selection depending on whether
     * that bone is visible or not...
     */
    LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
      /* only items related to this object will be relevant */
      if ((ksp->id == &ob->id) && (ksp->rna_path != nullptr)) {
        bPoseChannel *pchan = nullptr;
        char boneName[sizeof(pchan->name)];
        if (!BLI_str_quoted_substr(ksp->rna_path, "bones[", boneName, sizeof(boneName))) {
          continue;
        }
        pchan = BKE_pose_channel_find_name(pose, boneName);

        if (pchan) {
          /* select if bone is visible and can be affected */
          if (blender::animrig::bone_is_selectable(arm, pchan)) {
            pchan->flag |= POSE_SELECTED;
            changed = true;
          }
        }
      }
    }

    if (changed || !extend) {
      ED_pose_bone_select_tag_update(ob);
      changed_multi = true;
    }
  }

  return changed_multi;
}

static wmOperatorStatus pose_select_grouped_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  const SelectRelatedMode mode = SelectRelatedMode(RNA_enum_get(op->ptr, "type"));
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  /* sanity check */
  if (ob->pose == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* selection types */
  switch (mode) {
    case SelectRelatedMode::SAME_COLLECTION:
      changed = pose_select_same_collection(C, extend);
      break;

    case SelectRelatedMode::SAME_COLOR:
      changed = pose_select_same_color(C, extend);
      break;

    case SelectRelatedMode::SAME_KEYINGSET:
      changed = pose_select_same_keyingset(C, op->reports, extend);
      break;

    case SelectRelatedMode::CHILDREN:
      changed = pose_select_children(C, true, extend);
      break;

    case SelectRelatedMode::IMMEDIATE_CHILDREN:
      changed = pose_select_children(C, false, extend);
      break;

    case SelectRelatedMode::PARENT:
      changed = pose_select_parents(C, extend);
      break;

    case SelectRelatedMode::SIBLINGS:
      changed = pose_select_siblings(C, extend);
      break;

    default:
      printf("pose_select_grouped() - Unknown selection type %d\n", int(mode));
      break;
  }

  /* report done status */
  if (changed) {
    ED_outliner_select_sync_from_pose_bone_tag(C);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void POSE_OT_select_grouped(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_select_grouped_types[] = {
      {int(SelectRelatedMode::SAME_COLLECTION),
       "COLLECTION",
       0,
       "Collection",
       "Same collections as the active bone"},
      {int(SelectRelatedMode::SAME_COLOR), "COLOR", 0, "Color", "Same color as the active bone"},
      {int(SelectRelatedMode::SAME_KEYINGSET),
       "KEYINGSET",
       0,
       "Keying Set",
       "All bones affected by active Keying Set"},
      {int(SelectRelatedMode::CHILDREN),
       "CHILDREN",
       0,
       "Children",
       "Select all children of currently selected bones"},
      {int(SelectRelatedMode::IMMEDIATE_CHILDREN),
       "CHILDREN_IMMEDIATE",
       0,
       "Immediate Children",
       "Select direct children of currently selected bones"},
      {int(SelectRelatedMode::PARENT),
       "PARENT",
       0,
       "Parents",
       "Select the parents of currently selected bones"},
      {int(SelectRelatedMode::SIBLINGS),
       "SIBLINGS",
       0,
       "Siblings",
       "Select all bones that have the same parent as currently selected bones"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Select Grouped";
  ot->description = "Select all visible bones grouped by similar properties";
  ot->idname = "POSE_OT_select_grouped";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_select_grouped_exec;
  ot->poll = ED_operator_posemode; /* TODO: expand to support edit mode as well. */

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

/* Add the given selection flags to the bone flags. */
static void bone_selection_flags_add(bPoseChannel *pchan, const ePchan_Flag new_selection_flags)
{
  pchan->flag |= new_selection_flags;
}

/* Set the bone flags to the given selection flags. */
static void bone_selection_flags_set(bPoseChannel *pchan, const ePchan_Flag new_selection_flags)
{
  pchan->flag = new_selection_flags;
}

/**
 * \note clone of #armature_select_mirror_exec keep in sync
 */
static wmOperatorStatus pose_select_mirror_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_active_object(C);

  const bool is_weight_paint = (ob_active->mode & OB_MODE_WEIGHT_PAINT) != 0;
  const bool active_only = RNA_boolean_get(op->ptr, "only_active");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  const auto set_bone_selection_flags = extend ? bone_selection_flags_add :
                                                 bone_selection_flags_set;

  Vector<Object *> objects = BKE_object_pose_array_get_unique(scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bPoseChannel *pchan_mirror_act = nullptr;

    /* Remember the pre-mirroring selection flags of the bones. */
    blender::Map<bPoseChannel *, ePchan_Flag> old_selection_flags;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      /* Treat invisible bones as deselected. */
      const int flags = blender::animrig::bone_is_visible(arm, pchan) ? pchan->flag : 0;

      old_selection_flags.add_new(pchan, ePchan_Flag(flags));
    }

    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if (!blender::animrig::bone_is_selectable(arm, pchan)) {
        continue;
      }

      bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(ob->pose, pchan->name);
      if (!pchan_mirror) {
        /* If a bone cannot be mirrored, keep its flags as-is. This makes it possible to select
         * the spine and an arm, and still flip the selection to the other arm (without losing
         * the selection on the spine). */
        continue;
      }

      if (pchan->bone == arm->act_bone) {
        pchan_mirror_act = pchan_mirror;
      }

      /* If active-only, don't touch unrelated bones. */
      if (active_only && !ELEM(arm->act_bone, pchan->bone, pchan_mirror->bone)) {
        continue;
      }

      const ePchan_Flag flags_mirror = old_selection_flags.lookup(pchan_mirror);
      set_bone_selection_flags(pchan, flags_mirror);
    }

    if (pchan_mirror_act) {
      arm->act_bone = pchan_mirror_act->bone;

      /* In weight-paint we select the associated vertex group too. */
      if (is_weight_paint) {
        blender::ed::object::vgroup_select_by_name(ob_active, pchan_mirror_act->name);
        DEG_id_tag_update(&ob_active->id, ID_RECALC_GEOMETRY);
      }
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

    /* Need to tag armature for cow updates, or else selection doesn't update. */
    DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
  }

  ED_outliner_select_sync_from_pose_bone_tag(C);

  return OPERATOR_FINISHED;
}

void POSE_OT_select_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Mirror";
  ot->idname = "POSE_OT_select_mirror";
  ot->description = "Mirror the bone selection";

  /* API callbacks. */
  ot->exec = pose_select_mirror_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "only_active", false, "Active Only", "Only operate on the active bone");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}
