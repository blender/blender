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
 * \ingroup edobj
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_tracking.h"
#include "BIK_api.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_object.h"
#include "ED_keyframing.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "object_intern.h"

/* -------------- Get Active Constraint Data ---------------------- */

/* if object in posemode, active bone constraints, else object constraints */
ListBase *get_active_constraints(Object *ob)
{
  if (ob == NULL) {
    return NULL;
  }

  if (ob->mode & OB_MODE_POSE) {
    bPoseChannel *pchan;

    pchan = BKE_pose_channel_active(ob);
    if (pchan) {
      return &pchan->constraints;
    }
  }
  else {
    return &ob->constraints;
  }

  return NULL;
}

/* Find the list that a given constraint belongs to,
 * and/or also get the posechannel this is from (if applicable) */
ListBase *get_constraint_lb(Object *ob, bConstraint *con, bPoseChannel **r_pchan)
{
  if (r_pchan) {
    *r_pchan = NULL;
  }

  if (ELEM(NULL, ob, con)) {
    return NULL;
  }

  /* try object constraints first */
  if ((BLI_findindex(&ob->constraints, con) != -1)) {
    return &ob->constraints;
  }

  /* if armature, try pose bones too */
  if (ob->pose) {
    bPoseChannel *pchan;

    /* try each bone in order
     * NOTE: it's not possible to directly look up the active bone yet, so this will have to do
     */
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if ((BLI_findindex(&pchan->constraints, con) != -1)) {

        if (r_pchan) {
          *r_pchan = pchan;
        }

        return &pchan->constraints;
      }
    }
  }

  /* done */
  return NULL;
}

/* single constraint */
bConstraint *get_active_constraint(Object *ob)
{
  return BKE_constraints_active_get(get_active_constraints(ob));
}

/* -------------- Constraint Management (Add New, Remove, Rename) -------------------- */
#ifdef WITH_PYTHON
/* ------------- PyConstraints ------------------ */

/* this callback sets the text-file to be used for selected menu item */
static void validate_pyconstraint_cb(Main *bmain, void *arg1, void *arg2)
{
  bPythonConstraint *data = arg1;
  Text *text = NULL;
  int index = *((int *)arg2);
  int i;

  /* exception for no script */
  if (index) {
    /* innovative use of a for...loop to search */
    for (text = bmain->texts.first, i = 1; text && index != i; i++, text = text->id.next) {
      /* pass */
    }
  }
  data->text = text;
}

/* this returns a string for the list of usable pyconstraint script names */
static char *buildmenu_pyconstraints(Main *bmain, Text *con_text, int *pyconindex)
{
  DynStr *pupds = BLI_dynstr_new();
  Text *text;
  char *str;
  char buf[64];
  int i;

  /* add title first */
  sprintf(buf, "Scripts: %%t|[None]%%x0|");
  BLI_dynstr_append(pupds, buf);

  /* init active-index first */
  if (con_text == NULL) {
    *pyconindex = 0;
  }

  /* loop through markers, adding them */
  for (text = bmain->texts.first, i = 1; text; i++, text = text->id.next) {
    /* this is important to ensure that right script is shown as active */
    if (text == con_text) {
      *pyconindex = i;
    }

    /* only include valid pyconstraint scripts */
    if (BPY_is_pyconstraint(text)) {
      BLI_dynstr_append(pupds, text->id.name + 2);

      sprintf(buf, "%%x%d", i);
      BLI_dynstr_append(pupds, buf);

      if (text->id.next) {
        BLI_dynstr_append(pupds, "|");
      }
    }
  }

  /* convert to normal MEM_malloc'd string */
  str = BLI_dynstr_get_cstring(pupds);
  BLI_dynstr_free(pupds);

  return str;
}
#endif /* WITH_PYTHON */

#if 0  // UNUSED, until pyconstraints are added back.
/* this callback gets called when the 'refresh' button of a pyconstraint gets pressed */
static void update_pyconstraint_cb(void *arg1, void *arg2)
{
#  ifndef WITH_PYTHON
  (void)arg1; /* unused */
  (void)arg2; /* unused */
#  else
  Object *owner = (Object *)arg1;
  bConstraint *con = (bConstraint *)arg2;
  if (owner && con) {
    BPY_pyconstraint_update(owner, con);
  }
#  endif
}
#endif  // UNUSED

/* helper function for add_constriant - sets the last target for the active constraint */
static void set_constraint_nth_target(bConstraint *con,
                                      Object *target,
                                      const char subtarget[],
                                      int index)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
  ListBase targets = {NULL, NULL};
  bConstraintTarget *ct;
  int num_targets, i;

  if (cti && cti->get_constraint_targets) {
    cti->get_constraint_targets(con, &targets);
    num_targets = BLI_listbase_count(&targets);

    if (index < 0) {
      if (abs(index) < num_targets) {
        index = num_targets - abs(index);
      }
      else {
        index = num_targets - 1;
      }
    }
    else if (index >= num_targets) {
      index = num_targets - 1;
    }

    for (ct = targets.first, i = 0; ct; ct = ct->next, i++) {
      if (i == index) {
        ct->tar = target;
        BLI_strncpy(ct->subtarget, subtarget, sizeof(ct->subtarget));
        break;
      }
    }

    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, 0);
    }
  }
}

/* ------------- Constraint Sanity Testing ------------------- */

static void test_constraint(
    Main *bmain, Object *owner, bPoseChannel *pchan, bConstraint *con, int type)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
  ListBase targets = {NULL, NULL};
  bConstraintTarget *ct;
  bool check_targets = true;

  /* clear disabled-flag first */
  con->flag &= ~CONSTRAINT_DISABLE;

  if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
    bKinematicConstraint *data = con->data;

    /* bad: we need a separate set of checks here as poletarget is
     * optional... otherwise poletarget must exist too or else
     * the constraint is deemed invalid
     */
    /* default IK check ... */
    if (BKE_object_exists_check(bmain, data->tar) == 0) {
      data->tar = NULL;
      con->flag |= CONSTRAINT_DISABLE;
    }
    else if (data->tar == owner) {
      if (!BKE_armature_find_bone_name(BKE_armature_from_object(owner), data->subtarget)) {
        con->flag |= CONSTRAINT_DISABLE;
      }
    }

    if (data->poletar) {
      if (BKE_object_exists_check(bmain, data->poletar) == 0) {
        data->poletar = NULL;
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
    bPivotConstraint *data = con->data;

    /* target doesn't have to exist, but if it is non-null, it must exist! */
    if (data->tar && BKE_object_exists_check(bmain, data->tar) == 0) {
      data->tar = NULL;
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
    bActionConstraint *data = con->data;

    /* validate action */
    if (data->act == NULL) {
      /* must have action */
      con->flag |= CONSTRAINT_DISABLE;
    }
    else if (data->act->idroot != ID_OB) {
      /* only object-rooted actions can be used */
      data->act = NULL;
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
    bFollowPathConstraint *data = con->data;

    /* don't allow track/up axes to be the same */
    if (data->upflag == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->upflag + 3 == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_TRACKTO) {
    bTrackToConstraint *data = con->data;

    /* don't allow track/up axes to be the same */
    if (data->reserved2 == data->reserved1) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->reserved2 + 3 == data->reserved1) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_LOCKTRACK) {
    bLockTrackConstraint *data = con->data;

    if (data->lockflag == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
    if (data->lockflag + 3 == data->trackflag) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
    bSplineIKConstraint *data = con->data;

    /* if the number of points does not match the amount required by the chain length,
     * free the points array and request a rebind...
     */
    if ((data->points == NULL) || (data->numpoints != data->chainlen + 1)) {
      /* free the points array */
      if (data->points) {
        MEM_freeN(data->points);
        data->points = NULL;
      }

      /* clear the bound flag, forcing a rebind next time this is evaluated */
      data->flag &= ~CONSTRAINT_SPLINEIK_BOUND;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
    bFollowTrackConstraint *data = con->data;

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0) {
      if (data->clip != NULL && data->track[0]) {
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
          if (!BKE_tracking_track_get_named(tracking, tracking_object, data->track)) {
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
    bCameraSolverConstraint *data = con->data;

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0 && (data->clip == NULL)) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
    bObjectSolverConstraint *data = con->data;

    if ((data->flag & CAMERASOLVER_ACTIVECLIP) == 0 && (data->clip == NULL)) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }
  else if (con->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
    bTransformCacheConstraint *data = con->data;

    if ((data->cache_file == NULL) || (data->object_path[0] == '\0')) {
      con->flag |= CONSTRAINT_DISABLE;
    }
  }

  /* Check targets for constraints */
  if (check_targets && cti && cti->get_constraint_targets) {
    cti->get_constraint_targets(con, &targets);

    /* constraints with empty target list that actually require targets */
    if (!targets.first && ELEM(con->type, CONSTRAINT_TYPE_ARMATURE)) {
      con->flag |= CONSTRAINT_DISABLE;
    }

    /* disable and clear constraints targets that are incorrect */
    for (ct = targets.first; ct; ct = ct->next) {
      /* general validity checks (for those constraints that need this) */
      if (BKE_object_exists_check(bmain, ct->tar) == 0) {
        /* object doesn't exist, but constraint requires target */
        ct->tar = NULL;
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
          ct->tar = NULL;
          con->flag |= CONSTRAINT_DISABLE;
        }
      }

      /* target checks for specific constraints */
      if (ELEM(con->type,
               CONSTRAINT_TYPE_FOLLOWPATH,
               CONSTRAINT_TYPE_CLAMPTO,
               CONSTRAINT_TYPE_SPLINEIK)) {
        if (ct->tar) {
          if (ct->tar->type != OB_CURVE) {
            ct->tar = NULL;
            con->flag |= CONSTRAINT_DISABLE;
          }
          else {
            Curve *cu = ct->tar->data;

            /* auto-set 'Path' setting on curve so this works  */
            cu->flag |= CU_PATH;
          }
        }
      }
      else if (con->type == CONSTRAINT_TYPE_ARMATURE) {
        if (ct->tar) {
          if (ct->tar->type != OB_ARMATURE) {
            ct->tar = NULL;
            con->flag |= CONSTRAINT_DISABLE;
          }
          else if (!BKE_armature_find_bone_name(BKE_armature_from_object(ct->tar),
                                                ct->subtarget)) {
            /* bone must exist in armature... */
            con->flag |= CONSTRAINT_DISABLE;
          }
        }
      }
    }

    /* free any temporary targets */
    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, 0);
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
static void test_constraints(Main *bmain, Object *owner, bPoseChannel *pchan)
{
  bConstraint *curcon;
  ListBase *conlist = NULL;
  int type;

  if (owner == NULL) {
    return;
  }

  type = constraint_type_get(owner, pchan);

  /* Get the constraint list for this object */
  switch (type) {
    case CONSTRAINT_OBTYPE_OBJECT:
      conlist = &owner->constraints;
      break;
    case CONSTRAINT_OBTYPE_BONE:
      conlist = &pchan->constraints;
      break;
  }

  /* Check all constraints - is constraint valid? */
  if (conlist) {
    for (curcon = conlist->first; curcon; curcon = curcon->next) {
      test_constraint(bmain, owner, pchan, curcon, type);
    }
  }
}

void object_test_constraints(Main *bmain, Object *owner)
{
  if (owner->constraints.first) {
    test_constraints(bmain, owner, NULL);
  }

  if (owner->type == OB_ARMATURE && owner->pose) {
    bPoseChannel *pchan;

    for (pchan = owner->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->constraints.first) {
        test_constraints(bmain, owner, pchan);
      }
    }
  }
}

static void object_test_constraint(Main *bmain, Object *owner, bConstraint *con)
{
  if (owner->type == OB_ARMATURE && owner->pose) {
    if (BLI_findindex(&owner->constraints, con) != -1) {
      test_constraint(bmain, owner, NULL, con, CONSTRAINT_OBTYPE_OBJECT);
    }
    else {
      bPoseChannel *pchan;
      for (pchan = owner->pose->chanbase.first; pchan; pchan = pchan->next) {
        if (BLI_findindex(&pchan->constraints, con) != -1) {
          test_constraint(bmain, owner, pchan, con, CONSTRAINT_OBTYPE_BONE);
          break;
        }
      }
    }
  }
  else {
    test_constraint(bmain, owner, NULL, con, CONSTRAINT_OBTYPE_OBJECT);
  }
}

/*** generic functions for operators using constraint names and data context *********************/

#define EDIT_CONSTRAINT_OWNER_OBJECT 0
#define EDIT_CONSTRAINT_OWNER_BONE 1

static const EnumPropertyItem constraint_owner_items[] = {
    {EDIT_CONSTRAINT_OWNER_OBJECT,
     "OBJECT",
     0,
     "Object",
     "Edit a constraint on the active object"},
    {EDIT_CONSTRAINT_OWNER_BONE, "BONE", 0, "Bone", "Edit a constraint on the active bone"},
    {0, NULL, 0, NULL, NULL},
};

static bool edit_constraint_poll_generic(bContext *C, StructRNA *rna_type)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", rna_type);
  Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);

  if (!ptr.data) {
    CTX_wm_operator_poll_msg_set(C, "Context missing 'constraint'");
    return 0;
  }

  if (!ob) {
    CTX_wm_operator_poll_msg_set(C, "Context missing active object");
    return 0;
  }

  if (ID_IS_LINKED(ob) || (ptr.id.data && ID_IS_LINKED(ptr.id.data))) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit library data");
    return 0;
  }

  if (ID_IS_OVERRIDE_LIBRARY(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit constraints coming from library override");
    return (((bConstraint *)ptr.data)->flag & CONSTRAINT_OVERRIDE_LIBRARY_LOCAL) != 0;
  }

  return 1;
}

static bool edit_constraint_poll(bContext *C)
{
  return edit_constraint_poll_generic(C, &RNA_Constraint);
}

static void edit_constraint_properties(wmOperatorType *ot)
{
  PropertyRNA *prop;
  prop = RNA_def_string(
      ot->srna, "constraint", NULL, MAX_NAME, "Constraint", "Name of the constraint to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_enum(
      ot->srna, "owner", constraint_owner_items, 0, "Owner", "The owner of this constraint");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int edit_constraint_invoke_properties(bContext *C, wmOperator *op)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);
  bConstraint *con;
  ListBase *list;

  if (RNA_struct_property_is_set(op->ptr, "constraint") &&
      RNA_struct_property_is_set(op->ptr, "owner")) {
    return 1;
  }

  if (ptr.data) {
    con = ptr.data;
    RNA_string_set(op->ptr, "constraint", con->name);

    list = get_constraint_lb(ob, con, NULL);

    if (&ob->constraints == list) {
      RNA_enum_set(op->ptr, "owner", EDIT_CONSTRAINT_OWNER_OBJECT);
    }
    else {
      RNA_enum_set(op->ptr, "owner", EDIT_CONSTRAINT_OWNER_BONE);
    }

    return 1;
  }

  return 0;
}

static bConstraint *edit_constraint_property_get(wmOperator *op, Object *ob, int type)
{
  char constraint_name[MAX_NAME];
  int owner = RNA_enum_get(op->ptr, "owner");
  bConstraint *con;
  ListBase *list = NULL;

  RNA_string_get(op->ptr, "constraint", constraint_name);

  if (owner == EDIT_CONSTRAINT_OWNER_OBJECT) {
    list = &ob->constraints;
  }
  else if (owner == EDIT_CONSTRAINT_OWNER_BONE) {
    bPoseChannel *pchan = BKE_pose_channel_active(ob);
    if (pchan) {
      list = &pchan->constraints;
    }
    else {
#if 0
      if (G.debug & G_DEBUG) {
        printf("edit_constraint_property_get: No active bone for object '%s'\n",
               (ob) ? ob->id.name + 2 : "<None>");
      }
#endif
      return NULL;
    }
  }
  else {
#if 0
    if (G.debug & G_DEBUG) {
      printf("edit_constraint_property_get: defaulting to getting list in the standard way\n");
    }
#endif
    list = get_active_constraints(ob);
  }

  con = BKE_constraints_find_name(list, constraint_name);
#if 0
  if (G.debug & G_DEBUG) {
    printf("constraint found = %p, %s\n", (void *)con, (con) ? con->name : "<Not found>");
  }
#endif

  if (con && (type != 0) && (con->type != type)) {
    con = NULL;
  }

  return con;
}

/* ********************** CONSTRAINT-SPECIFIC STUFF ********************* */

/* ---------- Distance-Dependent Constraints ---------- */
/* StretchTo, Limit Distance */

static int stretchto_reset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_STRETCHTO);
  bStretchToConstraint *data = (con) ? (bStretchToConstraint *)con->data : NULL;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* just set original length to 0.0, which will cause a reset on next recalc */
  data->orglength = 0.0f;
  ED_object_constraint_update(bmain, ob);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, NULL);
  return OPERATOR_FINISHED;
}

static int stretchto_reset_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return stretchto_reset_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

static int limitdistance_reset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_DISTLIMIT);
  bDistLimitConstraint *data = (con) ? (bDistLimitConstraint *)con->data : NULL;

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* just set original length to 0.0, which will cause a reset on next recalc */
  data->dist = 0.0f;
  ED_object_constraint_update(bmain, ob);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, NULL);
  return OPERATOR_FINISHED;
}

static int limitdistance_reset_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return limitdistance_reset_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/* ------------- Child-Of Constraint ------------------ */

static void child_get_inverse_matrix(const bContext *C,
                                     Scene *scene,
                                     Object *ob,
                                     bConstraint *con,
                                     float invmat[4][4],
                                     const int owner)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);

  /* nullify inverse matrix first */
  unit_m4(invmat);

  if (owner == EDIT_CONSTRAINT_OWNER_BONE) {
    bPoseChannel *pchan;
    /* try to find a pose channel - assume that this is the constraint owner */
    /* TODO: get from context instead? */
    if (ob && ob->pose && (pchan = BKE_pose_channel_active(ob))) {
      bConstraint *con_last;
      /* calculate/set inverse matrix:
       * We just calculate all transform-stack eval up to but not including this constraint.
       * This is because inverse should just inverse correct for just the constraint's influence
       * when it gets applied; that is, at the time of application, we don't know anything about
       * what follows.
       */
      float imat[4][4], tmat[4][4];
      float pmat[4][4];

      /* make sure we passed the correct constraint */
      BLI_assert(BLI_findindex(&pchan->constraints, con) != -1);

      /* 1. calculate posemat where inverse doesn't exist yet (inverse was cleared above),
       * to use as baseline ("pmat") to derive delta from. This extra calc saves users
       * from having pressing "Clear Inverse" first
       */
      BKE_pose_where_is(depsgraph, scene, ob);
      copy_m4_m4(pmat, pchan->pose_mat);

      /* 2. knock out constraints starting from this one */
      con_last = pchan->constraints.last;
      pchan->constraints.last = con->prev;

      if (con->prev) {
        /* new end must not point to this one, else this chain cutting is useless */
        con->prev->next = NULL;
      }
      else {
        /* constraint was first */
        pchan->constraints.first = NULL;
      }

      /* 3. solve pose without disabled constraints */
      BKE_pose_where_is(depsgraph, scene, ob);

      /* 4. determine effect of constraint by removing the newly calculated
       * pchan->pose_mat from the original pchan->pose_mat, thus determining
       * the effect of the constraint
       */
      invert_m4_m4(imat, pchan->pose_mat);
      mul_m4_m4m4(tmat, pmat, imat);
      invert_m4_m4(invmat, tmat);

      /* 5. restore constraints */
      pchan->constraints.last = con_last;

      if (con->prev) {
        /* hook up prev to this one again */
        con->prev->next = con;
      }
      else {
        /* set as first again */
        pchan->constraints.first = con;
      }

      /* 6. recalculate pose with new inv-mat applied */
      BKE_pose_where_is(depsgraph, scene, ob);
    }
  }
  if (owner == EDIT_CONSTRAINT_OWNER_OBJECT) {
    if (ob) {
      Object workob;

      /* make sure we passed the correct constraint */
      BLI_assert(BLI_findindex(&ob->constraints, con) != -1);

      /* use BKE_object_workob_calc_parent to find inverse - just like for normal parenting */
      BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
      invert_m4_m4(invmat, workob.obmat);
    }
  }
}

/* ChildOf Constraint - set inverse callback */
static int childof_set_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_CHILDOF);
  bChildOfConstraint *data = (con) ? (bChildOfConstraint *)con->data : NULL;
  const int owner = RNA_enum_get(op->ptr, "owner");

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == NULL) {
    printf("DEBUG: Child-Of Set Inverse - object = '%s'\n", (ob) ? ob->id.name + 2 : "<None>");
    BKE_report(op->reports, RPT_ERROR, "Could not find constraint data for Child-Of Set Inverse");
    return OPERATOR_CANCELLED;
  }

  child_get_inverse_matrix(C, scene, ob, con, data->invmat, owner);

  ED_object_constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static int childof_set_inverse_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return childof_set_inverse_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void CONSTRAINT_OT_childof_set_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Inverse";
  ot->idname = "CONSTRAINT_OT_childof_set_inverse";
  ot->description = "Set inverse correction for ChildOf constraint";

  /* callbacks */
  ot->invoke = childof_set_inverse_invoke;
  ot->exec = childof_set_inverse_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/* ChildOf Constraint - clear inverse callback */
static int childof_clear_inverse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_CHILDOF);
  bChildOfConstraint *data = (con) ? (bChildOfConstraint *)con->data : NULL;

  if (data == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Child Of constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* simply clear the matrix */
  unit_m4(data->invmat);

  ED_object_constraint_update(bmain, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static int childof_clear_inverse_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return childof_clear_inverse_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void CONSTRAINT_OT_childof_clear_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Inverse";
  ot->idname = "CONSTRAINT_OT_childof_clear_inverse";
  ot->description = "Clear inverse correction for ChildOf constraint";

  /* callbacks */
  ot->invoke = childof_clear_inverse_invoke;
  ot->exec = childof_clear_inverse_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/* --------------- Follow Path Constraint ------------------ */

static int followpath_path_animate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_FOLLOWPATH);
  bFollowPathConstraint *data = (con) ? (bFollowPathConstraint *)con->data : NULL;

  bAction *act = NULL;
  FCurve *fcu = NULL;
  int sfra = RNA_int_get(op->ptr, "frame_start");
  int len = RNA_int_get(op->ptr, "length");
  float standardRange = 1.0;

  /* nearly impossible sanity check */
  if (data == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Follow Path constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* add F-Curve as appropriate */
  if (data->tar) {
    Curve *cu = (Curve *)data->tar->data;

    if (ELEM(NULL, cu->adt, cu->adt->action) ||
        (list_find_fcurve(&cu->adt->action->curves, "eval_time", 0) == NULL)) {
      /* create F-Curve for path animation */
      act = verify_adt_action(bmain, &cu->id, 1);
      fcu = verify_fcurve(bmain, act, NULL, NULL, "eval_time", 0, 1);

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
    PointerRNA ptr;
    PropertyRNA *prop;
    char *path;

    /* get RNA pointer to constraint's "offset_factor" property - to build RNA path */
    RNA_pointer_create(&ob->id, &RNA_FollowPathConstraint, con, &ptr);
    prop = RNA_struct_find_property(&ptr, "offset_factor");

    path = RNA_path_from_ID_to_property(&ptr, prop);

    /* create F-Curve for constraint */
    act = verify_adt_action(bmain, &ob->id, 1);
    fcu = verify_fcurve(bmain, act, NULL, NULL, path, 0, 1);

    /* standard vertical range - 0.0 to 1.0 */
    standardRange = 1.0f;

    /* enable "Use Fixed Position" so that animating this has effect */
    data->followflag |= FOLLOWPATH_STATIC;

    /* path needs to be freed */
    if (path) {
      MEM_freeN(path);
    }
  }

  /* setup dummy 'generator' modifier here to get 1-1 correspondence still working
   * and define basic slope of this curve based on the properties
   */
  if (!fcu->bezt && !fcu->fpt && !fcu->modifiers.first) {
    FModifier *fcm = add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR, fcu);
    FMod_Generator *gen = fcm->data;

    /* Assume that we have the following equation:
     *     y = Ax + B
     *         1    0       <-- coefficients array indices
     */
    float A = standardRange / (float)(len);
    float B = (float)(-sfra) * A;

    gen->coefficients[1] = A;
    gen->coefficients[0] = B;
  }

  /* updates... */
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  return OPERATOR_FINISHED;
}

static int followpath_path_animate_invoke(bContext *C,
                                          wmOperator *op,
                                          const wmEvent *UNUSED(event))
{
  /* hook up invoke properties for figuring out which constraint we're dealing with */
  if (edit_constraint_invoke_properties(C, op)) {
    return followpath_path_animate_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

/* ------------- Object Solver Constraint ------------------ */

static int objectsolver_set_inverse_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_OBJECTSOLVER);
  bObjectSolverConstraint *data = (con) ? (bObjectSolverConstraint *)con->data : NULL;
  const int owner = RNA_enum_get(op->ptr, "owner");

  /* despite 3 layers of checks, we may still not be able to find a constraint */
  if (data == NULL) {
    printf("DEBUG: Child-Of Set Inverse - object = '%s'\n", (ob) ? ob->id.name + 2 : "<None>");
    BKE_report(op->reports, RPT_ERROR, "Could not find constraint data for Child-Of Set Inverse");
    return OPERATOR_CANCELLED;
  }

  child_get_inverse_matrix(C, scene, ob, con, data->invmat, owner);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static int objectsolver_set_inverse_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return objectsolver_set_inverse_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void CONSTRAINT_OT_objectsolver_set_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Inverse";
  ot->idname = "CONSTRAINT_OT_objectsolver_set_inverse";
  ot->description = "Set inverse correction for ObjectSolver constraint";

  /* callbacks */
  ot->invoke = objectsolver_set_inverse_invoke;
  ot->exec = objectsolver_set_inverse_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

static int objectsolver_clear_inverse_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, CONSTRAINT_TYPE_OBJECTSOLVER);
  bObjectSolverConstraint *data = (con) ? (bObjectSolverConstraint *)con->data : NULL;

  if (data == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Child Of constraint not found");
    return OPERATOR_CANCELLED;
  }

  /* simply clear the matrix */
  unit_m4(data->invmat);

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

  return OPERATOR_FINISHED;
}

static int objectsolver_clear_inverse_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return objectsolver_clear_inverse_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void CONSTRAINT_OT_objectsolver_clear_inverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Inverse";
  ot->idname = "CONSTRAINT_OT_objectsolver_clear_inverse";
  ot->description = "Clear inverse correction for ObjectSolver constraint";

  /* callbacks */
  ot->invoke = objectsolver_clear_inverse_invoke;
  ot->exec = objectsolver_clear_inverse_exec;
  ot->poll = edit_constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  edit_constraint_properties(ot);
}

/***************************** BUTTONS ****************************/

void ED_object_constraint_set_active(Object *ob, bConstraint *con)
{
  ListBase *lb = get_constraint_lb(ob, con, NULL);

  /* lets be nice and escape if its active already */
  /* NOTE: this assumes that the stack doesn't have other active ones set... */
  if ((lb && con) && (con->flag & CONSTRAINT_ACTIVE)) {
    return;
  }

  BKE_constraints_active_set(lb, con);
}

void ED_object_constraint_update(Main *bmain, Object *ob)
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
  if (ob->proxy && ob->adt) {
    /* We need to make use of ugly #POSE_ANIMATION_WORKAROUND here too,
     * else anim data are not reloaded after calling `BKE_pose_rebuild()`,
     * which causes T43872.
     * Note that this is a bit wide here, since we cannot be sure whether there are some locked
     * proxy bones or not.
     * XXX Temp hack until new depsgraph hopefully solves this. */
    DEG_id_tag_update(&ob->id, ID_RECALC_ANIMATION);
  }
}

void ED_object_constraint_dependency_update(Main *bmain, Object *ob)
{
  ED_object_constraint_update(bmain, ob);

  if (ob->pose) {
    object_pose_tag_update(bmain, ob);
  }
  DEG_relations_tag_update(bmain);
}

void ED_object_constraint_tag_update(Main *bmain, Object *ob, bConstraint *con)
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
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_constraint_dependency_tag_update(Main *bmain, Object *ob, bConstraint *con)
{
  ED_object_constraint_tag_update(bmain, ob, con);

  if (ob->pose) {
    object_pose_tag_update(bmain, ob);
  }
  DEG_relations_tag_update(bmain);
}

static bool constraint_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  return (ptr.id.data && ptr.data);
}

static int constraint_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
  Object *ob = ptr.id.data;
  bConstraint *con = ptr.data;
  ListBase *lb = get_constraint_lb(ob, con, NULL);

  /* free the constraint */
  if (BKE_constraint_remove_ex(lb, ob, con, true)) {
    /* there's no active constraint now, so make sure this is the case */
    BKE_constraints_active_set(&ob->constraints, NULL);
    /* needed to set the flags on posebones correctly */
    ED_object_constraint_update(bmain, ob);

    /* relations */
    DEG_relations_tag_update(CTX_data_main(C));

    /* notifiers */
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);

    return OPERATOR_FINISHED;
  }
  else {
    /* couldn't remove due to some invalid data */
    return OPERATOR_CANCELLED;
  }
}

void CONSTRAINT_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Constraint";
  ot->idname = "CONSTRAINT_OT_delete";
  ot->description = "Remove constraint from constraint stack";

  /* callbacks */
  ot->exec = constraint_delete_exec;
  ot->poll = constraint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int constraint_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, 0);

  if (con && con->next) {
    ListBase *conlist = get_constraint_lb(ob, con, NULL);
    bConstraint *nextCon = con->next;

    /* insert the nominated constraint after the one that used to be after it */
    BLI_remlink(conlist, con);
    BLI_insertlinkafter(conlist, nextCon, con);

    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static int constraint_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return constraint_move_down_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

static int constraint_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
  bConstraint *con = edit_constraint_property_get(op, ob, 0);

  if (con && con->prev) {
    ListBase *conlist = get_constraint_lb(ob, con, NULL);
    bConstraint *prevCon = con->prev;

    /* insert the nominated constraint before the one that used to be before it */
    BLI_remlink(conlist, con);
    BLI_insertlinkbefore(conlist, prevCon, con);

    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static int constraint_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (edit_constraint_invoke_properties(C, op)) {
    return constraint_move_up_exec(C, op);
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

/***************************** OPERATORS ****************************/

/************************ remove constraint operators *********************/

static int pose_constraints_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Object *prev_ob = NULL;

  /* free constraints for all selected bones */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    BKE_constraints_free(&pchan->constraints);
    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_SPLINEIK | PCHAN_HAS_CONST);

    if (prev_ob != ob) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
      prev_ob = ob;
    }
  }
  CTX_DATA_END;

  /* force depsgraph to get recalculated since relationships removed */
  DEG_relations_tag_update(bmain);

  /* note, calling BIK_clear_data() isn't needed here */

  return OPERATOR_FINISHED;
}

void POSE_OT_constraints_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Constraints";
  ot->idname = "POSE_OT_constraints_clear";
  ot->description = "Clear all the constraints for the selected bones";

  /* callbacks */
  ot->exec = pose_constraints_clear_exec;
  ot->poll =
      ED_operator_posemode_exclusive;  // XXX - do we want to ensure there are selected bones too?
}

static int object_constraints_clear_exec(bContext *C, wmOperator *UNUSED(op))
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
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_constraints_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Object Constraints";
  ot->idname = "OBJECT_OT_constraints_clear";
  ot->description = "Clear all the constraints for the active Object only";

  /* callbacks */
  ot->exec = object_constraints_clear_exec;
  ot->poll = ED_operator_object_active_editable;
}

/************************ copy all constraints operators *********************/

static int pose_constraint_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bPoseChannel *pchan = CTX_data_active_pose_bone(C);

  /* don't do anything if bone doesn't exist or doesn't have any constraints */
  if (ELEM(NULL, pchan, pchan->constraints.first)) {
    BKE_report(op->reports, RPT_ERROR, "No active bone with constraints for copying");
    return OPERATOR_CANCELLED;
  }

  Object *prev_ob = NULL;

  /* copy all constraints from active posebone to all selected posebones */
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

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, NULL);

  return OPERATOR_FINISHED;
}

void POSE_OT_constraints_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Constraints to Selected Bones";
  ot->idname = "POSE_OT_constraints_copy";
  ot->description = "Copy constraints to other selected bones";

  /* api callbacks */
  ot->exec = pose_constraint_copy_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_constraint_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Object *obact = ED_object_active_context(C);

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
  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_ADDED, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_constraints_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Constraints to Selected Objects";
  ot->idname = "OBJECT_OT_constraints_copy";
  ot->description = "Copy constraints to other selected objects";

  /* api callbacks */
  ot->exec = object_constraint_copy_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ add constraint operators *********************/

/* get the Object and/or PoseChannel to use as target */
static bool get_new_constraint_target(
    bContext *C, int con_type, Object **tar_ob, bPoseChannel **tar_pchan, bool add)
{
  Object *obact = ED_object_active_context(C);
  bPoseChannel *pchanact = BKE_pose_channel_active(obact);
  bool only_curve = false, only_mesh = false, only_ob = false;
  bool found = false;

  /* clear tar_ob and tar_pchan fields before use
   * - assume for now that both always exist...
   */
  *tar_ob = NULL;
  *tar_pchan = NULL;

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
        if ((ob->type == OB_ARMATURE) && (ob->mode & OB_MODE_POSE) &&
            (!only_curve && !only_mesh)) {
          /* just use the active bone, and assume that it is visible + usable */
          *tar_ob = ob;
          *tar_pchan = BKE_pose_channel_active(ob);
          found = true;

          break;
        }
        else if (((!only_curve) || (ob->type == OB_CURVE)) &&
                 ((!only_mesh) || (ob->type == OB_MESH))) {
          /* set target */
          *tar_ob = ob;
          found = true;

          /* perform some special operations on the target */
          if (only_curve) {
            /* Curve-Path option must be enabled for follow-path constraints to be able to work */
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
    Base *base = BASACT(view_layer);
    Object *obt;

    /* add new target object */
    obt = BKE_object_add(bmain, scene, view_layer, OB_EMPTY, NULL);

    /* transform cent to global coords for loc */
    if (pchanact) {
      /* Since by default, IK targets the tip of the last bone,
       * use the tip of the active PoseChannel if adding a target for an IK Constraint. */
      if (con_type == CONSTRAINT_TYPE_KINEMATIC) {
        mul_v3_m4v3(obt->loc, obact->obmat, pchanact->pose_tail);
      }
      else {
        mul_v3_m4v3(obt->loc, obact->obmat, pchanact->pose_head);
      }
    }
    else {
      copy_v3_v3(obt->loc, obact->obmat[3]);
    }

    /* restore, BKE_object_add sets active */
    BASACT(view_layer) = base;
    ED_object_base_select(base, BA_SELECT);

    /* make our new target the new object */
    *tar_ob = obt;
    found = true;
  }

  /* return whether there's any target */
  return found;
}

/* used by add constraint operators to add the constraint required */
static int constraint_add_exec(
    bContext *C, wmOperator *op, Object *ob, ListBase *list, int type, const bool setTarget)
{
  Main *bmain = CTX_data_main(C);
  bPoseChannel *pchan;
  bConstraint *con;

  if (list == &ob->constraints) {
    pchan = NULL;
  }
  else {
    pchan = BKE_pose_channel_active(ob);

    /* ensure not to confuse object/pose adding */
    if (pchan == NULL) {
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
    con = BKE_constraint_add_for_pose(ob, pchan, NULL, type);
  }
  else {
    con = BKE_constraint_add_for_object(ob, NULL, type);
  }

  /* get the first selected object/bone, and make that the target
   * - apart from the buttons-window add buttons, we shouldn't add in this way
   */
  if (setTarget) {
    Object *tar_ob = NULL;
    bPoseChannel *tar_pchan = NULL;

    /* get the target objects, adding them as need be */
    if (get_new_constraint_target(C, type, &tar_ob, &tar_pchan, 1)) {
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

  /* do type-specific tweaking to the constraint settings  */
  switch (type) {
    case CONSTRAINT_TYPE_PYTHON: /* FIXME: this code is not really valid anymore */
    {
#ifdef WITH_PYTHON
      char *menustr;
      int scriptint = 0;
      /* popup a list of usable scripts */
      menustr = buildmenu_pyconstraints(bmain, NULL, &scriptint);
      /* XXX scriptint = pupmenu(menustr); */
      MEM_freeN(menustr);

      /* only add constraint if a script was chosen */
      if (scriptint) {
        /* add constraint */
        validate_pyconstraint_cb(bmain, con->data, &scriptint);

        /* make sure target allowance is set correctly */
        BPY_pyconstraint_update(ob, con);
      }
#endif
      break;
    }

    default:
      break;
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
    if (BKE_constraints_proxylocked_owner(ob, pchan) && ob->adt) {
      /* We need to make use of ugly POSE_ANIMATION_WORKAROUND here too,
       * else anim data are not reloaded after calling `BKE_pose_rebuild()`, which causes T43872.
       * XXX Temp hack until new depsgraph hopefully solves this. */
      DEG_id_tag_update(&ob->id, ID_RECALC_ANIMATION);
    }
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
static int object_constraint_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_active_context(C);
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
static int pose_constraint_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(ED_object_active_context(C));
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

  return constraint_add_exec(C, op, ob, get_active_constraints(ob), type, with_targets);
}

/* ------------------ */

/* Filters constraints that are only compatible with bones */
static const EnumPropertyItem *object_constraint_add_itemf(bContext *UNUSED(C),
                                                           PointerRNA *UNUSED(ptr),
                                                           PropertyRNA *UNUSED(prop),
                                                           bool *r_free)
{
  const EnumPropertyItem *item = rna_enum_constraint_type_items;
  EnumPropertyItem *object_constraint_items = NULL;
  int totitem = 0;

  while (item->identifier) {
    if ((item->value != CONSTRAINT_TYPE_KINEMATIC) && (item->value != CONSTRAINT_TYPE_SPLINEIK)) {
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

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_constraint_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "");
  RNA_def_enum_funcs(prop, object_constraint_add_itemf);
  ot->prop = prop;
}

void OBJECT_OT_constraint_add_with_targets(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Constraint (with Targets)";
  ot->description =
      "Add a constraint to the active object, with target (where applicable) set to the selected "
      "Objects/Bones";
  ot->idname = "OBJECT_OT_constraint_add_with_targets";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_constraint_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "");
  RNA_def_enum_funcs(prop, object_constraint_add_itemf);
  ot->prop = prop;
}

void POSE_OT_constraint_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Constraint";
  ot->description = "Add a constraint to the active bone";
  ot->idname = "POSE_OT_constraint_add";

  /* api callbacks */
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

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_constraint_add_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_constraint_type_items, 0, "Type", "");
}

/************************ IK Constraint operators *********************/
/* NOTE: only for Pose-Channels */
// TODO: should these be here, or back in editors/armature/poseobject.c again?

/* present menu with options + validation for targets to use */
static int pose_ik_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bPoseChannel *pchan = BKE_pose_channel_active(ob);
  bConstraint *con = NULL;

  uiPopupMenu *pup;
  uiLayout *layout;
  Object *tar_ob = NULL;
  bPoseChannel *tar_pchan = NULL;

  /* must have active bone */
  if (ELEM(NULL, ob, pchan)) {
    BKE_report(op->reports, RPT_ERROR, "Must have an active bone to add IK constraint to");
    return OPERATOR_CANCELLED;
  }

  /* bone must not have any constraints already */
  for (con = pchan->constraints.first; con; con = con->next) {
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
  if (get_new_constraint_target(C, CONSTRAINT_TYPE_KINEMATIC, &tar_ob, &tar_pchan, 0)) {
    /* bone target, or object target?
     * - the only thing that matters is that we want a target...
     */
    if (tar_pchan) {
      uiItemBooleanO(
          layout, IFACE_("To Active Bone"), ICON_NONE, "POSE_OT_ik_add", "with_targets", 1);
    }
    else {
      uiItemBooleanO(
          layout, IFACE_("To Active Object"), ICON_NONE, "POSE_OT_ik_add", "with_targets", 1);
    }
  }
  else {
    /* we have a choice of adding to a new empty, or not setting any target (targetless IK) */
    uiItemBooleanO(
        layout, IFACE_("To New Empty Object"), ICON_NONE, "POSE_OT_ik_add", "with_targets", 1);
    uiItemBooleanO(
        layout, IFACE_("Without Targets"), ICON_NONE, "POSE_OT_ik_add", "with_targets", 0);
  }

  /* finish building the menu, and process it (should result in calling self again) */
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

/* call constraint_add_exec() to add the IK constraint */
static int pose_ik_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const bool with_targets = RNA_boolean_get(op->ptr, "with_targets");

  /* add the constraint - all necessary checks should have
   * been done by the invoke() callback already... */
  return constraint_add_exec(
      C, op, ob, get_active_constraints(ob), CONSTRAINT_TYPE_KINEMATIC, with_targets);
}

void POSE_OT_ik_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add IK to Bone";
  ot->description = "Add IK Constraint to the active Bone";
  ot->idname = "POSE_OT_ik_add";

  /* api callbacks */
  ot->invoke = pose_ik_add_invoke;
  ot->exec = pose_ik_add_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "with_targets",
                  1,
                  "With Targets",
                  "Assign IK Constraint with targets derived from the select bones/objects");
}

/* ------------------ */

/* remove IK constraints from selected bones */
static int pose_ik_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *prev_ob = NULL;

  /* only remove IK Constraints */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    bConstraint *con, *next;

    /* TODO: should we be checking if these constraints were local
     * before we try and remove them? */
    for (con = pchan->constraints.first; con; con = next) {
      next = con->next;
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        BKE_constraint_remove(&pchan->constraints, con);
      }
    }
    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_TARGET);

    if (prev_ob != ob) {
      prev_ob = ob;

      /* Refresh depsgraph. */
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

      /* Note, notifier might evolve. */
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

  /* api callbacks */
  ot->exec = pose_ik_clear_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
