/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"

#include "BIK_api.h"
#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_animdata.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* ------------------------------------------------------------------- */
/** \name Constraint Data Accessors
 * \{ */

ListBase *constraint_active_list(Object *ob)
{
  if (ob == nullptr) {
    return nullptr;
  }

  if (ob->mode & OB_MODE_POSE) {
    bPoseChannel *pchan;

    pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);
    if (pchan) {
      return &pchan->constraints;
    }
  }
  else {
    return &ob->constraints;
  }

  return nullptr;
}

ListBase *pose_constraint_list(const bContext *C)
{
  bPoseChannel *pose_bone = static_cast<bPoseChannel *>(CTX_data_pointer_get(C, "pose_bone").data);
  if (pose_bone == nullptr) {
    pose_bone = static_cast<bPoseChannel *>(CTX_data_pointer_get(C, "active_pose_bone").data);
    if (pose_bone == nullptr) {
      return nullptr;
    }
  }

  return &pose_bone->constraints;
}

ListBase *constraint_list_from_constraint(Object *ob, bConstraint *con, bPoseChannel **r_pchan)
{
  if (r_pchan) {
    *r_pchan = nullptr;
  }

  if (ELEM(nullptr, ob, con)) {
    return nullptr;
  }

  /* try object constraints first */
  if (BLI_findindex(&ob->constraints, con) != -1) {
    return &ob->constraints;
  }

  /* if armature, try pose bones too */
  if (ob->pose) {
    /* try each bone in order
     * NOTE: it's not possible to directly look up the active bone yet, so this will have to do
     */
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if (BLI_findindex(&pchan->constraints, con) != -1) {

        if (r_pchan) {
          *r_pchan = pchan;
        }

        return &pchan->constraints;
      }
    }
  }

  /* done */
  return nullptr;
}

bConstraint *constraint_active_get(Object *ob)
{
  return BKE_constraints_active_get(constraint_active_list(ob));
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Add Constraint Utilities
 * \{ */

/* helper function for add_constriant - sets the last target for the active constraint */
static void set_constraint_nth_target(bConstraint *con,
                                      Object *target,
                                      const char subtarget[],
                                      int index)
{
  ListBase targets = {nullptr, nullptr};
  bConstraintTarget *ct;
  int targets_num, i;

  if (BKE_constraint_targets_get(con, &targets)) {
    targets_num = BLI_listbase_count(&targets);

    if (index < 0) {
      if (abs(index) < targets_num) {
        index = targets_num - abs(index);
      }
      else {
        index = targets_num - 1;
      }
    }
    else if (index >= targets_num) {
      index = targets_num - 1;
    }

    for (ct = static_cast<bConstraintTarget *>(targets.first), i = 0; ct; ct = ct->next, i++) {
      if (i == index) {
        ct->tar = target;
        STRNCPY_UTF8(ct->subtarget, subtarget);
        break;
      }
    }

    BKE_constraint_targets_flush(con, &targets, false);
  }
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Constraint Sanity Testing
 * \{ */

static void test_constraint(
    Main *bmain, Object *owner, bPoseChannel *pchan, bConstraint *con, int type)
{
  ListBase targets = {nullptr, nullptr};
  bool check_targets = true;

  /* clear disabled-flag first */
  con->flag &= ~CONSTRAINT_DISABLE;

  if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
    bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);

    /* Bad: we need a separate set of checks here as pole-target is optional...
     * otherwise pole-target must exist too or else the constraint is deemed invalid. */

    /* default IK check ... */
    if (BKE_object_exists_check(bmain, data->tar) == 0) {
      data->tar = nullptr;
      con->flag |= CONSTRAINT_DISABLE;
    }
    else if (data->tar == owner) {
      if (!BKE_armature_find_bone_name(BKE_armature_from_object(owner), data->subtarget)) {
        con->flag |= CONSTRAINT_DISABLE;
      }
    }

    if (data->poletar) {
      if (BKE_object_exists_check(bmain, data->poletar) == 0) {
        data->poletar = nullptr;
        con->flag |= CONSTRAINT_DISABLE;
      }
      else if (data->poletar == owner) {
        if (!BKE_armature_find_bone_name(BKE_armature_from_object(owner), data->polesubtarget)) {
          con->flag |= CONSTRAINT_DISABLE;
        }
      }
    }
    /* ... can be overwritten here */
    BIK_test_constraint(owner, con);
    /* targets have already been checked for this */
    check_targets = false;
  }
  else if (con->type == CONSTRAINT_TYPE_PIVOT) {
    bPivotConstraint *data = static_cast<bPivotConstraint *>(con->data);

    /* target doesn't have to exist, but if it is non-null, it must exist! */
    if (data->tar && BKE_object_exists_check(bmain, data->tar) == 0) {
      data->tar = nullptr;
      con->flag |= CONSTRAINT_DISABLE;
    }
    else if (data->tar == owner) {
      if (!BKE_armature_find_bone_name(BKE_armature_from_object(owner), data->subtarget)) {
        con->flag |= CONSTRAINT_DISABLE;
      }
    }

    /* targets have already been checked for this */
    check_targets = false;
  }
  else if (con->type == CONSTRAINT_TYPE_ACTION) {
    bActionConstraint *data = static_cast<bActionConstraint *>(con->data);

    /* validate action */
    if (data->act == nullptr) {
      /* must have action */
      con->flag |= CONSTRAINT_DISABLE;
    }
    else {
      if (animrig::legacy::action_treat_as_legacy(*data->act)) {
        if (!ELEM(data->act->idroot, ID_OB, 0)) {
          /* Only object-rooted actions can be used. */
          data->act = nullptr;
          con->flag |= CONSTRAINT_DISABLE;
        }
      }
      else {
        /* The slot was assigned, so assume that it is suitable to animate the
         * owner (only suitable slots appear in the drop-down). */
        animrig::Action &action = data->act->wrap();
        animrig::Slot *slot = action.slot_for_handle(data->action_slot_handle);
        if (!slot) {
          con->flag |= CONSTRAINT_DISABLE;
        }
      }
    }

    /* Skip target checking if we're not using it */
    if (data->flag & ACTCON_USE_EVAL_TIME) {
      check_targets = false;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
    bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);

    /* don't allow track/up axes to be the same */
    if (data->upflag == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->upflag + 3 == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_TRACKTO) {
    bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);

    /* don't allow track/up axes to be the same */
    if (data->reserved2 == data->reserved1) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->reserved2 + 3 == data->reserved1) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_LOCKTRACK) {
    bLockTrackConstraint *data = static_cast<bLockTrackConstraint *>(con->data);

    if (data->lockflag == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->lockflag + 3 == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
    bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);

    /* if the number of points does not match the amount required by the chain length,
     * free the points array and request a rebind...
     */
    if ((data->points == nullptr) || (data->numpoints != data->chainlen + 1)) {
      MEM_SAFE_FREE(data->points);
      data->numpoints = 0;

      /* clear the bound flag, forcing a rebind next time this is evaluated */
      data->flag &= ~CONSTRAINT_SPLINEIK_BOUND;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
    bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0) {
      if (data->clip != nullptr && data->track[0]) {
        MovieTracking *tracking = &data->clip->tracking;
        MovieTrackingObject *tracking_object;

        if (data->object[0]) {
          tracking_object = BKE_tracking_object_get_named(tracking, data->object);
        }
        else {
          tracking_object = BKE_tracking_object_get_camera(tracking);
        }

        if (!tracking_object) {
          con->flag |= CONSTRAINT_DISABLE;
        }
        else {
          if (!BKE_tracking_object_find_track_with_name(tracking_object, data->track)) {
            con->flag |= CONSTRAINT_DISABLE;
          }
        }
      }
      else {
        con->flag |= CONSTRAINT_DISABLE;
      }
    }
  }
  else if (con->type == CONSTRAINT_TYPE_CAMERASOLVER) {
    bCameraSolverConstraint *data = static_cast<bCameraSolverConstraint *>(con->data);

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0 && (data->clip == nullptr)) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
    bObjectSolverConstraint *data = static_cast<bObjectSolverConstraint *>(con->data);

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0 && (data->clip == nullptr)) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
    bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);

    if ((data->cache_file == nullptr) || (data->object_path[0] == '\0')) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }

  /* Check targets for constraints */
  if (check_targets && BKE_constraint_targets_get(con, &targets)) {
    /* disable and clear constraints targets that are incorrect */
    LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
      /* general validity checks (for those constraints that need this) */
      if (BKE_object_exists_check(bmain, ct->tar) == 0) {
        /* object doesn't exist, but constraint requires target */
        ct->tar = nullptr;
        con->flag |= CONSTRAINT_DISABLE;
      }
      else if (ct->tar == owner) {
        if (type == CONSTRAINT_OBTYPE_BONE) {
          if (!BKE_armature_find_bone_name(BKE_armature_from_object(owner), ct->subtarget)) {
            /* bone must exist in armature... */
            /* TODO: clear subtarget? */
            con->flag |= CONSTRAINT_DISABLE;
          }
          else if (STREQ(pchan->name, ct->subtarget)) {
            /* cannot target self */
            ct->subtarget[0] = '\0';
            con->flag |= CONSTRAINT_DISABLE;
          }
        }
        else {
          /* cannot use self as target */
          ct->tar = nullptr;
          con->flag |= CONSTRAINT_DISABLE;
        }
      }

      /* target checks for specific constraints */
      if (ELEM(con->type,
               CONSTRAINT_TYPE_FOLLOWPATH,
               CONSTRAINT_TYPE_CLAMPTO,
               CONSTRAINT_TYPE_SPLINEIK))
      {
        if (ct->tar) {
          /* The object type check is only needed here in case we have a placeholder
           * object assigned (because the library containing the curve is missing).
           *
           * In other cases it should be impossible to have a type mismatch.
           */
          if (ct->tar->type != OB_CURVES_LEGACY) {
            con->flag |= CONSTRAINT_DISABLE;
          }
          else {
            Curve *cu = static_cast<Curve *>(ct->tar->data);

            /* auto-set 'Path' setting on curve so this works. */
            cu->flag |= CU_PATH;
          }
        }
      }
      else if (con->type == CONSTRAINT_TYPE_ARMATURE) {
        if (ct->tar) {
          /* The object type check is only needed here in case we have a placeholder
           * object assigned (because the library containing the armature is missing).
           *
           * In other cases it should be impossible to have a type mismatch.
           */
          if (ct->tar->type != OB_ARMATURE) {
            con->flag |= CONSTRAINT_DISABLE;
          }
          else if (!BKE_armature_find_bone_name(BKE_armature_from_object(ct->tar), ct->subtarget))
          {
            /* bone must exist in armature... */
            con->flag |= CONSTRAINT_DISABLE;
          }
        }
      }
    }

    /* free any temporary targets */
    BKE_constraint_targets_flush(con, &targets, false);
  }
  else if (check_targets) {
    /* constraints with empty target list that actually require targets */
    if (ELEM(con->type, CONSTRAINT_TYPE_ARMATURE)) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
}

static int constraint_type_get(Object *owner, bPoseChannel *pchan)
{
  int type;
  /* Check parents */
  if (pchan) {
    switch (owner->type) {
      case OB_ARMATURE:
        type = CONSTRAINT_OBTYPE_BONE;
        break;
      default:
        type = CONSTRAINT_OBTYPE_OBJECT;
        break;
    }
  }
  else {
    type = CONSTRAINT_OBTYPE_OBJECT;
  }
  return type;
}

/* checks validity of object pointers, and NULLs,
 * if Bone doesn't exist it sets the CONSTRAINT_DISABLE flag.
 */
static void test_constraints(Main *bmain, Object *ob, bPoseChannel *pchan)
{
  ListBase *conlist = nullptr;
  int type;

  if (ob == nullptr) {
    return;
  }

  type = constraint_type_get(ob, pchan);

  /* Get the constraint list for this object */
  switch (type) {
    case CONSTRAINT_OBTYPE_OBJECT:
      conlist = &ob->constraints;
      break;
    case CONSTRAINT_OBTYPE_BONE:
      conlist = &pchan->constraints;
      break;
  }

  /* Check all constraints - is constraint valid? */
  if (conlist) {
    LISTBASE_FOREACH (bConstraint *, curcon, conlist) {
      test_constraint(bmain, ob, pchan, curcon, type);
    }
  }
}

void object_test_constraints(Main *bmain, Object *ob)
{
  if (ob->constraints.first) {
    test_constraints(bmain, ob, nullptr);
  }

  if (ob->type == OB_ARMATURE && ob->pose) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if (pchan->constraints.first) {
        test_constraints(bmain, ob, pchan);
      }
    }
  }
}

static void object_test_constraint(Main *bmain, Object *ob, bConstraint *con)
{
  if (ob->type == OB_ARMATURE && ob->pose) {
    if (BLI_findindex(&ob->constraints, con) != -1) {
      test_constraint(bmain, ob, nullptr, con, CONSTRAINT_OBTYPE_OBJECT);
    }
    else {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if (BLI_findindex(&pchan->constraints, con) != -1) {
          test_constraint(bmain, ob, pchan, con, CONSTRAINT_OBTYPE_BONE);
          break;
        }
      }
    }
  }
  else {
    test_constraint(bmain, ob, nullptr, con, CONSTRAINT_OBTYPE_OBJECT);
  }
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Generic Functions for Operators Using Constraint Names and Data Context
 * \{ */

#define EDIT_CONSTRAINT_OWNER_OBJECT 0
#define EDIT_CONSTRAINT_OWNER_BONE 1

static const EnumPropertyItem constraint_owner_items[] = {
    {EDIT_CONSTRAINT_OWNER_OBJECT,
     "OBJECT",
     0,
     "Object",
     "Edit a constraint on the active object"},
    {EDIT_CONSTRAINT_OWNER_BONE, "BONE", 0, "Bone", "Edit a constraint on the active bone"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool edit_constraint_poll_generic(bContext *C,
                                         StructRNA *rna_type,
                                         const bool is_liboverride_allowed)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", rna_type);
  Object *ob = (ptr.owner_id) ? (Object *)ptr.owner_id : context_active_object(C);
  bConstraint *con = static_cast<bConstraint *>(ptr.data);

  if (!ED_operator_object_active_editable_ex(C, ob)) {
    return false;
  }

  if (ptr.owner_id != nullptr && !ID_IS_EDITABLE(ptr.owner_id)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit library data");
    return false;
  }

  if (!is_liboverride_allowed && BKE_constraint_is_nonlocal_in_liboverride(ob, con)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit constraints coming from linked data in a library override");
    return false;
  }

  return true;
}

static bool edit_constraint_poll(bContext *C)
{
  return edit_constraint_poll_generic(C, &RNA_Constraint, false);
}

/* Used by operators performing actions allowed also on constraints from the overridden linked
 * object (not only from added 'local' ones). */
static bool edit_constraint_liboverride_allowed_poll(bContext *C)
{
  return edit_constraint_poll_generic(C, &RNA_Constraint, true);
}

static void edit_constraint_properties(wmOperatorType *ot)
{
  PropertyRNA *prop;
  prop = RNA_def_string(
      ot->srna, "constraint", nullptr, MAX_NAME, "Constraint", "Name of the constraint to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_enum(
      ot->srna, "owner", constraint_owner_items, 0, "Owner", "The owner of this constraint");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void edit_constraint_report_property(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "report", false, "Report", "Create a notification after the operation");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static bool edit_constraint_invoke_properties(bContext *C,
                                              wmOperator *op,
                                              const wmEvent *event,
                                              wmOperatorStatus *r_retval)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  Object *ob = (ptr.owner_id) ? (Object *)ptr.owner_id : context_active_object(C);
  bConstraint *con;
  ListBase *list;

  if (RNA_struct_property_is_set(op->ptr, "constraint") &&
      RNA_struct_property_is_set(op->ptr, "owner"))
  {
    return true;
  }

  if (ptr.data) {
    con = static_cast<bConstraint *>(ptr.data);
    RNA_string_set(op->ptr, "constraint", con->name);

    list = constraint_list_from_constraint(ob, con, nullptr);

    if (&ob->constraints == list) {
      RNA_enum_set(op->ptr, "owner", EDIT_CONSTRAINT_OWNER_OBJECT);
    }
    else {
      RNA_enum_set(op->ptr, "owner", EDIT_CONSTRAINT_OWNER_BONE);
    }

    return true;
  }

  /* Check the custom data of panels under the mouse for a modifier. */
  if (event != nullptr) {
    PointerRNA *panel_ptr = UI_region_panel_custom_data_under_cursor(C, event);

    if (!(panel_ptr == nullptr || RNA_pointer_is_null(panel_ptr))) {
      if (RNA_struct_is_a(panel_ptr->type, &RNA_Constraint)) {
        con = static_cast<bConstraint *>(panel_ptr->data);
        RNA_string_set(op->ptr, "constraint", con->name);
        list = constraint_list_from_constraint(ob, con, nullptr);
        RNA_enum_set(op->ptr,
                     "owner",
                     (&ob->constraints == list) ? EDIT_CONSTRAINT_OWNER_OBJECT :
                                                  EDIT_CONSTRAINT_OWNER_BONE);

        return true;
      }

      BLI_assert(r_retval != nullptr); /* We need the return value in this case. */
      if (r_retval != nullptr) {
        *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
      }
      return false;
    }
  }

  if (r_retval != nullptr) {
    *r_retval = OPERATOR_CANCELLED;
  }
  return false;
}

static bConstraint *edit_constraint_property_get(bContext *C, wmOperator *op, Object *ob, int type)
{
  char constraint_name[MAX_NAME];
  int owner = RNA_enum_get(op->ptr, "owner");
  bConstraint *con;
  ListBase *list = nullptr;

  RNA_string_get(op->ptr, "constraint", constraint_name);

  if (owner == EDIT_CONSTRAINT_OWNER_BONE) {
    list = pose_constraint_list(C);
    if (!list) {
      return nullptr;
    }
  }
  else {
    list = &ob->constraints;
  }

  con = BKE_constraints_find_name(list, constraint_name);
#if 0
  if (G.debug & G_DEBUG) {
    printf("constraint found = %p, %s\n", (void *)con, (con) ? con->name : "<Not found>");
  }
#endif

  if (con && (type != 0) && (con->type != type)) {
    con = nullptr;
  }

  return con;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Stretch-To/Limit-Distance Constraint (Reset Original Length Operator)
 *
 * For Stretch-To & Limit-Distance constraints.
 * \{ */

static wmOperatorStatus stretchto_reset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_STRETCHTO);
  bStretchToConstraint *data = (con) ? (bStretchToConstraint *)con->data : nullptr;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* just set original length to 0.0, which will cause a reset on next recalc */
  data->orglength = 0.0f;
  constraint_update(bmain, ob);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, nullptr);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus stretchto_reset_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return stretchto_reset_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_stretchto_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Original Length";
  ot->idname = "CONSTRAINT_OT_stretchto_reset";
  ot->description = "Reset original length of bone for Stretch To Constraint";

  /* callbacks */
  ot->invoke = stretchto_reset_invoke;
  ot->exec = stretchto_reset_exec;
  ot->poll = edit_constraint_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Distance Constraint (Reset Operator)
 *
 * For Limit-Distance constraint.
 * \{ */

static wmOperatorStatus limitdistance_reset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_DISTLIMIT);
  bDistLimitConstraint *data = (con) ? (bDistLimitConstraint *)con->data : nullptr;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* just set original length to 0.0, which will cause a reset on next recalc */
  data->dist = 0.0f;
  constraint_update(bmain, ob);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, nullptr);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus limitdistance_reset_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return limitdistance_reset_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_limitdistance_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Distance";
  ot->idname = "CONSTRAINT_OT_limitdistance_reset";
  ot->description = "Reset limiting distance for Limit Distance Constraint";

  /* callbacks */
  ot->invoke = limitdistance_reset_invoke;
  ot->exec = limitdistance_reset_exec;
  ot->poll = edit_constraint_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Child-Of Constraint (Set Inverse Operator)
 * \{ */

/* Force evaluation so that the 'set inverse' flag is handled.
 * No-op when the constraint is enabled, as in such cases the evaluation will happen anyway.
 */
static void force_evaluation_if_constraint_disabled(bContext *C, Object *ob, bConstraint *con)
{
  if ((con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) == 0) {
    return;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = DEG_get_evaluated_scene(depsgraph);

  short flag_backup = con->flag;
  con->flag &= ~(CONSTRAINT_DISABLE | CONSTRAINT_OFF);
  BKE_object_eval_constraints(depsgraph, scene, ob);
  con->flag = flag_backup;
}

/* ChildOf Constraint - set inverse callback */
static wmOperatorStatus childof_set_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_CHILDOF);
  bChildOfConstraint *data = (con) ? (bChildOfConstraint *)con->data : nullptr;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == nullptr) {
    printf("DEBUG: Child-Of Set Inverse - object = '%s'\n", (ob) ? ob->id.name + 2 : "<None>");
    BKE_report(op->reports, RPT_ERROR, "Could not find constraint data for Child-Of Set Inverse");
    return OPERATOR_CANCELLED;
  }

  /* Set a flag to request recalculation on next update. */
  data->flag |= CHILDOF_SET_INVERSE;

  /* Force constraint to run, it will perform the recalculation. */
  force_evaluation_if_constraint_disabled(C, ob, con);

  constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus childof_set_inverse_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return childof_set_inverse_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_childof_set_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Inverse";
  ot->idname = "CONSTRAINT_OT_childof_set_inverse";
  ot->description = "Set inverse correction for Child Of constraint";

  /* callbacks */
  ot->invoke = childof_set_inverse_invoke;
  ot->exec = childof_set_inverse_exec;
  ot->poll = edit_constraint_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/* ChildOf Constraint - clear inverse callback */
static wmOperatorStatus childof_clear_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_CHILDOF);
  bChildOfConstraint *data = (con) ? (bChildOfConstraint *)con->data : nullptr;

  if (data == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Child Of constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* simply clear the matrix */
  unit_m4(data->invmat);

  constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus childof_clear_inverse_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return childof_clear_inverse_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

static bool childof_clear_inverse_poll(bContext *C)
{
  if (!edit_constraint_liboverride_allowed_poll(C)) {
    return false;
  }

  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  bConstraint *con = static_cast<bConstraint *>(ptr.data);

  /* Allow workflows with unset context's constraint.
   * The constraint can also be provided as an operator's property. */
  if (con == nullptr) {
    return true;
  }

  if (con->type != CONSTRAINT_TYPE_CHILDOF) {
    return false;
  }

  bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);

  if (is_identity_m4(data->invmat)) {
    CTX_wm_operator_poll_msg_set(C, "No inverse correction is set, so there is nothing to clear");
    return false;
  }
  return true;
}

void CONSTRAINT_OT_childof_clear_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Inverse";
  ot->idname = "CONSTRAINT_OT_childof_clear_inverse";
  ot->description = "Clear inverse correction for Child Of constraint";

  /* callbacks */
  ot->invoke = childof_clear_inverse_invoke;
  ot->exec = childof_clear_inverse_exec;
  ot->poll = childof_clear_inverse_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Follow Path Constraint (Auto Animate Path Operator)
 * \{ */

static wmOperatorStatus followpath_path_animate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_FOLLOWPATH);
  bFollowPathConstraint *data = (con) ? (bFollowPathConstraint *)con->data : nullptr;

  bAction *act = nullptr;
  FCurve *fcu = nullptr;
  int sfra = RNA_int_get(op->ptr, "frame_start");
  int len = RNA_int_get(op->ptr, "length");
  float standardRange = 1.0;

  /* nearly impossible sanity check */
  if (data == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Follow Path constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* add F-Curve as appropriate */
  if (data->tar) {
    Curve *cu = (Curve *)data->tar->data;

    if (ELEM(nullptr, cu->adt, cu->adt->action) ||
        (animrig::fcurve_find_in_assigned_slot(*cu->adt, {"eval_time", 0}) == nullptr))
    {
      /* create F-Curve for path animation */
      act = animrig::id_action_ensure(bmain, &cu->id);
      PointerRNA id_ptr = RNA_id_pointer_create(&cu->id);
      fcu = animrig::action_fcurve_ensure_ex(bmain, act, nullptr, &id_ptr, {"eval_time", 0});

      /* standard vertical range - 1:1 = 100 frames */
      standardRange = 100.0f;
    }
    else {
      /* path anim exists already - abort for now as this may well be what was intended */
      BKE_report(op->reports, RPT_WARNING, "Path is already animated");
      return OPERATOR_CANCELLED;
    }
  }
  else {
    /* animate constraint's "fixed offset" */
    PropertyRNA *prop;

    /* get RNA pointer to constraint's "offset_factor" property - to build RNA path */
    PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_FollowPathConstraint, con);
    prop = RNA_struct_find_property(&ptr, "offset_factor");

    const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop);
    BLI_assert(path.has_value());

    /* create F-Curve for constraint */
    act = animrig::id_action_ensure(bmain, &ob->id);
    PointerRNA id_ptr = RNA_id_pointer_create(&ob->id);
    fcu = animrig::action_fcurve_ensure_ex(bmain, act, nullptr, &id_ptr, {path->c_str(), 0});

    /* standard vertical range - 0.0 to 1.0 */
    standardRange = 1.0f;

    /* enable "Use Fixed Position" so that animating this has effect */
    data->followflag |= FOLLOWPATH_STATIC;
  }

  /* setup dummy 'generator' modifier here to get 1-1 correspondence still working
   * and define basic slope of this curve based on the properties
   */
  if (!fcu->bezt && !fcu->fpt && !fcu->modifiers.first) {
    FModifier *fcm = add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR, fcu);
    FMod_Generator *gen = static_cast<FMod_Generator *>(fcm->data);

    /* Assume that we have the following equation:
     *     y = Ax + B
     *         1    0       <-- coefficients array indices
     */
    float A = standardRange / float(len);
    float B = float(-sfra) * A;

    gen->coefficients[1] = A;
    gen->coefficients[0] = B;
  }

  /* updates... */
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus followpath_path_animate_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent * /*event*/)
{
  /* hook up invoke properties for figuring out which constraint we're dealing with */
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return followpath_path_animate_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_followpath_path_animate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Auto Animate Path";
  ot->idname = "CONSTRAINT_OT_followpath_path_animate";
  ot->description =
      "Add default animation for path used by constraint if it isn't animated already";

  /* callbacks */
  ot->invoke = followpath_path_animate_invoke;
  ot->exec = followpath_path_animate_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  edit_constraint_properties(ot);
  RNA_def_int(ot->srna,
              "frame_start",
              1,
              MINAFRAME,
              MAXFRAME,
              "Start Frame",
              "First frame of path animation",
              MINAFRAME,
              MAXFRAME);
  RNA_def_int(ot->srna,
              "length",
              100,
              0,
              MAXFRAME,
              "Length",
              "Number of frames that path animation should take",
              0,
              MAXFRAME);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Object Solver Constraint (Set Inverse Operator)
 * \{ */

static wmOperatorStatus objectsolver_set_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_OBJECTSOLVER);
  bObjectSolverConstraint *data = (con) ? (bObjectSolverConstraint *)con->data : nullptr;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == nullptr) {
    printf("DEBUG: ObjectSolver Set Inverse - object = '%s'\n", (ob) ? ob->id.name + 2 : "<None>");
    BKE_report(
        op->reports, RPT_ERROR, "Could not find constraint data for ObjectSolver Set Inverse");
    return OPERATOR_CANCELLED;
  }

  /* Set a flag to request recalculation on next update. */
  data->flag |= OBJECTSOLVER_SET_INVERSE;

  /* Force constraint to run, it will perform the recalculation. */
  force_evaluation_if_constraint_disabled(C, ob, con);

  constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus objectsolver_set_inverse_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return objectsolver_set_inverse_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_objectsolver_set_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Inverse";
  ot->idname = "CONSTRAINT_OT_objectsolver_set_inverse";
  ot->description = "Set inverse correction for Object Solver constraint";

  /* callbacks */
  ot->invoke = objectsolver_set_inverse_invoke;
  ot->exec = objectsolver_set_inverse_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Object Solver Constraint (Clear Inverse Operator)
 * \{ */

static wmOperatorStatus objectsolver_clear_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, CONSTRAINT_TYPE_OBJECTSOLVER);
  bObjectSolverConstraint *data = (con) ? (bObjectSolverConstraint *)con->data : nullptr;

  if (data == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Child Of constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* simply clear the matrix */
  unit_m4(data->invmat);

  constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus objectsolver_clear_inverse_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  if (edit_constraint_invoke_properties(C, op, nullptr, nullptr)) {
    return objectsolver_clear_inverse_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

static bool objectsolver_clear_inverse_poll(bContext *C)
{
  if (!edit_constraint_poll(C)) {
    return false;
  }

  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  bConstraint *con = static_cast<bConstraint *>(ptr.data);
  if (con == nullptr) {
    return true;
  }

  bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;

  if (is_identity_m4(data->invmat)) {
    CTX_wm_operator_poll_msg_set(C, "No inverse correction is set, so there is nothing to clear");
    return false;
  }
  return true;
}

void CONSTRAINT_OT_objectsolver_clear_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Inverse";
  ot->idname = "CONSTRAINT_OT_objectsolver_clear_inverse";
  ot->description = "Clear inverse correction for Object Solver constraint";

  /* callbacks */
  ot->invoke = objectsolver_clear_inverse_invoke;
  ot->exec = objectsolver_clear_inverse_exec;
  ot->poll = objectsolver_clear_inverse_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Constraint Management Utilities
 * \{ */

void constraint_active_set(Object *ob, bConstraint *con)
{
  ListBase *lb = constraint_list_from_constraint(ob, con, nullptr);

  /* lets be nice and escape if its active already */
  /* NOTE: this assumes that the stack doesn't have other active ones set... */
  if ((lb && con) && (con->flag & CONSTRAINT_ACTIVE)) {
    return;
  }

  BKE_constraints_active_set(lb, con);
}

void constraint_update(Main *bmain, Object *ob)
{
  if (ob->pose) {
    BKE_pose_update_constraint_flags(ob->pose);
  }

  object_test_constraints(bmain, ob);

  if (ob->type == OB_ARMATURE) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
  }
  else {
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
}

static void object_pose_tag_update(Main *bmain, Object *ob)
{
  BKE_pose_tag_recalc(bmain, ob->pose); /* Checks & sort pose channels. */
}

void constraint_dependency_update(Main *bmain, Object *ob)
{
  constraint_update(bmain, ob);

  if (ob->pose) {
    object_pose_tag_update(bmain, ob);
  }
  DEG_relations_tag_update(bmain);
}

void constraint_tag_update(Main *bmain, Object *ob, bConstraint *con)
{
  if (ob->pose) {
    BKE_pose_tag_update_constraint_flags(ob->pose);
  }

  if (con) {
    object_test_constraint(bmain, ob, con);
  }

  if (ob->type == OB_ARMATURE) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
  }
  else {
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }

  /* Do Copy-on-Write tag here too, otherwise constraint
   * influence/mute buttons in UI have no effect
   */
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
}

void constraint_dependency_tag_update(Main *bmain, Object *ob, bConstraint *con)
{
  constraint_tag_update(bmain, ob, con);

  if (ob->pose) {
    object_pose_tag_update(bmain, ob);
  }
  DEG_relations_tag_update(bmain);
}

bool constraint_move_to_index(Object *ob, bConstraint *con, const int index)
{
  BLI_assert(con != nullptr);
  BLI_assert(index >= 0);

  ListBase *conlist = constraint_list_from_constraint(ob, con, nullptr);
  int current_index = BLI_findindex(conlist, con);
  BLI_assert(current_index >= 0);

  BLI_listbase_link_move(conlist, con, index - current_index);

  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT, ob);

  return true;
}

void constraint_link(Main *bmain, Object *ob_dst, ListBase *dst, ListBase *src)
{
  BKE_constraints_free(dst);
  BKE_constraints_copy(dst, src, true);
  LISTBASE_FOREACH (bConstraint *, con, dst) {
    constraint_dependency_tag_update(bmain, ob_dst, con);
  }
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, nullptr);
}

void constraint_copy_for_object(Main *bmain, Object *ob_dst, bConstraint *con)
{
  bConstraint *copy_con = BKE_constraint_copy_for_object(ob_dst, con);
  copy_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

  constraint_dependency_tag_update(bmain, ob_dst, con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, ob_dst);
}

void constraint_copy_for_pose(Main *bmain, Object *ob_dst, bPoseChannel *pchan, bConstraint *con)
{
  bConstraint *copy_con = BKE_constraint_copy_for_pose(ob_dst, pchan, con);
  copy_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

  constraint_dependency_tag_update(bmain, ob_dst, con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, ob_dst);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Delete Constraint Operator
 * \{ */

static wmOperatorStatus constraint_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  if (con == nullptr) {
    return OPERATOR_CANCELLED;
  }

  ListBase *lb = constraint_list_from_constraint(ob, con, nullptr);

  /* Store name temporarily for report. */
  char name[MAX_NAME];
  STRNCPY_UTF8(name, con->name);

  /* free the constraint */
  if (BKE_constraint_remove_ex(lb, ob, con)) {
    /* Needed to set the flags on pose-bones correctly. */
    constraint_update(bmain, ob);

    /* relations */
    DEG_relations_tag_update(bmain);

    /* notifiers */
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);

    if (RNA_boolean_get(op->ptr, "report")) {
      BKE_reportf(op->reports, RPT_INFO, "Removed constraint: %s", name);
    }

    return OPERATOR_FINISHED;
  }
  /* couldn't remove due to some invalid data */
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus constraint_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (!edit_constraint_invoke_properties(C, op, event, &retval)) {
    return OPERATOR_CANCELLED;
  }
  return constraint_delete_exec(C, op);
}

void CONSTRAINT_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Constraint";
  ot->idname = "CONSTRAINT_OT_delete";
  ot->description = "Remove constraint from constraint stack";

  /* callbacks */
  ot->invoke = constraint_delete_invoke;
  ot->exec = constraint_delete_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
  edit_constraint_report_property(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Apply Constraint Operator
 * \{ */

static wmOperatorStatus constraint_apply_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  if (con == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bPoseChannel *pchan;
  ListBase *constraints = constraint_list_from_constraint(ob, con, &pchan);

  /* Store name temporarily for report. */
  char name[MAX_NAME];
  STRNCPY_UTF8(name, con->name);
  const bool is_first_constraint = con != constraints->first;

  /* Copy the constraint. */
  bool success;
  if (pchan) {
    success = BKE_constraint_apply_and_remove_for_pose(
        depsgraph, scene, constraints, ob, con, pchan);
  }
  else {
    success = BKE_constraint_apply_and_remove_for_object(depsgraph, scene, constraints, ob, con);
  }

  if (!success) {
    /* Couldn't remove due to some invalid data. */
    return OPERATOR_CANCELLED;
  }

  /* Update for any children that may get moved. */
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  /* Needed to set the flags on pose-bones correctly. */
  constraint_update(bmain, ob);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
  if (pchan) {
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }
  else {
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
  }

  if (RNA_boolean_get(op->ptr, "report")) {
    if (is_first_constraint) {
      BKE_report(op->reports,
                 RPT_INFO,
                 "Applied constraint was not first, result may not be as expected");
    }
    else {
      /* Only add this report if the operator didn't cause another one. The purpose here is
       * to alert that something happened, and the previous report will do that anyway. */
      BKE_reportf(op->reports, RPT_INFO, "Applied constraint: %s", name);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus constraint_apply_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (!edit_constraint_invoke_properties(C, op, event, &retval)) {
    return OPERATOR_CANCELLED;
  }
  return constraint_apply_exec(C, op);
}

void CONSTRAINT_OT_apply(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Constraint";
  ot->idname = "CONSTRAINT_OT_apply";
  ot->description = "Apply constraint and remove from the stack";

  /* callbacks */
  ot->invoke = constraint_apply_invoke;
  ot->exec = constraint_apply_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
  edit_constraint_report_property(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Constraint Operator
 * \{ */

static wmOperatorStatus constraint_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  if (con == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bPoseChannel *pchan;
  ListBase *constraints = constraint_list_from_constraint(ob, con, &pchan);

  /* Store name temporarily for report. */
  char name[MAX_NAME];
  STRNCPY_UTF8(name, con->name);

  /* Copy the constraint. */
  bConstraint *copy_con;
  if (pchan) {
    copy_con = BKE_constraint_copy_for_pose(ob, pchan, con);
  }
  else {
    copy_con = BKE_constraint_copy_for_object(ob, con);
  }

  if (!copy_con) {
    /* Couldn't remove due to some invalid data. */
    return OPERATOR_CANCELLED;
  }
  copy_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

  /* Move constraint to correct position. */
  const int new_index = BLI_findindex(constraints, con) + 1;
  const int current_index = BLI_findindex(constraints, copy_con);
  BLI_assert(new_index >= 0);
  BLI_assert(current_index >= 0);
  BLI_listbase_link_move(constraints, copy_con, new_index - current_index);

  /* Needed to set the flags on pose-bones correctly. */
  constraint_update(bmain, ob);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_ADDED, ob);

  if (RNA_boolean_get(op->ptr, "report")) {
    BKE_reportf(op->reports, RPT_INFO, "Copied constraint: %s", name);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus constraint_copy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (!edit_constraint_invoke_properties(C, op, event, &retval)) {
    return OPERATOR_CANCELLED;
  }
  return constraint_copy_exec(C, op);
}

void CONSTRAINT_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Constraint";
  ot->idname = "CONSTRAINT_OT_copy";
  ot->description = "Duplicate constraint at the same position in the stack";

  /* callbacks */
  ot->invoke = constraint_copy_invoke;
  ot->exec = constraint_copy_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
  edit_constraint_report_property(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Constraint To Selected Operator
 * \{ */

static wmOperatorStatus constraint_copy_to_selected_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *obact = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, obact, 0);

  if (con == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bPoseChannel *pchan;
  constraint_list_from_constraint(obact, con, &pchan);

  if (pchan) {
    /* Don't do anything if bone doesn't exist or doesn't have any constraints. */
    if (pchan->constraints.first == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "No constraints for copying");
      return OPERATOR_CANCELLED;
    }

    Object *prev_ob = nullptr;

    /* Copy all constraints from active pose-bone to all selected pose-bones. */
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, chan, selected_pose_bones, Object *, ob) {
      /* If we're not handling the object we're copying from, copy all constraints over. */
      if (pchan == chan) {
        continue;
      }

      bConstraint *copy_con = BKE_constraint_copy_for_pose(ob, chan, con);
      copy_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

      /* Update flags (need to add here, not just copy). */
      chan->constflag |= pchan->constflag;

      if (prev_ob == ob) {
        continue;
      }

      BKE_pose_tag_recalc(bmain, ob->pose);
      DEG_id_tag_update((ID *)ob, ID_RECALC_GEOMETRY);
      prev_ob = ob;
    }
    CTX_DATA_END;
  }
  else {
    /* Copy all constraints from active object to all selected objects. */
    CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
      /* If we're not handling the object we're copying from, copy all constraints over. */
      if (obact == ob) {
        continue;
      }

      bConstraint *copy_con = BKE_constraint_copy_for_object(ob, con);
      copy_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
    }
    CTX_DATA_END;
  }

  /* Force depsgraph to get recalculated since new relationships added. */
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus constraint_copy_to_selected_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  wmOperatorStatus retval;
  if (!edit_constraint_invoke_properties(C, op, event, &retval)) {
    return retval;
  }
  return constraint_copy_to_selected_exec(C, op);
}

static bool constraint_copy_to_selected_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  Object *obact = (ptr.owner_id) ? (Object *)ptr.owner_id : context_active_object(C);
  bConstraint *con = static_cast<bConstraint *>(ptr.data);
  bPoseChannel *pchan;
  constraint_list_from_constraint(obact, con, &pchan);

  if (pchan) {
    bool found = false;
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, chan, selected_pose_bones, Object *, ob) {
      UNUSED_VARS(ob);
      if (pchan != chan) {
        /** NOTE: Can not return here, because CTX_DATA_BEGIN_WITH_ID allocated
         * a list that needs to be freed by CTX_DATA_END. */
        found = true;
        break;
      }
    }
    CTX_DATA_END;
    if (found) {
      return true;
    }

    CTX_wm_operator_poll_msg_set(C, "No other bones are selected");
    return false;
  }

  if (!obact) {
    CTX_wm_operator_poll_msg_set(C, "No selected object to copy from");
    return false;
  }

  bool found = false;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob != obact) {
      /** NOTE: Can not return here, because CTX_DATA_BEGIN allocated
       * a list that needs to be freed by CTX_DATA_END. */
      found = true;
      break;
    }
  }
  CTX_DATA_END;
  if (found) {
    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "No other objects are selected");
  return false;
}

void CONSTRAINT_OT_copy_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Constraint To Selected";
  ot->idname = "CONSTRAINT_OT_copy_to_selected";
  ot->description = "Copy constraint to other selected objects/bones";

  /* API callbacks. */
  ot->exec = constraint_copy_to_selected_exec;
  ot->invoke = constraint_copy_to_selected_invoke;
  ot->poll = constraint_copy_to_selected_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Down Constraint Operator
 * \{ */

static wmOperatorStatus constraint_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  if (con && con->next) {
    ListBase *conlist = constraint_list_from_constraint(ob, con, nullptr);
    bConstraint *nextCon = con->next;

    /* insert the nominated constraint after the one that used to be after it */
    BLI_remlink(conlist, con);
    BLI_insertlinkafter(conlist, nextCon, con);

    constraint_update(CTX_data_main(C), ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus constraint_move_down_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_constraint_invoke_properties(C, op, event, &retval)) {
    return constraint_move_down_exec(C, op);
  }
  return retval;
}

void CONSTRAINT_OT_move_down(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Constraint Down";
  ot->idname = "CONSTRAINT_OT_move_down";
  ot->description = "Move constraint down in constraint stack";

  /* callbacks */
  ot->invoke = constraint_move_down_invoke;
  ot->exec = constraint_move_down_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Up Constraint Operator
 * \{ */

static wmOperatorStatus constraint_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  if (con && con->prev) {
    ListBase *conlist = constraint_list_from_constraint(ob, con, nullptr);
    bConstraint *prevCon = con->prev;

    /* insert the nominated constraint before the one that used to be before it */
    BLI_remlink(conlist, con);
    BLI_insertlinkbefore(conlist, prevCon, con);

    constraint_update(CTX_data_main(C), ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus constraint_move_up_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_constraint_invoke_properties(C, op, event, &retval)) {
    return constraint_move_up_exec(C, op);
  }
  return retval;
}

void CONSTRAINT_OT_move_up(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Constraint Up";
  ot->idname = "CONSTRAINT_OT_move_up";
  ot->description = "Move constraint up in constraint stack";

  /* callbacks */
  ot->exec = constraint_move_up_exec;
  ot->invoke = constraint_move_up_invoke;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Constraint To Index Operator
 * \{ */

static wmOperatorStatus constraint_move_to_index_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  bConstraint *con = edit_constraint_property_get(C, op, ob, 0);

  int new_index = RNA_int_get(op->ptr, "index");
  new_index = std::max(new_index, 0);

  if (con) {
    constraint_move_to_index(ob, con, new_index);

    constraint_update(CTX_data_main(C), ob);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus constraint_move_to_index_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_constraint_invoke_properties(C, op, event, &retval)) {
    return constraint_move_to_index_exec(C, op);
  }
  return retval;
}

void CONSTRAINT_OT_move_to_index(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Constraint to Index";
  ot->idname = "CONSTRAINT_OT_move_to_index";
  ot->description =
      "Change the constraint's position in the list so it evaluates after the set number of "
      "others";

  /* callbacks */
  ot->exec = constraint_move_to_index_exec;
  ot->invoke = constraint_move_to_index_invoke;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  edit_constraint_properties(ot);
  RNA_def_int(ot->srna,
              "index",
              0,
              0,
              INT_MAX,
              "Index",
              "The index to move the constraint to",
              0,
              INT_MAX);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Pose Constraints Operator
 * \{ */

static wmOperatorStatus pose_constraints_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *prev_ob = nullptr;

  /* free constraints for all selected bones */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    BKE_constraints_free(&pchan->constraints);
    pchan->constflag = 0;

    if (prev_ob != ob) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
      prev_ob = ob;
    }
  }
  CTX_DATA_END;

  /* force depsgraph to get recalculated since relationships removed */
  DEG_relations_tag_update(bmain);

  /* NOTE: calling BIK_clear_data() isn't needed here. */

  return OPERATOR_FINISHED;
}

void POSE_OT_constraints_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Constraints";
  ot->idname = "POSE_OT_constraints_clear";
  ot->description = "Clear all constraints from the selected bones";

  /* callbacks */
  ot->exec = pose_constraints_clear_exec;
  /* XXX: do we want to ensure there are selected bones too? */
  ot->poll = ED_operator_object_active_local_editable_posemode_exclusive;
}

static wmOperatorStatus object_constraints_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);

  /* do freeing */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    BKE_constraints_free(&ob->constraints);
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
  CTX_DATA_END;

  /* force depsgraph to get recalculated since relationships removed */
  DEG_relations_tag_update(bmain);

  /* do updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Object Constraints Operator
 * \{ */

void OBJECT_OT_constraints_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Object Constraints";
  ot->idname = "OBJECT_OT_constraints_clear";
  ot->description = "Clear all constraints from the selected objects";

  /* callbacks */
  ot->exec = object_constraints_clear_exec;
  ot->poll = ED_operator_object_active_local_editable;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Pose Constraints Operator
 * \{ */

static wmOperatorStatus pose_constraint_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bPoseChannel *pchan = CTX_data_active_pose_bone(C);

  /* don't do anything if bone doesn't exist or doesn't have any constraints */
  if (ELEM(nullptr, pchan, pchan->constraints.first)) {
    BKE_report(op->reports, RPT_ERROR, "No active bone with constraints for copying");
    return OPERATOR_CANCELLED;
  }

  Object *prev_ob = nullptr;

  /* Copy all constraints from active pose-bone to all selected pose-bones. */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, chan, selected_pose_bones, Object *, ob) {
    /* if we're not handling the object we're copying from, copy all constraints over */
    if (pchan != chan) {
      BKE_constraints_copy(&chan->constraints, &pchan->constraints, true);
      /* update flags (need to add here, not just copy) */
      chan->constflag |= pchan->constflag;

      if (prev_ob != ob) {
        BKE_pose_tag_recalc(bmain, ob->pose);
        DEG_id_tag_update((ID *)ob, ID_RECALC_GEOMETRY);
        prev_ob = ob;
      }
    }
  }
  CTX_DATA_END;

  /* force depsgraph to get recalculated since new relationships added */
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, nullptr);

  return OPERATOR_FINISHED;
}

void POSE_OT_constraints_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Constraints to Selected Bones";
  ot->idname = "POSE_OT_constraints_copy";
  ot->description = "Copy constraints to other selected bones";

  /* API callbacks. */
  ot->exec = pose_constraint_copy_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Object Constraints Operator
 * \{ */

static wmOperatorStatus object_constraint_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *obact = context_active_object(C);

  /* copy all constraints from active object to all selected objects */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    /* if we're not handling the object we're copying from, copy all constraints over */
    if (obact != ob) {
      BKE_constraints_copy(&ob->constraints, &obact->constraints, true);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
    }
  }
  CTX_DATA_END;

  /* force depsgraph to get recalculated since new relationships added */
  DEG_relations_tag_update(bmain);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_constraints_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Constraints to Selected Objects";
  ot->idname = "OBJECT_OT_constraints_copy";
  ot->description = "Copy constraints to other selected objects";

  /* API callbacks. */
  ot->exec = object_constraint_copy_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Add Constraints Operator
 * \{ */

/* get the Object and/or PoseChannel to use as target */
static bool get_new_constraint_target(
    bContext *C, int con_type, Object **tar_ob, bPoseChannel **tar_pchan, bool add)
{
  Object *obact = context_active_object(C);
  bPoseChannel *pchanact = BKE_pose_channel_active_if_bonecoll_visible(obact);
  bool only_curve = false, only_mesh = false, only_ob = false;
  bool found = false;

  /* clear tar_ob and tar_pchan fields before use
   * - assume for now that both always exist...
   */
  *tar_ob = nullptr;
  *tar_pchan = nullptr;

  /* check if constraint type doesn't requires a target
   * - if so, no need to get any targets
   */
  switch (con_type) {
    /* no-target constraints --------------------------- */
    /* null constraint - shouldn't even be added! */
    case CONSTRAINT_TYPE_NULL:
    /* limit constraints - no targets needed */
    case CONSTRAINT_TYPE_LOCLIMIT:
    case CONSTRAINT_TYPE_ROTLIMIT:
    case CONSTRAINT_TYPE_SIZELIMIT:
    case CONSTRAINT_TYPE_SAMEVOL:
      return false;

    /* restricted target-type constraints -------------- */
    /* NOTE: for these, we cannot try to add a target object if no valid ones are found,
     * since that doesn't work */
    /* curve-based constraints - set the only_curve and only_ob flags */
    case CONSTRAINT_TYPE_CLAMPTO:
    case CONSTRAINT_TYPE_FOLLOWPATH:
    case CONSTRAINT_TYPE_SPLINEIK:
      only_curve = true;
      only_ob = true;
      add = false;
      break;

    /* mesh only? */
    case CONSTRAINT_TYPE_SHRINKWRAP:
      only_mesh = true;
      only_ob = true;
      add = false;
      break;

    /* Armature only. */
    case CONSTRAINT_TYPE_ARMATURE:
      add = false;
      break;
  }

  /* if the active Object is Armature, and we can search for bones, do so... */
  if ((obact->type == OB_ARMATURE) && (only_ob == false)) {
    /* search in list of selected Pose-Channels for target */
    CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones_from_active_object) {
      /* just use the first one that we encounter, as long as it is not the active one */
      if (pchan != pchanact) {
        *tar_ob = obact;
        *tar_pchan = pchan;
        found = true;

        break;
      }
    }
    CTX_DATA_END;
  }

  /* if not yet found, try selected Objects... */
  if (found == false) {
    /* search in selected objects context */
    CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
      /* just use the first object we encounter (that isn't the active object)
       * and which fulfills the criteria for the object-target that we've got
       */
      if (ob != obact) {
        /* for armatures in pose mode, look inside the armature for the active bone
         * so that we set up cross-armature constraints with less effort
         */
        if ((ob->type == OB_ARMATURE) && (ob->mode & OB_MODE_POSE) && (!only_curve && !only_mesh))
        {

          /* Only use the object & bone if the bone is visible & selected
           * since we may have multiple objects in pose mode at once. */
          bPoseChannel *pchan = BKE_pose_channel_active_or_first_selected(ob);
          if (pchan != nullptr) {
            *tar_pchan = pchan;
            *tar_ob = ob;
            found = true;
          }

          break;
        }
        if (((!only_curve) || (ob->type == OB_CURVES_LEGACY)) &&
            ((!only_mesh) || (ob->type == OB_MESH)))
        {
          /* set target */
          *tar_ob = ob;
          found = true;

          /* perform some special operations on the target */
          if (only_curve) {
            /* Curve-Path option must be enabled for follow-path constraints to be able to work
             */
            Curve *cu = (Curve *)ob->data;
            cu->flag |= CU_PATH;
          }

          break;
        }
      }
    }
    CTX_DATA_END;
  }

  /* if still not found, add a new empty to act as a target (if allowed) */
  if ((found == false) && (add)) {
    Main *bmain = CTX_data_main(C);
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = BKE_view_layer_active_base_get(view_layer);
    Object *obt;

    /* add new target object */
    obt = BKE_object_add(bmain, scene, view_layer, OB_EMPTY, nullptr);

    /* transform cent to global coords for loc */
    if (pchanact) {
      /* Since by default, IK targets the tip of the last bone,
       * use the tip of the active PoseChannel if adding a target for an IK Constraint. */
      if (con_type == CONSTRAINT_TYPE_KINEMATIC) {
        mul_v3_m4v3(obt->loc, obact->object_to_world().ptr(), pchanact->pose_tail);
      }
      else {
        mul_v3_m4v3(obt->loc, obact->object_to_world().ptr(), pchanact->pose_head);
      }
    }
    else {
      copy_v3_v3(obt->loc, obact->object_to_world().location());
    }

    /* restore, BKE_object_add sets active */
    view_layer->basact = base;
    base_select(base, BA_SELECT);

    /* make our new target the new object */
    *tar_ob = obt;
    found = true;
  }

  /* return whether there's any target */
  return found;
}

/* used by add constraint operators to add the constraint required */
static wmOperatorStatus constraint_add_exec(
    bContext *C, wmOperator *op, Object *ob, ListBase *list, int type, const bool setTarget)
{
  Main *bmain = CTX_data_main(C);
  bPoseChannel *pchan;
  bConstraint *con;

  if (list == &ob->constraints) {
    pchan = nullptr;
  }
  else {
    pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);

    /* ensure not to confuse object/pose adding */
    if (pchan == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "No active pose bone to add a constraint to");
      return OPERATOR_CANCELLED;
    }
  }
  /* check if constraint to be added is valid for the given constraints stack */
  if (type == CONSTRAINT_TYPE_NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Create a new constraint of the type required,
   * and add it to the active/given constraints list. */
  if (pchan) {
    con = BKE_constraint_add_for_pose(ob, pchan, nullptr, type);
  }
  else {
    con = BKE_constraint_add_for_object(ob, nullptr, type);
  }

  /* get the first selected object/bone, and make that the target
   * - apart from the buttons-window add buttons, we shouldn't add in this way
   */
  if (setTarget) {
    Object *tar_ob = nullptr;
    bPoseChannel *tar_pchan = nullptr;

    /* get the target objects, adding them as need be */
    if (get_new_constraint_target(C, type, &tar_ob, &tar_pchan, true)) {

      /* Armature constraints don't have a target by default, add one. */
      if (type == CONSTRAINT_TYPE_ARMATURE) {
        bArmatureConstraint *acon = static_cast<bArmatureConstraint *>(con->data);
        bConstraintTarget *ct = MEM_callocN<bConstraintTarget>("Constraint Target");

        ct->weight = 1.0f;
        BLI_addtail(&acon->targets, ct);

        constraint_dependency_tag_update(bmain, ob, con);
      }

      /* Method of setting target depends on the type of target we've got - by default,
       * just set the first target (distinction here is only for multiple-targeted constraints).
       */
      if (tar_pchan) {
        set_constraint_nth_target(con, tar_ob, tar_pchan->name, 0);
      }
      else {
        set_constraint_nth_target(con, tar_ob, "", 0);
      }
    }
  }

  /* make sure all settings are valid - similar to above checks, but sometimes can be wrong */
  object_test_constraints(bmain, ob);

  if (pchan) {
    BKE_pose_update_constraint_flags(ob->pose);
  }

  /* force depsgraph to get recalculated since new relationships added */
  DEG_relations_tag_update(bmain);

  if ((ob->type == OB_ARMATURE) && (pchan)) {
    BKE_pose_tag_recalc(bmain, ob->pose); /* sort pose channels */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
  }
  else {
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_ADDED, ob);

  return OPERATOR_FINISHED;
}

/* ------------------ */

/* dummy operator callback */
static wmOperatorStatus object_constraint_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  int type = RNA_enum_get(op->ptr, "type");
  short with_targets = 0;

  if (!ob) {
    BKE_report(op->reports, RPT_ERROR, "No active object to add constraint to");
    return OPERATOR_CANCELLED;
  }

  /* hack: set constraint targets from selected objects in context is allowed when
   * operator name included 'with_targets', since the menu doesn't allow multiple properties
   */
  if (strstr(op->idname, "with_targets")) {
    with_targets = 1;
  }

  return constraint_add_exec(C, op, ob, &ob->constraints, type, with_targets);
}

/* dummy operator callback */
static wmOperatorStatus pose_constraint_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(context_active_object(C));
  int type = RNA_enum_get(op->ptr, "type");
  short with_targets = 0;

  if (!ob) {
    BKE_report(op->reports, RPT_ERROR, "No active object to add constraint to");
    return OPERATOR_CANCELLED;
  }

  /* hack: set constraint targets from selected objects in context is allowed when
   * operator name included 'with_targets', since the menu doesn't allow multiple properties
   */
  if (strstr(op->idname, "with_targets")) {
    with_targets = 1;
  }

  return constraint_add_exec(C, op, ob, constraint_active_list(ob), type, with_targets);
}

/* ------------------ */

/* Filters constraints that are only compatible with bones */
static const EnumPropertyItem *object_constraint_add_itemf(bContext * /*C*/,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{
  const EnumPropertyItem *item = rna_enum_constraint_type_items;
  EnumPropertyItem *object_constraint_items = nullptr;
  int totitem = 0;

  while (item->identifier) {
    if (!ELEM(item->value, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
      RNA_enum_item_add(&object_constraint_items, &totitem, item);
    }
    item++;
  }

  RNA_enum_item_end(&object_constraint_items, &totitem);
  *r_free = true;

  return object_constraint_items;
}

void OBJECT_OT_constraint_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Constraint";
  ot->description = "Add a constraint to the active object";
  ot->idname = "OBJECT_OT_constraint_add";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_constraint_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "type", rna_enum_dummy_NULL_items, 0, "Type", "");
  RNA_def_enum_funcs(prop, object_constraint_add_itemf);
  ot->prop = prop;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Add Constraints With Targets Operator
 * \{ */

void OBJECT_OT_constraint_add_with_targets(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Constraint (with Targets)";
  ot->description =
      "Add a constraint to the active object, with target (where applicable) set to the "
      "selected objects/bones";
  ot->idname = "OBJECT_OT_constraint_add_with_targets";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_constraint_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "type", rna_enum_dummy_NULL_items, 0, "Type", "");
  RNA_def_enum_funcs(prop, object_constraint_add_itemf);
  ot->prop = prop;
}

void POSE_OT_constraint_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Constraint";
  ot->description = "Add a constraint to the active bone";
  ot->idname = "POSE_OT_constraint_add";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_constraint_add_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_constraint_type_items, 0, "Type", "");
}

void POSE_OT_constraint_add_with_targets(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Constraint (with Targets)";
  ot->description =
      "Add a constraint to the active bone, with target (where applicable) set to the selected "
      "Objects/Bones";
  ot->idname = "POSE_OT_constraint_add_with_targets";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_constraint_add_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_constraint_type_items, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Add IK to Bone Operator
 *
 * \note Only for pose-channels.
 * \{ */

/* TODO: should these be here, or back in `editors/armature/poseobject.c` again? */

/* present menu with options + validation for targets to use */
static wmOperatorStatus pose_ik_add_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bPoseChannel *pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);
  bConstraint *con = nullptr;

  uiPopupMenu *pup;
  uiLayout *layout;
  Object *tar_ob = nullptr;
  bPoseChannel *tar_pchan = nullptr;

  /* must have active bone */
  if (ELEM(nullptr, ob, pchan)) {
    BKE_report(op->reports, RPT_ERROR, "Must have an active bone to add IK constraint to");
    return OPERATOR_CANCELLED;
  }

  /* bone must not have any constraints already */
  for (con = static_cast<bConstraint *>(pchan->constraints.first); con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
      break;
    }
  }
  if (con) {
    BKE_report(op->reports, RPT_ERROR, "Bone already has an IK constraint");
    return OPERATOR_CANCELLED;
  }

  /* prepare popup menu to choose targeting options */
  pup = UI_popup_menu_begin(C, IFACE_("Add IK"), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  /* the type of targets we'll set determines the menu entries to show... */
  if (get_new_constraint_target(C, CONSTRAINT_TYPE_KINEMATIC, &tar_ob, &tar_pchan, false)) {
    /* bone target, or object target?
     * - the only thing that matters is that we want a target...
     */
    if (tar_pchan) {
      PointerRNA op_ptr = layout->op("POSE_OT_ik_add", IFACE_("Target Selected Bone"), ICON_NONE);
      RNA_boolean_set(&op_ptr, "with_targets", true);
    }
    else {
      PointerRNA op_ptr = layout->op(
          "POSE_OT_ik_add", IFACE_("Target Selected Object"), ICON_NONE);
      RNA_boolean_set(&op_ptr, "with_targets", true);
    }
  }
  else {
    /* we have a choice of adding to a new empty, or not setting any target (targetless IK) */
    PointerRNA op_ptr = layout->op("POSE_OT_ik_add", IFACE_("Target New Empty Object"), ICON_NONE);
    RNA_boolean_set(&op_ptr, "with_targets", true);
    op_ptr = layout->op("POSE_OT_ik_add", IFACE_("Without Target"), ICON_NONE);
    RNA_boolean_set(&op_ptr, "with_targets", false);
  }

  /* finish building the menu, and process it (should result in calling self again) */
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

/* call constraint_add_exec() to add the IK constraint */
static wmOperatorStatus pose_ik_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const bool with_targets = RNA_boolean_get(op->ptr, "with_targets");

  /* add the constraint - all necessary checks should have
   * been done by the invoke() callback already... */
  return constraint_add_exec(
      C, op, ob, constraint_active_list(ob), CONSTRAINT_TYPE_KINEMATIC, with_targets);
}

void POSE_OT_ik_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add IK to Bone";
  ot->description =
      "Add an IK Constraint to the active Bone. The target can be a selected bone or object";
  ot->idname = "POSE_OT_ik_add";

  /* API callbacks. */
  ot->invoke = pose_ik_add_invoke;
  ot->exec = pose_ik_add_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "with_targets",
                  true,
                  "With Targets",
                  "Assign IK Constraint with targets derived from the select bones/objects");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear IK from Bone Operator
 *
 * Remove IK constraints from selected bones.
 * \{ */

static wmOperatorStatus pose_ik_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Object *prev_ob = nullptr;

  /* only remove IK Constraints */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    bConstraint *con, *next;

    /* TODO: should we be checking if these constraints were local
     * before we try and remove them? */
    for (con = static_cast<bConstraint *>(pchan->constraints.first); con; con = next) {
      next = con->next;
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        BKE_constraint_remove_ex(&pchan->constraints, ob, con);
      }
    }
    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_NO_TARGET);

    if (prev_ob != ob) {
      prev_ob = ob;

      /* Refresh depsgraph. */
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
    }
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

void POSE_OT_ik_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove IK";
  ot->description = "Remove all IK Constraints from selected bones";
  ot->idname = "POSE_OT_ik_clear";

  /* API callbacks. */
  ot->exec = pose_ik_clear_exec;
  ot->poll = ED_operator_object_active_local_editable_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::object
