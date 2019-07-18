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
 * Armature EditMode tools - transforms, chain based editing, and other settings
 */

/** \file
 * \ingroup edarmature
 */

#include <assert.h>

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_ghash.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* ************************** Object Tools Exports ******************************* */
/* NOTE: these functions are exported to the Object module to be called from the tools there */

void ED_armature_transform_apply(Main *bmain, Object *ob, float mat[4][4], const bool do_props)
{
  bArmature *arm = ob->data;

  /* Put the armature into editmode */
  ED_armature_to_edit(arm);

  /* Transform the bones */
  ED_armature_transform_bones(arm, mat, do_props);

  /* Turn the list into an armature */
  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);
}

void ED_armature_transform_bones(struct bArmature *arm, float mat[4][4], const bool do_props)
{
  EditBone *ebone;
  float scale = mat4_to_scale(mat); /* store the scale of the matrix here to use on envelopes */
  float mat3[3][3];

  copy_m3_m4(mat3, mat);
  normalize_m3(mat3);
  /* Do the rotations */
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    float tmat[3][3];

    /* find the current bone's roll matrix */
    ED_armature_ebone_to_mat3(ebone, tmat);

    /* transform the roll matrix */
    mul_m3_m3m3(tmat, mat3, tmat);

    /* transform the bone */
    mul_m4_v3(mat, ebone->head);
    mul_m4_v3(mat, ebone->tail);

    /* apply the transformed roll back */
    mat3_to_vec_roll(tmat, NULL, &ebone->roll);

    if (do_props) {
      ebone->rad_head *= scale;
      ebone->rad_tail *= scale;
      ebone->dist *= scale;

      /* we could be smarter and scale by the matrix along the x & z axis */
      ebone->xwidth *= scale;
      ebone->zwidth *= scale;
    }
  }
}

void ED_armature_transform(Main *bmain, bArmature *arm, float mat[4][4], const bool do_props)
{
  if (arm->edbo) {
    ED_armature_transform_bones(arm, mat, do_props);
  }
  else {
    /* Put the armature into editmode */
    ED_armature_to_edit(arm);

    /* Transform the bones */
    ED_armature_transform_bones(arm, mat, do_props);

    /* Go back to object mode*/
    ED_armature_from_edit(bmain, arm);
    ED_armature_edit_free(arm);
  }
}

/* exported for use in editors/object/ */
/* 0 == do center, 1 == center new, 2 == center cursor */
void ED_armature_origin_set(
    Main *bmain, Object *ob, const float cursor[3], int centermode, int around)
{
  const bool is_editmode = BKE_object_is_in_editmode(ob);
  EditBone *ebone;
  bArmature *arm = ob->data;
  float cent[3];

  /* Put the armature into editmode */
  if (is_editmode == false) {
    ED_armature_to_edit(arm);
  }

  /* Find the centerpoint */
  if (centermode == 2) {
    copy_v3_v3(cent, cursor);
    invert_m4_m4(ob->imat, ob->obmat);
    mul_m4_v3(ob->imat, cent);
  }
  else {
    if (around == V3D_AROUND_CENTER_MEDIAN) {
      int total = 0;
      zero_v3(cent);
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        total += 2;
        add_v3_v3(cent, ebone->head);
        add_v3_v3(cent, ebone->tail);
      }
      if (total) {
        mul_v3_fl(cent, 1.0f / (float)total);
      }
    }
    else {
      float min[3], max[3];
      INIT_MINMAX(min, max);
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        minmax_v3v3_v3(min, max, ebone->head);
        minmax_v3v3_v3(min, max, ebone->tail);
      }
      mid_v3_v3v3(cent, min, max);
    }
  }

  /* Do the adjustments */
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    sub_v3_v3(ebone->head, cent);
    sub_v3_v3(ebone->tail, cent);
  }

  /* Turn the list into an armature */
  if (is_editmode == false) {
    ED_armature_from_edit(bmain, arm);
    ED_armature_edit_free(arm);
  }

  /* Adjust object location for new centerpoint */
  if (centermode && (is_editmode == false)) {
    mul_mat3_m4_v3(ob->obmat, cent); /* omit translation part */
    add_v3_v3(ob->loc, cent);
  }
}

/* ********************************* Roll ******************************* */

/* adjust bone roll to align Z axis with vector
 * vec is in local space and is normalized
 */
float ED_armature_ebone_roll_to_vector(const EditBone *bone,
                                       const float align_axis[3],
                                       const bool axis_only)
{
  float mat[3][3], nor[3];
  float vec[3], align_axis_proj[3], roll = 0.0f;

  BLI_ASSERT_UNIT_V3(align_axis);

  sub_v3_v3v3(nor, bone->tail, bone->head);

  /* If tail == head or the bone is aligned with the axis... */
  if (normalize_v3(nor) <= FLT_EPSILON ||
      (fabsf(dot_v3v3(align_axis, nor)) >= (1.0f - FLT_EPSILON))) {
    return roll;
  }

  vec_roll_to_mat3_normalized(nor, 0.0f, mat);

  /* project the new_up_axis along the normal */
  project_v3_v3v3_normalized(vec, align_axis, nor);
  sub_v3_v3v3(align_axis_proj, align_axis, vec);

  if (axis_only) {
    if (angle_v3v3(align_axis_proj, mat[2]) > (float)(M_PI_2)) {
      negate_v3(align_axis_proj);
    }
  }

  roll = angle_v3v3(align_axis_proj, mat[2]);

  cross_v3_v3v3(vec, mat[2], align_axis_proj);

  if (dot_v3v3(vec, nor) < 0.0f) {
    return -roll;
  }
  return roll;
}

/* note, ranges arithmetic is used below */
typedef enum eCalcRollTypes {
  /* pos */
  CALC_ROLL_POS_X = 0,
  CALC_ROLL_POS_Y,
  CALC_ROLL_POS_Z,

  CALC_ROLL_TAN_POS_X,
  CALC_ROLL_TAN_POS_Z,

  /* neg */
  CALC_ROLL_NEG_X,
  CALC_ROLL_NEG_Y,
  CALC_ROLL_NEG_Z,

  CALC_ROLL_TAN_NEG_X,
  CALC_ROLL_TAN_NEG_Z,

  /* no sign */
  CALC_ROLL_ACTIVE,
  CALC_ROLL_VIEW,
  CALC_ROLL_CURSOR,
} eCalcRollTypes;

static const EnumPropertyItem prop_calc_roll_types[] = {
    {0, "", 0, N_("Positive"), ""},
    {CALC_ROLL_TAN_POS_X, "POS_X", 0, "Local +X Tangent", ""},
    {CALC_ROLL_TAN_POS_Z, "POS_Z", 0, "Local +Z Tangent", ""},

    {CALC_ROLL_POS_X, "GLOBAL_POS_X", 0, "Global +X Axis", ""},
    {CALC_ROLL_POS_Y, "GLOBAL_POS_Y", 0, "Global +Y Axis", ""},
    {CALC_ROLL_POS_Z, "GLOBAL_POS_Z", 0, "Global +Z Axis", ""},

    {0, "", 0, N_("Negative"), ""},

    {CALC_ROLL_TAN_NEG_X, "NEG_X", 0, "Local -X Tangent", ""},
    {CALC_ROLL_TAN_NEG_Z, "NEG_Z", 0, "Local -Z Tangent", ""},

    {CALC_ROLL_NEG_X, "GLOBAL_NEG_X", 0, "Global -X Axis", ""},
    {CALC_ROLL_NEG_Y, "GLOBAL_NEG_Y", 0, "Global -Y Axis", ""},
    {CALC_ROLL_NEG_Z, "GLOBAL_NEG_Z", 0, "Global -Z Axis", ""},

    {0, "", 0, N_("Other"), ""},
    {CALC_ROLL_ACTIVE, "ACTIVE", 0, "Active Bone", ""},
    {CALC_ROLL_VIEW, "VIEW", 0, "View Axis", ""},
    {CALC_ROLL_CURSOR, "CURSOR", 0, "Cursor", ""},
    {0, NULL, 0, NULL, NULL},
};

static int armature_calc_roll_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_edit_object(C);
  int ret = OPERATOR_FINISHED;

  eCalcRollTypes type = RNA_enum_get(op->ptr, "type");
  const bool axis_only = RNA_boolean_get(op->ptr, "axis_only");
  /* axis_flip when matching the active bone never makes sense */
  bool axis_flip = ((type >= CALC_ROLL_ACTIVE) ? RNA_boolean_get(op->ptr, "axis_flip") :
                                                 (type >= CALC_ROLL_TAN_NEG_X) ? true : false);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    float imat[3][3];
    EditBone *ebone;

    if ((type >= CALC_ROLL_NEG_X) && (type <= CALC_ROLL_TAN_NEG_Z)) {
      type -= (CALC_ROLL_ACTIVE - CALC_ROLL_NEG_X);
      axis_flip = true;
    }

    copy_m3_m4(imat, ob->obmat);
    invert_m3(imat);

    if (type == CALC_ROLL_CURSOR) { /* Cursor */
      Scene *scene = CTX_data_scene(C);
      float cursor_local[3];
      const View3DCursor *cursor = &scene->cursor;

      invert_m4_m4(ob->imat, ob->obmat);
      copy_v3_v3(cursor_local, cursor->location);
      mul_m4_v3(ob->imat, cursor_local);

      /* cursor */
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) {
          float cursor_rel[3];
          sub_v3_v3v3(cursor_rel, cursor_local, ebone->head);
          if (axis_flip) {
            negate_v3(cursor_rel);
          }
          if (normalize_v3(cursor_rel) != 0.0f) {
            ebone->roll = ED_armature_ebone_roll_to_vector(ebone, cursor_rel, axis_only);
            changed = true;
          }
        }
      }
    }
    else if (ELEM(type, CALC_ROLL_TAN_POS_X, CALC_ROLL_TAN_POS_Z)) {
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if (ebone->parent) {
          bool is_edit = (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone));
          bool is_edit_parent = (EBONE_VISIBLE(arm, ebone->parent) &&
                                 EBONE_EDITABLE(ebone->parent));

          if (is_edit || is_edit_parent) {
            EditBone *ebone_other = ebone->parent;
            float dir_a[3];
            float dir_b[3];
            float vec[3];
            bool is_vec_zero;

            sub_v3_v3v3(dir_a, ebone->tail, ebone->head);
            normalize_v3(dir_a);

            /* find the first bone in the chane with a different direction */
            do {
              sub_v3_v3v3(dir_b, ebone_other->head, ebone_other->tail);
              normalize_v3(dir_b);

              if (type == CALC_ROLL_TAN_POS_Z) {
                cross_v3_v3v3(vec, dir_a, dir_b);
              }
              else {
                add_v3_v3v3(vec, dir_a, dir_b);
              }
            } while ((is_vec_zero = (normalize_v3(vec) < 0.00001f)) &&
                     (ebone_other = ebone_other->parent));

            if (!is_vec_zero) {
              if (axis_flip) {
                negate_v3(vec);
              }

              if (is_edit) {
                ebone->roll = ED_armature_ebone_roll_to_vector(ebone, vec, axis_only);
                changed = true;
              }

              /* parentless bones use cross product with child */
              if (is_edit_parent) {
                if (ebone->parent->parent == NULL) {
                  ebone->parent->roll = ED_armature_ebone_roll_to_vector(
                      ebone->parent, vec, axis_only);
                  changed = true;
                }
              }
            }
          }
        }
      }
    }
    else {
      float vec[3] = {0.0f, 0.0f, 0.0f};
      if (type == CALC_ROLL_VIEW) { /* View */
        RegionView3D *rv3d = CTX_wm_region_view3d(C);
        if (rv3d == NULL) {
          BKE_report(op->reports, RPT_ERROR, "No region view3d available");
          ret = OPERATOR_CANCELLED;
          goto cleanup;
        }

        copy_v3_v3(vec, rv3d->viewinv[2]);
        mul_m3_v3(imat, vec);
      }
      else if (type == CALC_ROLL_ACTIVE) {
        float mat[3][3];
        bArmature *arm_active = ob_active->data;
        ebone = (EditBone *)arm_active->act_edbone;
        if (ebone == NULL) {
          BKE_report(op->reports, RPT_ERROR, "No active bone set");
          ret = OPERATOR_CANCELLED;
          goto cleanup;
        }

        ED_armature_ebone_to_mat3(ebone, mat);
        copy_v3_v3(vec, mat[2]);
      }
      else { /* Axis */
        assert(type <= 5);
        if (type < 3) {
          vec[type] = 1.0f;
        }
        else {
          vec[type - 2] = -1.0f;
        }
        mul_m3_v3(imat, vec);
        normalize_v3(vec);
      }

      if (axis_flip) {
        negate_v3(vec);
      }

      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) {
          /* roll func is a callback which assumes that all is well */
          ebone->roll = ED_armature_ebone_roll_to_vector(ebone, vec, axis_only);
          changed = true;
        }
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if ((EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) == 0) {
          EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if (ebone_mirr && (EBONE_VISIBLE(arm, ebone_mirr) && EBONE_EDITABLE(ebone_mirr))) {
            ebone->roll = -ebone_mirr->roll;
          }
        }
      }
    }

    if (changed) {
      /* note, notifier might evolve */
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    }
  }

cleanup:
  MEM_freeN(objects);
  return ret;
}

void ARMATURE_OT_calculate_roll(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Recalculate Roll";
  ot->idname = "ARMATURE_OT_calculate_roll";
  ot->description = "Automatically fix alignment of select bones' axes";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = armature_calc_roll_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_calc_roll_types, CALC_ROLL_TAN_POS_X, "Type", "");
  RNA_def_boolean(ot->srna, "axis_flip", 0, "Flip Axis", "Negate the alignment axis");
  RNA_def_boolean(ot->srna,
                  "axis_only",
                  0,
                  "Shortest Rotation",
                  "Ignore the axis direction, use the shortest rotation to align");
}

static int armature_roll_clear_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const float roll = RNA_float_get(op->ptr, "roll");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    for (EditBone *ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) {
        /* Roll func is a callback which assumes that all is well. */
        ebone->roll = roll;
        changed = true;
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      for (EditBone *ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if ((EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) == 0) {
          EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if (ebone_mirr && (EBONE_VISIBLE(arm, ebone_mirr) && EBONE_EDITABLE(ebone_mirr))) {
            ebone->roll = -ebone_mirr->roll;
            changed = true;
          }
        }
      }
    }

    if (changed) {
      /* Note, notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_roll_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Roll";
  ot->idname = "ARMATURE_OT_roll_clear";
  ot->description = "Clear roll for selected bones";

  /* api callbacks */
  ot->exec = armature_roll_clear_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_rotation(ot->srna,
                         "roll",
                         0,
                         NULL,
                         DEG2RADF(-360.0f),
                         DEG2RADF(360.0f),
                         "Roll",
                         "",
                         DEG2RADF(-360.0f),
                         DEG2RADF(360.0f));
}

/* ******************************** Chain-Based Tools ********************************* */

/* temporary data-structure for merge/fill bones */
typedef struct EditBonePoint {
  struct EditBonePoint *next, *prev;

  EditBone *head_owner; /* EditBone which uses this point as a 'head' point */
  EditBone *tail_owner; /* EditBone which uses this point as a 'tail' point */

  float vec[3]; /* the actual location of the point in local/EditMode space */
} EditBonePoint;

/* find chain-tips (i.e. bones without children) */
static void chains_find_tips(ListBase *edbo, ListBase *list)
{
  EditBone *curBone, *ebo;
  LinkData *ld;

  /* note: this is potentially very slow ... there's got to be a better way */
  for (curBone = edbo->first; curBone; curBone = curBone->next) {
    short stop = 0;

    /* is this bone contained within any existing chain? (skip if so) */
    for (ld = list->first; ld; ld = ld->next) {
      for (ebo = ld->data; ebo; ebo = ebo->parent) {
        if (ebo == curBone) {
          stop = 1;
          break;
        }
      }

      if (stop) {
        break;
      }
    }
    /* skip current bone if it is part of an existing chain */
    if (stop) {
      continue;
    }

    /* is any existing chain part of the chain formed by this bone? */
    stop = 0;
    for (ebo = curBone->parent; ebo; ebo = ebo->parent) {
      for (ld = list->first; ld; ld = ld->next) {
        if (ld->data == ebo) {
          ld->data = curBone;
          stop = 1;
          break;
        }
      }

      if (stop) {
        break;
      }
    }
    /* current bone has already been added to a chain? */
    if (stop) {
      continue;
    }

    /* add current bone to a new chain */
    ld = MEM_callocN(sizeof(LinkData), "BoneChain");
    ld->data = curBone;
    BLI_addtail(list, ld);
  }
}

/* --------------------- */

static void fill_add_joint(EditBone *ebo, short eb_tail, ListBase *points)
{
  EditBonePoint *ebp;
  float vec[3];
  short found = 0;

  if (eb_tail) {
    copy_v3_v3(vec, ebo->tail);
  }
  else {
    copy_v3_v3(vec, ebo->head);
  }

  for (ebp = points->first; ebp; ebp = ebp->next) {
    if (equals_v3v3(ebp->vec, vec)) {
      if (eb_tail) {
        if ((ebp->head_owner) && (ebp->head_owner->parent == ebo)) {
          /* so this bone's tail owner is this bone */
          ebp->tail_owner = ebo;
          found = 1;
          break;
        }
      }
      else {
        if ((ebp->tail_owner) && (ebo->parent == ebp->tail_owner)) {
          /* so this bone's head owner is this bone */
          ebp->head_owner = ebo;
          found = 1;
          break;
        }
      }
    }
  }

  /* allocate a new point if no existing point was related */
  if (found == 0) {
    ebp = MEM_callocN(sizeof(EditBonePoint), "EditBonePoint");

    if (eb_tail) {
      copy_v3_v3(ebp->vec, ebo->tail);
      ebp->tail_owner = ebo;
    }
    else {
      copy_v3_v3(ebp->vec, ebo->head);
      ebp->head_owner = ebo;
    }

    BLI_addtail(points, ebp);
  }
}

/* bone adding between selected joints */
static int armature_fill_bones_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ListBase points = {NULL, NULL};
  EditBone *newbone = NULL;
  int count;
  bool mixed_object_error = false;

  /* loop over all bones, and only consider if visible */
  bArmature *arm = NULL;
  CTX_DATA_BEGIN_WITH_ID (C, EditBone *, ebone, visible_bones, bArmature *, arm_iter) {
    bool check = false;
    if (!(ebone->flag & BONE_CONNECTED) && (ebone->flag & BONE_ROOTSEL)) {
      fill_add_joint(ebone, 0, &points);
      check = true;
    }
    if (ebone->flag & BONE_TIPSEL) {
      fill_add_joint(ebone, 1, &points);
      check = true;
    }

    if (check) {
      if (arm && (arm != arm_iter)) {
        mixed_object_error = true;
      }
      arm = arm_iter;
    }
  }
  CTX_DATA_END;

  /* the number of joints determines how we fill:
   *  1) between joint and cursor (joint=head, cursor=tail)
   *  2) between the two joints (order is dependent on active-bone/hierarchy)
   *  3+) error (a smarter method involving finding chains needs to be worked out
   */
  count = BLI_listbase_count(&points);

  if (count == 0) {
    BKE_report(op->reports, RPT_ERROR, "No joints selected");
    return OPERATOR_CANCELLED;
  }
  else if (mixed_object_error) {
    BKE_report(op->reports, RPT_ERROR, "Bones for different objects selected");
    BLI_freelistN(&points);
    return OPERATOR_CANCELLED;
  }

  Object *obedit = NULL;
  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    FOREACH_OBJECT_IN_EDIT_MODE_BEGIN (view_layer, v3d, ob_iter) {
      if (ob_iter->data == arm) {
        obedit = ob_iter;
      }
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  BLI_assert(obedit != NULL);

  if (count == 1) {
    EditBonePoint *ebp;
    float curs[3];

    /* Get Points - selected joint */
    ebp = points.first;

    /* Get points - cursor (tail) */
    invert_m4_m4(obedit->imat, obedit->obmat);
    mul_v3_m4v3(curs, obedit->imat, scene->cursor.location);

    /* Create a bone */
    newbone = add_points_bone(obedit, ebp->vec, curs);
  }
  else if (count == 2) {
    EditBonePoint *ebp_a, *ebp_b;
    float head[3], tail[3];
    short headtail = 0;

    /* check that the points don't belong to the same bone */
    ebp_a = (EditBonePoint *)points.first;
    ebp_b = ebp_a->next;

    if (((ebp_a->head_owner == ebp_b->tail_owner) && (ebp_a->head_owner != NULL)) ||
        ((ebp_a->tail_owner == ebp_b->head_owner) && (ebp_a->tail_owner != NULL))) {
      BKE_report(op->reports, RPT_ERROR, "Same bone selected...");
      BLI_freelistN(&points);
      return OPERATOR_CANCELLED;
    }

    /* find which one should be the 'head' */
    if ((ebp_a->head_owner && ebp_b->head_owner) || (ebp_a->tail_owner && ebp_b->tail_owner)) {
      /* use active, nice predictable */
      if (arm->act_edbone && ELEM(arm->act_edbone, ebp_a->head_owner, ebp_a->tail_owner)) {
        headtail = 1;
      }
      else if (arm->act_edbone && ELEM(arm->act_edbone, ebp_b->head_owner, ebp_b->tail_owner)) {
        headtail = 2;
      }
      else {
        /* rule: whichever one is closer to 3d-cursor */
        float curs[3];
        float dist_sq_a, dist_sq_b;

        /* get cursor location */
        invert_m4_m4(obedit->imat, obedit->obmat);
        mul_v3_m4v3(curs, obedit->imat, scene->cursor.location);

        /* get distances */
        dist_sq_a = len_squared_v3v3(ebp_a->vec, curs);
        dist_sq_b = len_squared_v3v3(ebp_b->vec, curs);

        /* compare distances - closer one therefore acts as direction for bone to go */
        headtail = (dist_sq_a < dist_sq_b) ? 2 : 1;
      }
    }
    else if (ebp_a->head_owner) {
      headtail = 1;
    }
    else if (ebp_b->head_owner) {
      headtail = 2;
    }

    /* assign head/tail combinations */
    if (headtail == 2) {
      copy_v3_v3(head, ebp_a->vec);
      copy_v3_v3(tail, ebp_b->vec);
    }
    else if (headtail == 1) {
      copy_v3_v3(head, ebp_b->vec);
      copy_v3_v3(tail, ebp_a->vec);
    }

    /* add new bone and parent it to the appropriate end */
    if (headtail) {
      newbone = add_points_bone(obedit, head, tail);

      /* do parenting (will need to set connected flag too) */
      if (headtail == 2) {
        /* ebp tail or head - tail gets priority */
        if (ebp_a->tail_owner) {
          newbone->parent = ebp_a->tail_owner;
        }
        else {
          newbone->parent = ebp_a->head_owner;
        }
      }
      else {
        /* ebp_b tail or head - tail gets priority */
        if (ebp_b->tail_owner) {
          newbone->parent = ebp_b->tail_owner;
        }
        else {
          newbone->parent = ebp_b->head_owner;
        }
      }

      /* don't set for bone connecting two head points of bones */
      if (ebp_a->tail_owner || ebp_b->tail_owner) {
        newbone->flag |= BONE_CONNECTED;
      }
    }
  }
  else {
    BKE_reportf(op->reports, RPT_ERROR, "Too many points selected: %d", count);
    BLI_freelistN(&points);
    return OPERATOR_CANCELLED;
  }

  if (newbone) {
    ED_armature_edit_deselect_all(obedit);
    arm->act_edbone = newbone;
    newbone->flag |= BONE_TIPSEL;
  }

  /* updates */
  ED_armature_edit_refresh_layer_used(arm);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);

  /* free points */
  BLI_freelistN(&points);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_fill(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Fill Between Joints";
  ot->idname = "ARMATURE_OT_fill";
  ot->description = "Add bone between selected joint(s) and/or 3D-Cursor";

  /* callbacks */
  ot->exec = armature_fill_bones_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------------------- */

/* this function merges between two bones, removes them and those in-between,
 * and adjusts the parent relationships for those in-between
 */
static void bones_merge(
    Object *obedit, EditBone *start, EditBone *end, EditBone *endchild, ListBase *chains)
{
  bArmature *arm = obedit->data;
  EditBone *ebo, *ebone, *newbone;
  LinkData *chain;
  float head[3], tail[3];

  /* check if same bone */
  if (start == end) {
    if (G.debug & G_DEBUG) {
      printf("Error: same bone!\n");
      printf("\tstart = %s, end = %s\n", start->name, end->name);
    }
  }

  /* step 1: add a new bone
   * - head = head/tail of start (default head)
   * - tail = head/tail of end (default tail)
   * - parent = parent of start
   */
  if ((start->flag & BONE_TIPSEL) && (start->flag & BONE_SELECTED) == 0) {
    copy_v3_v3(head, start->tail);
  }
  else {
    copy_v3_v3(head, start->head);
  }
  if ((end->flag & BONE_ROOTSEL) && (end->flag & BONE_SELECTED) == 0) {
    copy_v3_v3(tail, end->head);
  }
  else {
    copy_v3_v3(tail, end->tail);
  }
  newbone = add_points_bone(obedit, head, tail);
  newbone->parent = start->parent;

  /* TODO, copy more things to the new bone */
  newbone->flag = start->flag & (BONE_HINGE | BONE_NO_DEFORM | BONE_NO_SCALE |
                                 BONE_NO_CYCLICOFFSET | BONE_NO_LOCAL_LOCATION | BONE_DONE);

  /* Step 2a: reparent any side chains which may be parented to any bone in the chain
   * of bones to merge - potentially several tips for side chains leading to some tree exist.
   */
  for (chain = chains->first; chain; chain = chain->next) {
    /* Traverse down chain until we hit the bottom or if we run into the tip of the chain of bones
     * we're merging (need to stop in this case to avoid corrupting this chain too!).
     */
    for (ebone = chain->data; (ebone) && (ebone != end); ebone = ebone->parent) {
      short found = 0;

      /* Check if this bone is parented to one in the merging chain
       * ! WATCHIT: must only go check until end of checking chain
       */
      for (ebo = end; (ebo) && (ebo != start->parent); ebo = ebo->parent) {
        /* side-chain found? --> remap parent to new bone, then we're done with this chain :) */
        if (ebone->parent == ebo) {
          ebone->parent = newbone;
          found = 1;
          break;
        }
      }

      /* carry on to the next tip now  */
      if (found) {
        break;
      }
    }
  }

  /* step 2b: parent child of end to newbone (child from this chain) */
  if (endchild) {
    endchild->parent = newbone;
  }

  /* step 3: delete all bones between and including start and end */
  for (ebo = end; ebo; ebo = ebone) {
    ebone = (ebo == start) ? (NULL) : (ebo->parent);
    bone_free(arm, ebo);
  }

  newbone->flag |= (BONE_ROOTSEL | BONE_TIPSEL | BONE_SELECTED);
  ED_armature_edit_sync_selection(arm->edbo);
}

static int armature_merge_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const short type = RNA_enum_get(op->ptr, "type");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;

    /* for now, there's only really one type of merging that's performed... */
    if (type == 1) {
      /* go down chains, merging bones */
      ListBase chains = {NULL, NULL};
      LinkData *chain, *nchain;
      EditBone *ebo;

      armature_tag_select_mirrored(arm);

      /* get chains (ends on chains) */
      chains_find_tips(arm->edbo, &chains);
      if (BLI_listbase_is_empty(&chains)) {
        continue;
      }

      /* each 'chain' is the last bone in the chain (with no children) */
      for (chain = chains.first; chain; chain = nchain) {
        EditBone *bstart = NULL, *bend = NULL;
        EditBone *bchild = NULL, *child = NULL;

        /* temporarily remove chain from list of chains */
        nchain = chain->next;
        BLI_remlink(&chains, chain);

        /* only consider bones that are visible and selected */
        for (ebo = chain->data; ebo; child = ebo, ebo = ebo->parent) {
          /* check if visible + selected */
          if (EBONE_VISIBLE(arm, ebo) && ((ebo->flag & BONE_CONNECTED) || (ebo->parent == NULL)) &&
              (ebo->flag & BONE_SELECTED)) {
            /* set either end or start (end gets priority, unless it is already set) */
            if (bend == NULL) {
              bend = ebo;
              bchild = child;
            }
            else {
              bstart = ebo;
            }
          }
          else {
            /* chain is broken... merge any continuous segments then clear */
            if (bstart && bend) {
              bones_merge(obedit, bstart, bend, bchild, &chains);
            }

            bstart = NULL;
            bend = NULL;
            bchild = NULL;
          }
        }

        /* merge from bstart to bend if something not merged */
        if (bstart && bend) {
          bones_merge(obedit, bstart, bend, bchild, &chains);
        }

        /* put back link */
        BLI_insertlinkbefore(&chains, nchain, chain);
      }

      armature_tag_unselect(arm);

      BLI_freelistN(&chains);
    }

    /* updates */
    ED_armature_edit_sync_selection(arm->edbo);
    ED_armature_edit_refresh_layer_used(arm);
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_merge(wmOperatorType *ot)
{
  static const EnumPropertyItem merge_types[] = {
      {1, "WITHIN_CHAIN", 0, "Within Chains", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Merge Bones";
  ot->idname = "ARMATURE_OT_merge";
  ot->description = "Merge continuous chains of selected bones";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = armature_merge_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", merge_types, 0, "Type", "");
}

/* --------------------- */

/* Switch Direction operator:
 * Currently, this does not use context loops, as context loops do not make it
 * easy to retrieve any hierarchical/chain relationships which are necessary for
 * this to be done easily.
 */

/* helper to clear BONE_TRANSFORM flags */
static void armature_clear_swap_done_flags(bArmature *arm)
{
  EditBone *ebone;

  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    ebone->flag &= ~BONE_TRANSFORM;
  }
}

static int armature_switch_direction_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;

    ListBase chains = {NULL, NULL};
    LinkData *chain;

    /* get chains of bones (ends on chains) */
    chains_find_tips(arm->edbo, &chains);
    if (BLI_listbase_is_empty(&chains)) {
      continue;
    }

    /* ensure that mirror bones will also be operated on */
    armature_tag_select_mirrored(arm);

    /* clear BONE_TRANSFORM flags
     * - used to prevent duplicate/canceling operations from occurring [#34123]
     * - BONE_DONE cannot be used here as that's already used for mirroring
     */
    armature_clear_swap_done_flags(arm);

    /* loop over chains, only considering selected and visible bones */
    for (chain = chains.first; chain; chain = chain->next) {
      EditBone *ebo, *child = NULL, *parent = NULL;

      /* loop over bones in chain */
      for (ebo = chain->data; ebo; ebo = parent) {
        /* parent is this bone's original parent
         * - we store this, as the next bone that is checked is this one
         *   but the value of ebo->parent may change here...
         */
        parent = ebo->parent;

        /* skip bone if already handled... [#34123] */
        if ((ebo->flag & BONE_TRANSFORM) == 0) {
          /* only if selected and editable */
          if (EBONE_VISIBLE(arm, ebo) && EBONE_EDITABLE(ebo)) {
            /* swap head and tail coordinates */
            swap_v3_v3(ebo->head, ebo->tail);

            /* do parent swapping:
             * - use 'child' as new parent
             * - connected flag is only set if points are coincidental
             */
            ebo->parent = child;
            if ((child) && equals_v3v3(ebo->head, child->tail)) {
              ebo->flag |= BONE_CONNECTED;
            }
            else {
              ebo->flag &= ~BONE_CONNECTED;
            }

            /* get next bones
             * - child will become the new parent of next bone
             */
            child = ebo;
          }
          else {
            /* not swapping this bone, however, if its 'parent' got swapped, unparent us from it
             * as it will be facing in opposite direction
             */
            if ((parent) && (EBONE_VISIBLE(arm, parent) && EBONE_EDITABLE(parent))) {
              ebo->parent = NULL;
              ebo->flag &= ~BONE_CONNECTED;
            }

            /* get next bones
             * - child will become new parent of next bone (not swapping occurred,
             *   so set to NULL to prevent infinite-loop)
             */
            child = NULL;
          }

          /* tag as done (to prevent double-swaps) */
          ebo->flag |= BONE_TRANSFORM;
        }
      }
    }

    /* free chains */
    BLI_freelistN(&chains);

    /* clear temp flags */
    armature_clear_swap_done_flags(arm);
    armature_tag_unselect(arm);

    /* note, notifier might evolve */
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_switch_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Switch Direction";
  ot->idname = "ARMATURE_OT_switch_direction";
  ot->description = "Change the direction that a chain of bones points in (head <-> tail swap)";

  /* api callbacks */
  ot->exec = armature_switch_direction_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Align ******************************* */

/* helper to fix a ebone position if its parent has moved due to alignment*/
static void fix_connected_bone(EditBone *ebone)
{
  float diff[3];

  if (!(ebone->parent) || !(ebone->flag & BONE_CONNECTED) ||
      equals_v3v3(ebone->parent->tail, ebone->head)) {
    return;
  }

  /* if the parent has moved we translate child's head and tail accordingly */
  sub_v3_v3v3(diff, ebone->parent->tail, ebone->head);
  add_v3_v3(ebone->head, diff);
  add_v3_v3(ebone->tail, diff);
}

/* helper to recursively find chains of connected bones starting at ebone and fix their position */
static void fix_editbone_connected_children(ListBase *edbo, EditBone *ebone)
{
  EditBone *selbone;

  for (selbone = edbo->first; selbone; selbone = selbone->next) {
    if ((selbone->parent) && (selbone->parent == ebone) && (selbone->flag & BONE_CONNECTED)) {
      fix_connected_bone(selbone);
      fix_editbone_connected_children(edbo, selbone);
    }
  }
}

static void bone_align_to_bone(ListBase *edbo, EditBone *selbone, EditBone *actbone)
{
  float selboneaxis[3], actboneaxis[3], length;

  sub_v3_v3v3(actboneaxis, actbone->tail, actbone->head);
  normalize_v3(actboneaxis);

  sub_v3_v3v3(selboneaxis, selbone->tail, selbone->head);
  length = len_v3(selboneaxis);

  mul_v3_fl(actboneaxis, length);
  add_v3_v3v3(selbone->tail, selbone->head, actboneaxis);
  selbone->roll = actbone->roll;

  /* if the bone being aligned has connected descendants they must be moved
   * according to their parent new position, otherwise they would be left
   * in an inconsistent state: connected but away from the parent*/
  fix_editbone_connected_children(edbo, selbone);
}

static int armature_align_bones_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  bArmature *arm = (bArmature *)ob->data;
  EditBone *actbone = CTX_data_active_bone(C);
  EditBone *actmirb = NULL;
  int num_selected_bones;

  /* there must be an active bone */
  if (actbone == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
    return OPERATOR_CANCELLED;
  }
  else if (arm->flag & ARM_MIRROR_EDIT) {
    /* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
     * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone
     *   (i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
     *   This is useful for arm-chains, for example parenting lower arm to upper arm
     * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
     *   then just use actbone. Useful when doing upper arm to spine.
     */
    actmirb = ED_armature_ebone_get_mirrored(arm->edbo, actbone);
    if (actmirb == NULL) {
      actmirb = actbone;
    }
  }

  /* if there is only 1 selected bone, we assume that that is the active bone,
   * since a user will need to have clicked on a bone (thus selecting it) to make it active
   */
  num_selected_bones = CTX_DATA_COUNT(C, selected_editable_bones);
  if (num_selected_bones <= 1) {
    /* When only the active bone is selected, and it has a parent,
     * align it to the parent, as that is the only possible outcome.
     */
    if (actbone->parent) {
      bone_align_to_bone(arm->edbo, actbone, actbone->parent);

      if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent)) {
        bone_align_to_bone(arm->edbo, actmirb, actmirb->parent);
      }

      BKE_reportf(op->reports, RPT_INFO, "Aligned bone '%s' to parent", actbone->name);
    }
  }
  else {
    /* Align 'selected' bones to the active one
     * - the context iterator contains both selected bones and their mirrored copies,
     *   so we assume that unselected bones are mirrored copies of some selected bone
     * - since the active one (and/or its mirror) will also be selected, we also need
     *   to check that we are not trying to operate on them, since such an operation
     *   would cause errors
     */

    /* align selected bones to the active one */
    CTX_DATA_BEGIN (C, EditBone *, ebone, selected_editable_bones) {
      if (ELEM(ebone, actbone, actmirb) == 0) {
        if (ebone->flag & BONE_SELECTED) {
          bone_align_to_bone(arm->edbo, ebone, actbone);
        }
        else {
          bone_align_to_bone(arm->edbo, ebone, actmirb);
        }
      }
    }
    CTX_DATA_END;

    BKE_reportf(
        op->reports, RPT_INFO, "%d bones aligned to bone '%s'", num_selected_bones, actbone->name);
  }

  /* note, notifier might evolve */
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_align(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Align Bones";
  ot->idname = "ARMATURE_OT_align";
  ot->description = "Align selected bones to the active bone (or to their parent)";

  /* api callbacks */
  ot->exec = armature_align_bones_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Split ******************************* */

static int armature_split_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;

    for (EditBone *bone = arm->edbo->first; bone; bone = bone->next) {
      if (bone->parent && (bone->flag & BONE_SELECTED) != (bone->parent->flag & BONE_SELECTED)) {
        bone->parent = NULL;
        bone->flag &= ~BONE_CONNECTED;
      }
    }
    for (EditBone *bone = arm->edbo->first; bone; bone = bone->next) {
      ED_armature_ebone_select_set(bone, (bone->flag & BONE_SELECTED) != 0);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  }

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split";
  ot->idname = "ARMATURE_OT_split";
  ot->description = "Split off selected bones from connected unselected bones";

  /* api callbacks */
  ot->exec = armature_split_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Delete ******************************* */

static bool armature_delete_ebone_cb(const char *bone_name, void *arm_p)
{
  bArmature *arm = arm_p;
  EditBone *ebone;

  ebone = ED_armature_ebone_find_name(arm->edbo, bone_name);
  return (ebone && (ebone->flag & BONE_SELECTED) && (arm->layer & ebone->layer));
}

/* previously delete_armature */
/* only editmode! */
static int armature_delete_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
  EditBone *curBone, *ebone_next;
  bool changed_multi = false;

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;
    bool changed = false;

    armature_select_mirrored(arm);

    BKE_pose_channels_remove(obedit, armature_delete_ebone_cb, arm);

    for (curBone = arm->edbo->first; curBone; curBone = ebone_next) {
      ebone_next = curBone->next;
      if (arm->layer & curBone->layer) {
        if (curBone->flag & BONE_SELECTED) {
          if (curBone == arm->act_edbone) {
            arm->act_edbone = NULL;
          }
          ED_armature_ebone_remove(arm, curBone);
          changed = true;
        }
      }
    }

    if (changed) {
      changed_multi = true;

      ED_armature_edit_sync_selection(arm->edbo);
      ED_armature_edit_refresh_layer_used(arm);
      BKE_pose_tag_recalc(CTX_data_main(C), obedit->pose);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
    }
  }
  MEM_freeN(objects);

  if (!changed_multi) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Selected Bone(s)";
  ot->idname = "ARMATURE_OT_delete";
  ot->description = "Remove selected bones from the armature";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = armature_delete_selected_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool armature_dissolve_ebone_cb(const char *bone_name, void *arm_p)
{
  bArmature *arm = arm_p;
  EditBone *ebone;

  ebone = ED_armature_ebone_find_name(arm->edbo, bone_name);
  return (ebone && (ebone->flag & BONE_DONE));
}

static int armature_dissolve_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone, *ebone_next;
  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;
    bool changed = false;

    /* store for mirror */
    GHash *ebone_flag_orig = NULL;
    int ebone_num = 0;

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      ebone->temp.p = NULL;
      ebone->flag &= ~BONE_DONE;
      ebone_num++;
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      GHashIterator gh_iter;

      ebone_flag_orig = BLI_ghash_ptr_new_ex(__func__, ebone_num);
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        union {
          int flag;
          void *p;
        } val = {0};
        val.flag = ebone->flag;
        BLI_ghash_insert(ebone_flag_orig, ebone, val.p);
      }

      armature_select_mirrored_ex(arm, BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);

      GHASH_ITER (gh_iter, ebone_flag_orig) {
        union {
          int flag;
          void *p;
        } *val_p = (void *)BLI_ghashIterator_getValue_p(&gh_iter);
        ebone = BLI_ghashIterator_getKey(&gh_iter);
        val_p->flag = ebone->flag & ~val_p->flag;
      }
    }

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (ebone->parent && ebone->flag & BONE_CONNECTED) {
        if (ebone->parent->temp.ebone == ebone->parent) {
          /* ignore */
        }
        else if (ebone->parent->temp.ebone) {
          /* set ignored */
          ebone->parent->temp.ebone = ebone->parent;
        }
        else {
          /* set child */
          ebone->parent->temp.ebone = ebone;
        }
      }
    }

    /* cleanup multiple used bones */
    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (ebone->temp.ebone == ebone) {
        ebone->temp.ebone = NULL;
      }
    }

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      /* break connections for unseen bones */
      if (((arm->layer & ebone->layer) &&
           ((ED_armature_ebone_selectflag_get(ebone) & (BONE_TIPSEL | BONE_SELECTED)))) == 0) {
        ebone->temp.ebone = NULL;
      }

      if (((arm->layer & ebone->layer) &&
           ((ED_armature_ebone_selectflag_get(ebone) & (BONE_ROOTSEL | BONE_SELECTED)))) == 0) {
        if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
          ebone->parent->temp.ebone = NULL;
        }
      }
    }

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {

      if (ebone->parent && (ebone->parent->temp.ebone == ebone)) {
        ebone->flag |= BONE_DONE;
      }
    }

    BKE_pose_channels_remove(obedit, armature_dissolve_ebone_cb, arm);

    for (ebone = arm->edbo->first; ebone; ebone = ebone_next) {
      ebone_next = ebone->next;

      if (ebone->flag & BONE_DONE) {
        copy_v3_v3(ebone->parent->tail, ebone->tail);
        ebone->parent->rad_tail = ebone->rad_tail;
        SET_FLAG_FROM_TEST(ebone->parent->flag, ebone->flag & BONE_TIPSEL, BONE_TIPSEL);

        ED_armature_ebone_remove_ex(arm, ebone, false);
        changed = true;
      }
    }

    if (changed) {
      for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
        if (ebone->parent && ebone->parent->temp.ebone && (ebone->flag & BONE_CONNECTED)) {
          ebone->rad_head = ebone->parent->rad_tail;
        }
      }

      if (arm->flag & ARM_MIRROR_EDIT) {
        for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
          union {
            int flag;
            void *p;
          } *val_p = (void *)BLI_ghash_lookup_p(ebone_flag_orig, ebone);
          if (val_p && val_p->flag) {
            ebone->flag &= ~val_p->flag;
          }
        }
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      BLI_ghash_free(ebone_flag_orig, NULL, NULL);
    }

    if (changed) {
      changed_multi = true;
      ED_armature_edit_sync_selection(arm->edbo);
      ED_armature_edit_refresh_layer_used(arm);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
    }
  }
  MEM_freeN(objects);

  if (!changed_multi) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_dissolve(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dissolve Selected Bone(s)";
  ot->idname = "ARMATURE_OT_dissolve";
  ot->description = "Dissolve selected bones from the armature";

  /* api callbacks */
  ot->exec = armature_dissolve_selected_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Show/Hide ******************************* */

static int armature_hide_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int invert = RNA_boolean_get(op->ptr, "unselected") ? BONE_SELECTED : 0;

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;
    bool changed = false;

    for (EditBone *ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (EBONE_VISIBLE(arm, ebone)) {
        if ((ebone->flag & BONE_SELECTED) != invert) {
          ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
          ebone->flag |= BONE_HIDDEN_A;
          changed = true;
        }
      }
    }

    if (!changed) {
      continue;
    }
    ED_armature_edit_validate_active(arm);
    ED_armature_edit_sync_selection(arm->edbo);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "ARMATURE_OT_hide";
  ot->description = "Tag selected bones to not be visible in Edit Mode";

  /* api callbacks */
  ot->exec = armature_hide_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

static int armature_reveal_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool select = RNA_boolean_get(op->ptr, "select");
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;
    bool changed = false;

    for (EditBone *ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (arm->layer & ebone->layer) {
        if (ebone->flag & BONE_HIDDEN_A) {
          if (!(ebone->flag & BONE_UNSELECTABLE)) {
            SET_FLAG_FROM_TEST(ebone->flag, select, (BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL));
          }
          ebone->flag &= ~BONE_HIDDEN_A;
          changed = true;
        }
      }
    }

    if (changed) {
      ED_armature_edit_validate_active(arm);
      ED_armature_edit_sync_selection(arm->edbo);

      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
    }
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Hidden";
  ot->idname = "ARMATURE_OT_reveal";
  ot->description = "Reveal all bones hidden in Edit Mode";

  /* api callbacks */
  ot->exec = armature_reveal_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}
