/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Armature EditMode tools - transforms, chain based editing, and other settings.
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_icons.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "ANIM_armature.hh"

#include "DEG_depsgraph.hh"

#include "armature_intern.hh"

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Object Tools Public API
 * \{ */

bArmature *ED_armature_context(const bContext *C)
{
  bArmature *armature = static_cast<bArmature *>(
      CTX_data_pointer_get_type(C, "armature", &RNA_Armature).data);

  if (armature == nullptr) {
    Object *object = blender::ed::object::context_active_object(C);
    if (object && object->type == OB_ARMATURE) {
      armature = static_cast<bArmature *>(object->data);
    }
  }

  return armature;
}

/* NOTE: these functions are exported to the Object module to be called from the tools there */

void ED_armature_edit_transform(bArmature *arm, const float mat[4][4], const bool do_props)
{
  float scale = mat4_to_scale(mat); /* store the scale of the matrix here to use on envelopes */
  float mat3[3][3];

  copy_m3_m4(mat3, mat);
  normalize_m3(mat3);
  /* Do the rotations */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    float tmat[3][3];

    /* find the current bone's roll matrix */
    ED_armature_ebone_to_mat3(ebone, tmat);

    /* transform the roll matrix */
    mul_m3_m3m3(tmat, mat3, tmat);

    /* transform the bone */
    mul_m4_v3(mat, ebone->head);
    mul_m4_v3(mat, ebone->tail);

    /* apply the transformed roll back */
    mat3_to_vec_roll(tmat, nullptr, &ebone->roll);

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

void ED_armature_transform(bArmature *arm, const float mat[4][4], const bool do_props)
{
  if (arm->edbo) {
    ED_armature_edit_transform(arm, mat, do_props);
  }
  else {
    BKE_armature_transform(arm, mat, do_props);
  }
}

void ED_armature_origin_set(
    Main *bmain, Object *ob, const float cursor[3], int centermode, int around)
{
  const bool is_editmode = BKE_object_is_in_editmode(ob);
  bArmature *arm = static_cast<bArmature *>(ob->data);
  float cent[3];

  /* Put the armature into edit-mode. */
  if (is_editmode == false) {
    ED_armature_to_edit(arm);
  }

  /* Find the center-point. */
  if (centermode == 2) {
    copy_v3_v3(cent, cursor);
    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
    mul_m4_v3(ob->world_to_object().ptr(), cent);
  }
  else {
    if (around == V3D_AROUND_CENTER_BOUNDS) {
      float min[3], max[3];
      INIT_MINMAX(min, max);
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        minmax_v3v3_v3(min, max, ebone->head);
        minmax_v3v3_v3(min, max, ebone->tail);
      }
      mid_v3_v3v3(cent, min, max);
    }
    else { /* #V3D_AROUND_CENTER_MEDIAN. */
      int total = 0;
      zero_v3(cent);
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        total += 2;
        add_v3_v3(cent, ebone->head);
        add_v3_v3(cent, ebone->tail);
      }
      if (total) {
        mul_v3_fl(cent, 1.0f / float(total));
      }
    }
  }

  /* Do the adjustments */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    sub_v3_v3(ebone->head, cent);
    sub_v3_v3(ebone->tail, cent);
  }

  /* Turn the list into an armature */
  if (is_editmode == false) {
    ED_armature_from_edit(bmain, arm);
    ED_armature_edit_free(arm);
  }

  /* Adjust object location for new center-point. */
  if (centermode && (is_editmode == false)) {
    mul_mat3_m4_v3(ob->object_to_world().ptr(), cent); /* omit translation part */
    add_v3_v3(ob->loc, cent);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Roll Calculate Operator
 * \{ */

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
      (fabsf(dot_v3v3(align_axis, nor)) >= (1.0f - FLT_EPSILON)))
  {
    return roll;
  }

  vec_roll_to_mat3_normalized(nor, 0.0f, mat);

  /* project the new_up_axis along the normal */
  project_v3_v3v3_normalized(vec, align_axis, nor);
  sub_v3_v3v3(align_axis_proj, align_axis, vec);

  if (axis_only) {
    if (angle_v3v3(align_axis_proj, mat[2]) > float(M_PI_2)) {
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

/* NOTE: ranges arithmetic is used below. */
enum eCalcRollTypes {
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
};

static const EnumPropertyItem prop_calc_roll_types[] = {
    RNA_ENUM_ITEM_HEADING(N_("Positive"), nullptr),
    {CALC_ROLL_TAN_POS_X, "POS_X", 0, "Local +X Tangent", ""},
    {CALC_ROLL_TAN_POS_Z, "POS_Z", 0, "Local +Z Tangent", ""},

    {CALC_ROLL_POS_X, "GLOBAL_POS_X", 0, "Global +X Axis", ""},
    {CALC_ROLL_POS_Y, "GLOBAL_POS_Y", 0, "Global +Y Axis", ""},
    {CALC_ROLL_POS_Z, "GLOBAL_POS_Z", 0, "Global +Z Axis", ""},

    RNA_ENUM_ITEM_HEADING(N_("Negative"), nullptr),
    {CALC_ROLL_TAN_NEG_X, "NEG_X", 0, "Local -X Tangent", ""},
    {CALC_ROLL_TAN_NEG_Z, "NEG_Z", 0, "Local -Z Tangent", ""},

    {CALC_ROLL_NEG_X, "GLOBAL_NEG_X", 0, "Global -X Axis", ""},
    {CALC_ROLL_NEG_Y, "GLOBAL_NEG_Y", 0, "Global -Y Axis", ""},
    {CALC_ROLL_NEG_Z, "GLOBAL_NEG_Z", 0, "Global -Z Axis", ""},

    RNA_ENUM_ITEM_HEADING(N_("Other"), nullptr),
    {CALC_ROLL_ACTIVE, "ACTIVE", 0, "Active Bone", ""},
    {CALC_ROLL_VIEW, "VIEW", 0, "View Axis", ""},
    {CALC_ROLL_CURSOR, "CURSOR", 0, "Cursor", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus armature_calc_roll_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_edit_object(C);

  eCalcRollTypes type = eCalcRollTypes(RNA_enum_get(op->ptr, "type"));
  const bool axis_only = RNA_boolean_get(op->ptr, "axis_only");
  /* axis_flip when matching the active bone never makes sense */
  bool axis_flip = ((type >= CALC_ROLL_ACTIVE)    ? RNA_boolean_get(op->ptr, "axis_flip") :
                    (type >= CALC_ROLL_TAN_NEG_X) ? true :
                                                    false);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    float imat[3][3];
    EditBone *ebone;

    if ((type >= CALC_ROLL_NEG_X) && (type <= CALC_ROLL_TAN_NEG_Z)) {
      type = eCalcRollTypes(int(type) - (CALC_ROLL_ACTIVE - CALC_ROLL_NEG_X));
      axis_flip = true;
    }

    copy_m3_m4(imat, ob->object_to_world().ptr());
    invert_m3(imat);

    if (type == CALC_ROLL_CURSOR) { /* Cursor */
      float cursor_local[3];
      const View3DCursor *cursor = &scene->cursor;

      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      copy_v3_v3(cursor_local, cursor->location);
      mul_m4_v3(ob->world_to_object().ptr(), cursor_local);

      /* cursor */
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if (blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone)) {
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
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if (ebone->parent) {
          bool is_edit = (blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone));
          bool is_edit_parent = (blender::animrig::bone_is_visible(arm, ebone->parent) &&
                                 EBONE_EDITABLE(ebone->parent));

          if (is_edit || is_edit_parent) {
            EditBone *ebone_other = ebone->parent;
            float dir_a[3];
            float dir_b[3];
            float vec[3];
            bool is_vec_zero;

            sub_v3_v3v3(dir_a, ebone->tail, ebone->head);
            normalize_v3(dir_a);

            /* find the first bone in the chain with a different direction */
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
                if (ebone->parent->parent == nullptr) {
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
        if (rv3d == nullptr) {
          BKE_report(op->reports, RPT_ERROR, "No region view3d available");
          return OPERATOR_CANCELLED;
        }

        copy_v3_v3(vec, rv3d->viewinv[2]);
        mul_m3_v3(imat, vec);
      }
      else if (type == CALC_ROLL_ACTIVE) {
        float mat[3][3];
        bArmature *arm_active = static_cast<bArmature *>(ob_active->data);
        ebone = (EditBone *)arm_active->act_edbone;
        if (ebone == nullptr) {
          BKE_report(op->reports, RPT_ERROR, "No active bone set");
          return OPERATOR_CANCELLED;
        }

        ED_armature_ebone_to_mat3(ebone, mat);
        copy_v3_v3(vec, mat[2]);
      }
      else if (type < 6) { /* NOTE: always true, check to quiet GCC12.2 `-Warray-bounds`. */
        /* Axis */
        if (type < 3) {
          vec[type] = 1.0f;
        }
        else {
          vec[type - 2] = -1.0f;
        }
        mul_m3_v3(imat, vec);
        normalize_v3(vec);
      }
      else {
        /* The previous block should handle all remaining cases. */
        BLI_assert_unreachable();
      }

      if (axis_flip) {
        negate_v3(vec);
      }

      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if (blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone)) {
          /* roll func is a callback which assumes that all is well */
          ebone->roll = ED_armature_ebone_roll_to_vector(ebone, vec, axis_only);
          changed = true;
        }
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if ((blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone)) == 0) {
          EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if (ebone_mirr &&
              (blender::animrig::bone_is_visible(arm, ebone_mirr) && EBONE_EDITABLE(ebone_mirr)))
          {
            ebone->roll = -ebone_mirr->roll;
          }
        }
      }
    }

    if (changed) {
      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    }
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_calculate_roll(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Recalculate Roll";
  ot->idname = "ARMATURE_OT_calculate_roll";
  ot->description = "Automatically fix alignment of select bones' axes";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = armature_calc_roll_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_calc_roll_types, CALC_ROLL_TAN_POS_X, "Type", "");
  RNA_def_boolean(ot->srna, "axis_flip", false, "Flip Axis", "Negate the alignment axis");
  RNA_def_boolean(ot->srna,
                  "axis_only",
                  false,
                  "Shortest Rotation",
                  "Ignore the axis direction, use the shortest rotation to align");
}

static wmOperatorStatus armature_roll_clear_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const float roll = RNA_float_get(op->ptr, "roll");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone)) {
        /* Roll func is a callback which assumes that all is well. */
        ebone->roll = roll;
        changed = true;
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if ((blender::animrig::bone_is_visible(arm, ebone) && EBONE_EDITABLE(ebone)) == 0) {
          EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if (ebone_mirr &&
              (blender::animrig::bone_is_visible(arm, ebone_mirr) && EBONE_EDITABLE(ebone_mirr)))
          {
            ebone->roll = -ebone_mirr->roll;
            changed = true;
          }
        }
      }
    }

    if (changed) {
      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    }
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_roll_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Roll";
  ot->idname = "ARMATURE_OT_roll_clear";
  ot->description = "Clear roll for selected bones";

  /* API callbacks. */
  ot->exec = armature_roll_clear_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_rotation(ot->srna,
                         "roll",
                         0,
                         nullptr,
                         DEG2RADF(-360.0f),
                         DEG2RADF(360.0f),
                         "Roll",
                         "",
                         DEG2RADF(-360.0f),
                         DEG2RADF(360.0f));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Chain-Based Tool Utilities
 * \{ */

/* temporary data-structure for merge/fill bones */
struct EditBonePoint {
  EditBonePoint *next, *prev;

  EditBone *head_owner; /* EditBone which uses this point as a 'head' point */
  EditBone *tail_owner; /* EditBone which uses this point as a 'tail' point */

  float vec[3]; /* the actual location of the point in local/EditMode space */
};

/* find chain-tips (i.e. bones without children) */
static void chains_find_tips(ListBase *edbo, ListBase *list)
{
  EditBone *ebo;
  LinkData *ld;

  /* NOTE: this is potentially very slow ... there's got to be a better way. */
  LISTBASE_FOREACH (EditBone *, curBone, edbo) {
    short stop = 0;

    /* is this bone contained within any existing chain? (skip if so) */
    LISTBASE_FOREACH (LinkData *, ld, list) {
      for (ebo = static_cast<EditBone *>(ld->data); ebo; ebo = ebo->parent) {
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
      LISTBASE_FOREACH (LinkData *, ld, list) {
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
    ld = MEM_callocN<LinkData>("BoneChain");
    ld->data = curBone;
    BLI_addtail(list, ld);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fill Operator
 * \{ */

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

  LISTBASE_FOREACH (EditBonePoint *, ebp, points) {
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
    ebp = MEM_callocN<EditBonePoint>("EditBonePoint");

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
static wmOperatorStatus armature_fill_bones_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ListBase points = {nullptr, nullptr};
  EditBone *newbone = nullptr;
  int count;
  bool mixed_object_error = false;

  /* loop over all bones, and only consider if visible */
  bArmature *arm = nullptr;
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

  if (mixed_object_error) {
    BKE_report(op->reports, RPT_ERROR, "Bones for different objects selected");
    BLI_freelistN(&points);
    return OPERATOR_CANCELLED;
  }

  Object *obedit = nullptr;
  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    FOREACH_OBJECT_IN_EDIT_MODE_BEGIN (scene, view_layer, v3d, ob_iter) {
      if (ob_iter->data == arm) {
        obedit = ob_iter;
      }
    }
    FOREACH_OBJECT_IN_EDIT_MODE_END;
  }
  BLI_assert(obedit != nullptr);

  if (count == 1) {
    EditBonePoint *ebp;
    float curs[3];

    /* Get Points - selected joint */
    ebp = static_cast<EditBonePoint *>(points.first);

    /* Get points - cursor (tail) */
    invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());
    mul_v3_m4v3(curs, obedit->world_to_object().ptr(), scene->cursor.location);

    /* Create a bone */
    newbone = add_points_bone(obedit, ebp->vec, curs);
  }
  else if (count == 2) {
    EditBonePoint *ebp_a, *ebp_b;
    float head[3], tail[3];
    short headtail = 0;

    /* check that the points don't belong to the same bone */
    ebp_a = static_cast<EditBonePoint *>(points.first);
    ebp_b = ebp_a->next;

    if (((ebp_a->head_owner == ebp_b->tail_owner) && (ebp_a->head_owner != nullptr)) ||
        ((ebp_a->tail_owner == ebp_b->head_owner) && (ebp_a->tail_owner != nullptr)))
    {
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
        invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());
        mul_v3_m4v3(curs, obedit->world_to_object().ptr(), scene->cursor.location);

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
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);
  DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);

  /* free points */
  BLI_freelistN(&points);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_fill(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Fill Between Joints";
  ot->idname = "ARMATURE_OT_fill";
  ot->description = "Add bone between selected joint(s) and/or 3D cursor";

  /* callbacks */
  ot->exec = armature_fill_bones_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Switch Direction Operator
 *
 * Currently, this does not use context loops, as context loops do not make it
 * easy to retrieve any hierarchical/chain relationships which are necessary for
 * this to be done easily.
 * \{ */

/* helper to clear BONE_TRANSFORM flags */
static void armature_clear_swap_done_flags(bArmature *arm)
{
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    ebone->flag &= ~BONE_TRANSFORM;
  }
}

static wmOperatorStatus armature_switch_direction_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    ListBase chains = {nullptr, nullptr};

    /* get chains of bones (ends on chains) */
    chains_find_tips(arm->edbo, &chains);
    if (BLI_listbase_is_empty(&chains)) {
      continue;
    }

    /* ensure that mirror bones will also be operated on */
    armature_tag_select_mirrored(arm);

    /* Clear BONE_TRANSFORM flags
     * - Used to prevent duplicate/canceling operations from occurring #34123.
     * - #BONE_DONE cannot be used here as that's already used for mirroring.
     */
    armature_clear_swap_done_flags(arm);

    /* loop over chains, only considering selected and visible bones */
    LISTBASE_FOREACH (LinkData *, chain, &chains) {
      EditBone *ebo, *child = nullptr, *parent = nullptr;

      /* loop over bones in chain */
      for (ebo = static_cast<EditBone *>(chain->data); ebo; ebo = parent) {
        /* parent is this bone's original parent
         * - we store this, as the next bone that is checked is this one
         *   but the value of ebo->parent may change here...
         */
        parent = ebo->parent;

        /* skip bone if already handled, see #34123. */
        if ((ebo->flag & BONE_TRANSFORM) == 0) {
          /* only if selected and editable */
          if (blender::animrig::bone_is_visible(arm, ebo) && EBONE_EDITABLE(ebo)) {
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
            if ((parent) &&
                (blender::animrig::bone_is_visible(arm, parent) && EBONE_EDITABLE(parent)))
            {
              ebo->parent = nullptr;
              ebo->flag &= ~BONE_CONNECTED;
            }

            /* get next bones
             * - child will become new parent of next bone (not swapping occurred,
             *   so set to nullptr to prevent infinite-loop)
             */
            child = nullptr;
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

    /* NOTE: notifier might evolve. */
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_switch_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Switch Direction";
  ot->idname = "ARMATURE_OT_switch_direction";
  ot->description = "Change the direction that a chain of bones points in (head and tail swap)";

  /* API callbacks. */
  ot->exec = armature_switch_direction_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Align Operator
 * \{ */

/* Helper to fix a ebone position if its parent has moved due to alignment. */
static void fix_connected_bone(EditBone *ebone)
{
  float diff[3];

  if (!(ebone->parent) || !(ebone->flag & BONE_CONNECTED) ||
      equals_v3v3(ebone->parent->tail, ebone->head))
  {
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
  LISTBASE_FOREACH (EditBone *, selbone, edbo) {
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

  /* If the bone being aligned has connected descendants they must be moved
   * according to their parent new position, otherwise they would be left
   * in an inconsistent state: connected but away from the parent. */
  fix_editbone_connected_children(edbo, selbone);
}

static wmOperatorStatus armature_align_bones_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(ob->data);
  EditBone *actbone = CTX_data_active_bone(C);
  EditBone *actmirb = nullptr;
  int num_selected_bones;

  /* there must be an active bone */
  if (actbone == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
    return OPERATOR_CANCELLED;
  }

  if (arm->flag & ARM_MIRROR_EDIT) {
    /* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
     * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone
     *   (i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
     *   This is useful for arm-chains, for example parenting lower arm to upper arm
     * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
     *   then just use actbone. Useful when doing upper arm to spine.
     */
    actmirb = ED_armature_ebone_get_mirrored(arm->edbo, actbone);
    if (actmirb == nullptr) {
      actmirb = actbone;
    }
  }

  /* if there is only 1 selected bone, we assume that it is the active bone,
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
        else if ((arm->flag & ARM_MIRROR_EDIT)) {
          /* Need to check for the mirror mode, because when editing multiple armatures with
           * differing mirror settings `actmirb` can be a nullptr. See #146242. */
          bone_align_to_bone(arm->edbo, ebone, actmirb);
        }
      }
    }
    CTX_DATA_END;

    BKE_reportf(
        op->reports, RPT_INFO, "%d bones aligned to bone '%s'", num_selected_bones, actbone->name);
  }

  /* NOTE: notifier might evolve. */
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_align(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Align Bones";
  ot->idname = "ARMATURE_OT_align";
  ot->description = "Align selected bones to the active bone (or to their parent)";

  /* API callbacks. */
  ot->exec = armature_align_bones_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Operator
 * \{ */

static wmOperatorStatus armature_split_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    LISTBASE_FOREACH (EditBone *, bone, arm->edbo) {
      if (bone->parent && (bone->flag & BONE_SELECTED) != (bone->parent->flag & BONE_SELECTED)) {
        bone->parent = nullptr;
        bone->flag &= ~BONE_CONNECTED;
      }
    }
    LISTBASE_FOREACH (EditBone *, bone, arm->edbo) {
      ED_armature_ebone_select_set(bone, (bone->flag & BONE_SELECTED) != 0);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split";
  ot->idname = "ARMATURE_OT_split";
  ot->description = "Split off selected bones from connected unselected bones";

  /* API callbacks. */
  ot->exec = armature_split_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static bool armature_delete_ebone_cb(const char *bone_name, void *arm_p)
{
  bArmature *arm = static_cast<bArmature *>(arm_p);
  EditBone *ebone;

  ebone = ED_armature_ebone_find_name(arm->edbo, bone_name);
  return (ebone && blender::animrig::bone_is_selected(arm, ebone));
}

/* previously delete_armature */
/* only editmode! */
static wmOperatorStatus armature_delete_selected_exec(bContext *C, wmOperator * /*op*/)
{
  EditBone *curBone, *ebone_next;
  bool changed_multi = false;

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    bool changed = false;

    armature_select_mirrored(arm);

    BKE_pose_channels_remove(obedit, armature_delete_ebone_cb, arm);

    for (curBone = static_cast<EditBone *>(arm->edbo->first); curBone; curBone = ebone_next) {
      ebone_next = curBone->next;
      if (blender::animrig::bone_is_selected(arm, curBone)) {
        if (curBone == arm->act_edbone) {
          arm->act_edbone = nullptr;
        }
        ED_armature_ebone_remove(arm, curBone);
        changed = true;
      }
    }

    if (changed) {
      changed_multi = true;

      ED_armature_edit_sync_selection(arm->edbo);
      BKE_pose_tag_recalc(CTX_data_main(C), obedit->pose);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
      ED_outliner_select_sync_from_edit_bone_tag(C);
    }
  }

  if (!changed_multi) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus armature_delete_selected_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete selected bones?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return armature_delete_selected_exec(C, op);
}

void ARMATURE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Selected Bone(s)";
  ot->idname = "ARMATURE_OT_delete";
  ot->description = "Remove selected bones from the armature";

  /* API callbacks. */
  ot->invoke = armature_delete_selected_invoke;
  ot->exec = armature_delete_selected_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

static bool armature_dissolve_ebone_cb(const char *bone_name, void *arm_p)
{
  bArmature *arm = static_cast<bArmature *>(arm_p);
  EditBone *ebone;

  ebone = ED_armature_ebone_find_name(arm->edbo, bone_name);
  return (ebone && (ebone->flag & BONE_DONE));
}

static wmOperatorStatus armature_dissolve_selected_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone, *ebone_next;
  bool changed_multi = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    bool changed = false;

    /* store for mirror */
    GHash *ebone_flag_orig = nullptr;
    int ebone_num = 0;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      ebone->temp.p = nullptr;
      ebone->flag &= ~BONE_DONE;
      ebone_num++;
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      GHashIterator gh_iter;

      ebone_flag_orig = BLI_ghash_ptr_new_ex(__func__, ebone_num);
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        union {
          int flag;
          void *p;
        } val = {0};
        val.flag = ebone->flag;
        BLI_ghash_insert(ebone_flag_orig, ebone, val.p);
      }

      armature_select_mirrored_ex(arm, BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);

      GHASH_ITER (gh_iter, ebone_flag_orig) {
        union Value {
          int flag;
          void *p;
        } *val_p = (Value *)BLI_ghashIterator_getValue_p(&gh_iter);
        ebone = static_cast<EditBone *>(BLI_ghashIterator_getKey(&gh_iter));
        val_p->flag = ebone->flag & ~val_p->flag;
      }
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
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
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (ebone->temp.ebone == ebone) {
        ebone->temp.ebone = nullptr;
      }
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      /* break connections for unseen bones */
      if ((blender::animrig::bone_is_visible(arm, ebone) &&
           (ED_armature_ebone_selectflag_get(ebone) & (BONE_TIPSEL | BONE_SELECTED))) == 0)
      {
        ebone->temp.ebone = nullptr;
      }

      if ((blender::animrig::bone_is_visible(arm, ebone) &&
           (ED_armature_ebone_selectflag_get(ebone) & (BONE_ROOTSEL | BONE_SELECTED))) == 0)
      {
        if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
          ebone->parent->temp.ebone = nullptr;
        }
      }
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {

      if (ebone->parent && (ebone->parent->temp.ebone == ebone)) {
        ebone->flag |= BONE_DONE;
      }
    }

    BKE_pose_channels_remove(obedit, armature_dissolve_ebone_cb, arm);

    for (ebone = static_cast<EditBone *>(arm->edbo->first); ebone; ebone = ebone_next) {
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
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if (ebone->parent && ebone->parent->temp.ebone && (ebone->flag & BONE_CONNECTED)) {
          ebone->rad_head = ebone->parent->rad_tail;
        }
      }

      if (arm->flag & ARM_MIRROR_EDIT) {
        LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
          union Value {
            int flag;
            void *p;
          } *val_p = (Value *)BLI_ghash_lookup_p(ebone_flag_orig, ebone);
          if (val_p && val_p->flag) {
            ebone->flag &= ~val_p->flag;
          }
        }
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      BLI_ghash_free(ebone_flag_orig, nullptr, nullptr);
    }

    if (changed) {
      changed_multi = true;
      ED_armature_edit_sync_selection(arm->edbo);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
      ED_outliner_select_sync_from_edit_bone_tag(C);
    }
  }

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

  /* API callbacks. */
  ot->exec = armature_dissolve_selected_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

static wmOperatorStatus armature_hide_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int invert = RNA_boolean_get(op->ptr, "unselected") ? BONE_SELECTED : 0;

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (blender::animrig::bone_is_visible(arm, ebone)) {
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
    ED_armature_edit_sync_selection(arm->edbo);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  }
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "ARMATURE_OT_hide";
  ot->description = "Tag selected bones to not be visible in Edit Mode";

  /* API callbacks. */
  ot->exec = armature_hide_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reveal Operator
 * \{ */

static wmOperatorStatus armature_reveal_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool select = RNA_boolean_get(op->ptr, "select");
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (ANIM_bonecoll_is_visible_editbone(arm, ebone)) {
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
      ED_armature_edit_sync_selection(arm->edbo);

      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    }
  }
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Hidden";
  ot->idname = "ARMATURE_OT_reveal";
  ot->description = "Reveal all bones hidden in Edit Mode";

  /* API callbacks. */
  ot->exec = armature_reveal_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/** \} */
