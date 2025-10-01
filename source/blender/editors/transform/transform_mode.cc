/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BKE_constraint.h"
#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_gizmo.hh"
#include "transform_orientations.hh"
#include "transform_snap.hh"

/* Own include. */
#include "transform_mode.hh"

namespace blender::ed::transform {

eTfmMode transform_mode_really_used(bContext *C, eTfmMode mode)
{
  if (mode == TFM_BONESIZE) {
    Object *ob = CTX_data_active_object(C);
    BLI_assert(ob);
    if (ob->type != OB_ARMATURE) {
      return TFM_RESIZE;
    }
    bArmature *arm = static_cast<bArmature *>(ob->data);
    if (arm->drawtype == ARM_DRAW_TYPE_ENVELOPE) {
      return TFM_BONE_ENVELOPE_DIST;
    }
  }

  return mode;
}

bool transdata_check_local_center(const TransInfo *t, short around)
{
  return ((around == V3D_AROUND_LOCAL_ORIGINS) &&
          ((t->options & (CTX_OBJECT | CTX_POSE_BONE)) ||
           /* Implicit: `(t->flag & T_EDIT)`. */
           ELEM(t->obedit_type,
                OB_MESH,
                OB_CURVES_LEGACY,
                OB_CURVES,
                OB_GREASE_PENCIL,
                OB_MBALL,
                OB_ARMATURE) ||
           (t->spacetype == SPACE_GRAPH) ||
           (t->options & (CTX_MOVIECLIP | CTX_MASK | CTX_PAINT_CURVE | CTX_SEQUENCER_IMAGE))));
}

bool transform_mode_is_changeable(const int mode)
{
  return ELEM(mode,
              TFM_ROTATION,
              TFM_RESIZE,
              TFM_TRACKBALL,
              TFM_TRANSLATION,
              TFM_EDGE_SLIDE,
              TFM_VERT_SLIDE,
              TFM_NORMAL_ROTATION);
}

bool transform_mode_affect_only_locations(const TransInfo *t)
{
  return (t->flag & T_V3D_ALIGN) && (t->options & CTX_OBJECT) &&
         (t->settings->transform_pivot_point != V3D_AROUND_CURSOR) && t->context &&
         (CTX_DATA_COUNT(t->context, selected_editable_objects) == 1);
}

/* -------------------------------------------------------------------- */
/** \name Transform Locks
 * \{ */

void protectedTransBits(short protectflag, float vec[3])
{
  if (protectflag & OB_LOCK_LOCX) {
    vec[0] = 0.0f;
  }
  if (protectflag & OB_LOCK_LOCY) {
    vec[1] = 0.0f;
  }
  if (protectflag & OB_LOCK_LOCZ) {
    vec[2] = 0.0f;
  }
}

/* This function only does the delta rotation. */
static void protectedQuaternionBits(short protectflag, float quat[4], const float oldquat[4])
{
  /* Check that protection flags are set. */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* Quaternions getting limited as 4D entities that they are. */
    if (protectflag & OB_LOCK_ROTW) {
      quat[0] = oldquat[0];
    }
    if (protectflag & OB_LOCK_ROTX) {
      quat[1] = oldquat[1];
    }
    if (protectflag & OB_LOCK_ROTY) {
      quat[2] = oldquat[2];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      quat[3] = oldquat[3];
    }
  }
  else {
    /* Quaternions get limited with euler... (compatibility mode). */
    float eul[3], oldeul[3], nquat[4], noldquat[4];
    float qlen;

    qlen = normalize_qt_qt(nquat, quat);
    normalize_qt_qt(noldquat, oldquat);

    quat_to_eul(eul, nquat);
    quat_to_eul(oldeul, noldquat);

    if (protectflag & OB_LOCK_ROTX) {
      eul[0] = oldeul[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      eul[1] = oldeul[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      eul[2] = oldeul[2];
    }

    eul_to_quat(quat, eul);

    /* Restore original quat size. */
    mul_qt_fl(quat, qlen);

    /* Quaternions flip w sign to accumulate rotations correctly. */
    if ((nquat[0] < 0.0f && quat[0] > 0.0f) || (nquat[0] > 0.0f && quat[0] < 0.0f)) {
      mul_qt_fl(quat, -1.0f);
    }
  }
}

static void protectedRotateBits(short protectflag, float eul[3], const float oldeul[3])
{
  if (protectflag & OB_LOCK_ROTX) {
    eul[0] = oldeul[0];
  }
  if (protectflag & OB_LOCK_ROTY) {
    eul[1] = oldeul[1];
  }
  if (protectflag & OB_LOCK_ROTZ) {
    eul[2] = oldeul[2];
  }
}

/**
 * This function only does the delta rotation.
 * Axis-angle is usually internally stored as quaternions.
 */
static void protectedAxisAngleBits(
    short protectflag, float axis[3], float *angle, const float oldAxis[3], float oldAngle)
{
  /* Check that protection flags are set. */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* Axis-angle getting limited as 4D entities that they are... */
    if (protectflag & OB_LOCK_ROTW) {
      *angle = oldAngle;
    }
    if (protectflag & OB_LOCK_ROTX) {
      axis[0] = oldAxis[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      axis[1] = oldAxis[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      axis[2] = oldAxis[2];
    }
  }
  else {
    /* Axis-angle get limited with euler. */
    float eul[3], oldeul[3];

    axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, axis, *angle);
    axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, oldAxis, oldAngle);

    if (protectflag & OB_LOCK_ROTX) {
      eul[0] = oldeul[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      eul[1] = oldeul[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      eul[2] = oldeul[2];
    }

    eulO_to_axis_angle(axis, angle, eul, EULER_ORDER_DEFAULT);

    /* When converting to axis-angle,
     * we need a special exception for the case when there is no axis. */
    if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
      /* For now, rotate around y-axis then (so that it simply becomes the roll). */
      axis[1] = 1.0f;
    }
  }
}

void protectedScaleBits(short protectflag, float scale[3])
{
  if (protectflag & OB_LOCK_SCALEX) {
    scale[0] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEY) {
    scale[1] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEZ) {
    scale[2] = 1.0f;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Limits
 * \{ */

void constraintTransLim(const TransInfo *t, const TransDataContainer *tc, TransData *td)
{
  if (td->con) {
    const bConstraintTypeInfo *ctiLoc = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_LOCLIMIT);
    const bConstraintTypeInfo *ctiDist = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_DISTLIMIT);

    bConstraintOb cob = {nullptr};
    bConstraint *con;
    float ctime = float(t->scene->r.cfra);

    /* Make a temporary bConstraintOb for using these limit constraints
     * - They only care that cob->matrix is correctly set ;-).
     * - Current space should be local.
     */
    unit_m4(cob.matrix);
    copy_v3_v3(cob.matrix[3], td->loc);

    /* Evaluate valid constraints. */
    for (con = td->con; con; con = con->next) {
      const bConstraintTypeInfo *cti = nullptr;
      ListBase targets = {nullptr, nullptr};

      /* Only consider constraint if enabled. */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* Only use it if it's tagged for this purpose (and the right type). */
      if (con->type == CONSTRAINT_TYPE_LOCLIMIT) {
        bLocLimitConstraint *data = (bLocLimitConstraint *)con->data;

        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }
        cti = ctiLoc;
      }
      else if (con->type == CONSTRAINT_TYPE_DISTLIMIT) {
        bDistLimitConstraint *data = (bDistLimitConstraint *)con->data;

        if ((data->flag & LIMITDIST_TRANSFORM) == 0) {
          continue;
        }
        cti = ctiDist;
      }

      if (cti) {
        /* Do space conversions. */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          mul_m3_v3(td->mtx, cob.matrix[3]);
          if (tc->use_local_mat) {
            add_v3_v3(cob.matrix[3], tc->mat[3]);
          }
        }
        else if (con->ownspace == CONSTRAINT_SPACE_POSE) {
          /* Bone space without considering object transformations. */
          mul_m3_v3(td->mtx, cob.matrix[3]);
          mul_m3_v3(tc->imat3, cob.matrix[3]);
        }
        else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
          /* Skip... incompatible spacetype. */
          continue;
        }

        /* Initialize the custom space for use in calculating the matrices. */
        BKE_constraint_custom_object_space_init(&cob, con);

        /* Get constraint targets if needed. */
        BKE_constraint_targets_for_solving_get(t->depsgraph, con, &cob, &targets, ctime);

        /* Do constraint. */
        cti->evaluate_constraint(con, &cob, &targets);

        /* Convert spaces again. */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          if (tc->use_local_mat) {
            sub_v3_v3(cob.matrix[3], tc->mat[3]);
          }
          mul_m3_v3(td->smtx, cob.matrix[3]);
        }
        else if (con->ownspace == CONSTRAINT_SPACE_POSE) {
          mul_m3_v3(tc->mat3, cob.matrix[3]);
          mul_m3_v3(td->smtx, cob.matrix[3]);
        }

        /* Free targets list. */
        BLI_freelistN(&targets);
      }
    }

    /* Copy results from `cob->matrix`. */
    copy_v3_v3(td->loc, cob.matrix[3]);
  }
}

static void constraintob_from_transdata(bConstraintOb *cob, TransDataExtension *td_ext)
{
  /* Make a temporary bConstraintOb for use by limit constraints
   * - they only care that cob->matrix is correctly set ;-)
   * - current space should be local
   */
  memset(cob, 0, sizeof(bConstraintOb));
  if (!td_ext) {
    return;
  }
  if (td_ext->rotOrder == ROT_MODE_QUAT) {
    /* Quaternion. */
    /* Objects and bones do normalization first too, otherwise
     * we don't necessarily end up with a rotation matrix, and
     * then conversion back to quat gives a different result. */
    float quat[4];
    normalize_qt_qt(quat, td_ext->quat);
    quat_to_mat4(cob->matrix, quat);
  }
  else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
    /* Axis angle. */
    axis_angle_to_mat4(cob->matrix, td_ext->rotAxis, *td_ext->rotAngle);
  }
  else {
    /* Eulers. */
    eulO_to_mat4(cob->matrix, td_ext->rot, td_ext->rotOrder);
  }
}

static void constraintRotLim(const TransInfo * /*t*/, TransData *td, TransDataExtension *td_ext)
{
  if (td->con) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_ROTLIMIT);
    bConstraintOb cob;
    bConstraint *con;
    bool do_limit = false;

    /* Evaluate valid constraints. */
    for (con = td->con; con; con = con->next) {
      /* Only consider constraint if enabled. */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* We're only interested in Limit-Rotation constraints. */
      if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
        bRotLimitConstraint *data = (bRotLimitConstraint *)con->data;

        /* Only use it if it's tagged for this purpose. */
        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }

        /* Skip incompatible space-types. */
        if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
          continue;
        }

        /* Only do conversion if necessary, to preserve quaternion and euler rotations. */
        if (do_limit == false) {
          constraintob_from_transdata(&cob, td_ext);
          do_limit = true;
        }

        /* Do space conversions. */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* Just multiply by `td->mtx` (this should be ok). */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }

        /* Do constraint. */
        cti->evaluate_constraint(con, &cob, nullptr);

        /* Convert spaces again. */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* Just multiply by `td->smtx` (this should be ok). */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }
      }
    }

    if (do_limit) {
      /* Copy results from `cob->matrix`. */
      if (td_ext->rotOrder == ROT_MODE_QUAT) {
        /* Quaternion. */
        mat4_to_quat(td_ext->quat, cob.matrix);
      }
      else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* Axis angle. */
        mat4_to_axis_angle(td_ext->rotAxis, td_ext->rotAngle, cob.matrix);
      }
      else {
        /* Eulers. */
        mat4_to_eulO(td_ext->rot, td_ext->rotOrder, cob.matrix);
      }
    }
  }
}

void constraintScaleLim(const TransInfo *t, const TransDataContainer *tc, int td_index)
{
  if (!tc->data_ext) {
    return;
  }

  TransData *td = &tc->data[td_index];
  if (!td->con) {
    return;
  }

  /* Make a temporary bConstraintOb for using these limit constraints
   * - they only care that cob->matrix is correctly set ;-)
   * - current space should be local
   */
  if ((td->flag & TD_SINGLE_SCALE) && !(t->con.mode & CON_APPLY)) {
    /* Scale val and reset the "scale". */
    return; /* TODO: fix this case. */
  }

  /* Reset val if SINGLESIZE but using a constraint. */
  if (td->flag & TD_SINGLE_SCALE) {
    return;
  }

  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_SIZELIMIT);
  bConstraintOb cob = {nullptr};
  bConstraint *con;
  float scale_sign[3], scale_abs[3];
  int i;

  TransDataExtension *td_ext = &tc->data_ext[td_index];

  /* Separate out sign to apply back later. */
  for (i = 0; i < 3; i++) {
    scale_sign[i] = signf(td_ext->scale[i]);
    scale_abs[i] = fabsf(td_ext->scale[i]);
  }

  size_to_mat4(cob.matrix, scale_abs);

  /* Evaluate valid constraints. */
  for (con = td->con; con; con = con->next) {
    /* Only consider constraint if enabled. */
    if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
      continue;
    }
    if (con->enforce == 0.0f) {
      continue;
    }

    /* We're only interested in Limit-Scale constraints. */
    if (con->type == CONSTRAINT_TYPE_SIZELIMIT) {
      bSizeLimitConstraint *data = static_cast<bSizeLimitConstraint *>(con->data);

      /* Only use it if it's tagged for this purpose. */
      if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
        continue;
      }

      /* Do space conversions. */
      if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
        /* Just multiply by `td->mtx` (this should be ok). */
        mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
      }
      else if (con->ownspace == CONSTRAINT_SPACE_POSE) {
        /* Bone space without considering object transformations. */
        mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        mul_m4_m3m4(cob.matrix, tc->imat3, cob.matrix);
      }
      else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
        /* Skip... incompatible `spacetype`. */
        continue;
      }

      /* Do constraint. */
      cti->evaluate_constraint(con, &cob, nullptr);

      /* Convert spaces again. */
      if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
        /* Just multiply by `td->smtx` (this should be ok). */
        mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
      }
      else if (con->ownspace == CONSTRAINT_SPACE_POSE) {
        mul_m4_m3m4(cob.matrix, tc->mat3, cob.matrix);
        mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
      }
    }
  }

  /* Copy results from `cob->matrix`. */
  if ((td->flag & TD_SINGLE_SCALE) && !(t->con.mode & CON_APPLY)) {
    /* Scale val and reset the "scale". */
    return; /* TODO: fix this case. */
  }

  /* Reset val if SINGLESIZE but using a constraint. */
  if (td->flag & TD_SINGLE_SCALE) {
    return;
  }

  /* Extract scale from matrix and apply back sign. */
  mat4_to_size(td_ext->scale, cob.matrix);
  mul_v3_v3(td_ext->scale, scale_sign);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation Utils)
 * \{ */

void headerRotation(TransInfo *t, char *str, const int str_size, float final)
{
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, t->scene->unit);

    ofs += BLI_snprintf_utf8_rlen(
        str + ofs, str_size - ofs, IFACE_("Rotation: %s %s %s"), &c[0], t->con.text, t->proptext);
  }
  else {
    ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                  str_size - ofs,
                                  IFACE_("Rotation: %.2f%s %s"),
                                  RAD2DEGF(final),
                                  t->con.text,
                                  t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_utf8_rlen(
        str + ofs, str_size - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
  }
}

void ElementRotation_ex(const TransInfo *t,
                        const TransDataContainer *tc,
                        TransData *td,
                        TransDataExtension *td_ext,
                        const float mat[3][3],
                        const float *center)
{
  float vec[3], totmat[3][3], smat[3][3];
  float eul[3], fmat[3][3], quat[4];

  if (t->flag & T_POINTS) {
    mul_m3_m3m3(totmat, mat, td->mtx);
    mul_m3_m3m3(smat, td->smtx, totmat);

    /* Apply gpencil falloff. */
    if (t->options & CTX_GPENCIL_STROKES) {
      if (t->obedit_type == OB_GREASE_PENCIL) {
        const float *gp_falloff = static_cast<const float *>(td->extra);
        if (gp_falloff != nullptr && *gp_falloff != 1.0f) {
          float ident_mat[3][3];
          unit_m3(ident_mat);
          interp_m3_m3m3(smat, ident_mat, smat, *gp_falloff);
        }
      }
    }

    sub_v3_v3v3(vec, td->iloc, center);
    mul_m3_v3(smat, vec);

    add_v3_v3v3(td->loc, vec, center);

    sub_v3_v3v3(vec, td->loc, td->iloc);
    protectedTransBits(td->protectflag, vec);
    add_v3_v3v3(td->loc, td->iloc, vec);

    if (td->flag & TD_USEQUAT) {
      mul_m3_series(fmat, td->smtx, mat, td->mtx);
      mat3_to_quat(quat, fmat); /* Actual transform. */

      if (td_ext->quat) {
        mul_qt_qtqt(td_ext->quat, quat, td_ext->iquat);

        /* Is there a reason not to have this here? -jahka. */
        protectedQuaternionBits(td->protectflag, td_ext->quat, td_ext->iquat);
      }
    }
  }
  /**
   * HACK WARNING
   *
   * This is some VERY ugly special case to deal with pose mode.
   *
   * The problem is that mtx and smtx include each bone orientation.
   *
   * That is needed to rotate each bone properly, HOWEVER, to calculate
   * the translation component, we only need the actual armature object's
   * matrix (and inverse). That is not all though. Once the proper translation
   * has been computed, it has to be converted back into the bone's space.
   */
  else if (t->options & CTX_POSE_BONE) {
    /* Extract and invert armature object matrix. */

    if ((td->flag & TD_NO_LOC) == 0) {
      sub_v3_v3v3(vec, td_ext->center_no_override, center);

      mul_m3_v3(tc->mat3, vec);  /* To Global space. */
      mul_m3_v3(mat, vec);       /* Applying rotation. */
      mul_m3_v3(tc->imat3, vec); /* To Local space. */

      add_v3_v3(vec, center);
      /* `vec` now is the location where the object has to be. */

      /* Translation needed from the initial location. */
      sub_v3_v3v3(vec, vec, td_ext->center_no_override);

      /* Special exception, see TD_PBONE_LOCAL_MTX definition comments. */
      if (td->flag & TD_PBONE_LOCAL_MTX_P) {
        /* Do nothing. */
      }
      else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
        mul_m3_v3(tc->mat3, vec);       /* To Global space. */
        mul_m3_v3(td_ext->l_smtx, vec); /* To Pose space (Local Location). */
      }
      else {
        mul_m3_v3(tc->mat3, vec); /* To Global space. */
        mul_m3_v3(td->smtx, vec); /* To Pose space. */
      }

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);

      constraintTransLim(t, tc, td);
    }

    /* Rotation. */
    /* MORE HACK: as in some cases the matrix to apply location and rot/scale is not the same,
     * and ElementRotation() might be called in Translation context (with align snapping),
     * we need to be sure to actually use the *rotation* matrix here...
     * So no other way than storing it in some dedicated members of `td_ext`! */
    if ((t->flag & T_V3D_ALIGN) == 0) { /* Align mode doesn't rotate objects itself. */
      /* Euler or quaternion/axis-angle? */
      if (td_ext->rotOrder == ROT_MODE_QUAT) {
        mul_m3_series(fmat, td_ext->r_smtx, mat, td_ext->r_mtx);

        mat3_to_quat(quat, fmat); /* Actual transform. */

        mul_qt_qtqt(td_ext->quat, quat, td_ext->iquat);
        /* This function works on end result. */
        protectedQuaternionBits(td->protectflag, td_ext->quat, td_ext->iquat);
      }
      else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* Calculate effect based on quaternions. */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td_ext->irotAxis, td_ext->irotAngle);

        mul_m3_series(fmat, td_ext->r_smtx, mat, td_ext->r_mtx);
        mat3_to_quat(quat, fmat); /* Actual transform. */
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td_ext->rotAxis, td_ext->rotAngle, tquat);

        /* This function works on end result. */
        protectedAxisAngleBits(td->protectflag,
                               td_ext->rotAxis,
                               td_ext->rotAngle,
                               td_ext->irotAxis,
                               td_ext->irotAngle);
      }
      else {
        float eulmat[3][3];

        mul_m3_m3m3(totmat, mat, td_ext->r_mtx);
        mul_m3_m3m3(smat, td_ext->r_smtx, totmat);

        /* Calculate the total rotation in eulers. */
        copy_v3_v3(eul, td_ext->irot);
        eulO_to_mat3(eulmat, eul, td_ext->rotOrder);

        /* `mat = transform`, `obmat = bone rotation`. */
        mul_m3_m3m3(fmat, smat, eulmat);

        mat3_to_compatible_eulO(eul, td_ext->rot, td_ext->rotOrder, fmat);

        /* And apply (to end result only). */
        protectedRotateBits(td->protectflag, eul, td_ext->irot);
        copy_v3_v3(td_ext->rot, eul);
      }

      constraintRotLim(t, td, td_ext);
    }
  }
  else {
    if ((td->flag & TD_NO_LOC) == 0) {
      /* Translation. */
      sub_v3_v3v3(vec, td->center, center);
      mul_m3_v3(mat, vec);
      add_v3_v3(vec, center);
      /* `vec` now is the location where the object has to be. */
      sub_v3_v3(vec, td->center);
      mul_m3_v3(td->smtx, vec);

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);
    }

    constraintTransLim(t, tc, td);

    /* Rotation. */
    if ((t->flag & T_V3D_ALIGN) == 0) { /* Align mode doesn't rotate objects itself. */
      /* Euler or quaternion? */
      if ((td_ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
        /* Can be called for texture space translate for example, then opt out. */
        if (td_ext->quat) {
          mul_m3_series(fmat, td->smtx, mat, td->mtx);

          if (!is_zero_v3(td_ext->dquat)) {
            /* Correct for delta quat. */
            float tmp_mat[3][3];
            quat_to_mat3(tmp_mat, td_ext->dquat);
            mul_m3_m3m3(fmat, fmat, tmp_mat);
          }

          mat3_to_quat(quat, fmat); /* Actual transform. */

          if (!is_zero_v4(td_ext->dquat)) {
            /* Correct back for delta quaternion. */
            float idquat[4];
            invert_qt_qt_normalized(idquat, td_ext->dquat);
            mul_qt_qtqt(quat, idquat, quat);
          }

          mul_qt_qtqt(td_ext->quat, quat, td_ext->iquat);

          /* This function works on end result. */
          protectedQuaternionBits(td->protectflag, td_ext->quat, td_ext->iquat);
        }
      }
      else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* Calculate effect based on quaternions. */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td_ext->irotAxis, td_ext->irotAngle);

        mul_m3_series(fmat, td->smtx, mat, td->mtx);
        mat3_to_quat(quat, fmat); /* Actual transform. */
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td_ext->rotAxis, td_ext->rotAngle, tquat);

        /* This function works on end result. */
        protectedAxisAngleBits(td->protectflag,
                               td_ext->rotAxis,
                               td_ext->rotAngle,
                               td_ext->irotAxis,
                               td_ext->irotAngle);
      }
      else {
        /* Calculate the total rotation in eulers. */
        float obmat[3][3];

        mul_m3_m3m3(totmat, mat, td->mtx);
        mul_m3_m3m3(smat, td->smtx, totmat);

        if (!is_zero_v3(td_ext->drot)) {
          /* Correct for delta rot. */
          add_eul_euleul(eul, td_ext->irot, td_ext->drot, td_ext->rotOrder);
        }
        else {
          copy_v3_v3(eul, td_ext->irot);
        }

        eulO_to_mat3(obmat, eul, td_ext->rotOrder);
        mul_m3_m3m3(fmat, smat, obmat);
        mat3_to_compatible_eulO(eul, td_ext->rot, td_ext->rotOrder, fmat);

        if (!is_zero_v3(td_ext->drot)) {
          /* Correct back for delta rot. */
          sub_eul_euleul(eul, eul, td_ext->drot, td_ext->rotOrder);
        }

        /* And apply. */
        protectedRotateBits(td->protectflag, eul, td_ext->irot);
        copy_v3_v3(td_ext->rot, eul);
      }

      constraintRotLim(t, td, td_ext);
    }
  }
}

void ElementRotation(const TransInfo *t,
                     const TransDataContainer *tc,
                     TransData *td,
                     TransDataExtension *td_ext,
                     const float mat[3][3],
                     const short around)
{
  const float *center;

  /* Local constraint shouldn't alter center. */
  if (transdata_check_local_center(t, around)) {
    center = td->center;
  }
  else {
    center = tc->center_local;
  }

  ElementRotation_ex(t, tc, td, td_ext, mat, center);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Resize Utils)
 * \{ */

void headerResize(TransInfo *t, const float vec[3], char *str, const int str_size)
{
  char tvec[NUM_STR_REP_LEN * 3];
  size_t ofs = 0;
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, t->scene->unit);
  }
  else {
    BLI_snprintf_utf8(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
    BLI_snprintf_utf8(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
    BLI_snprintf_utf8(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
  }

  if (t->con.mode & CON_APPLY) {
    switch (t->num.idx_max) {
      case 0:
        ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                      str_size - ofs,
                                      IFACE_("Scale: %s%s %s"),
                                      &tvec[0],
                                      t->con.text,
                                      t->proptext);
        break;
      case 1:
        ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                      str_size - ofs,
                                      IFACE_("Scale: %s : %s%s %s"),
                                      &tvec[0],
                                      &tvec[NUM_STR_REP_LEN],
                                      t->con.text,
                                      t->proptext);
        break;
      case 2:
        ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                      str_size - ofs,
                                      IFACE_("Scale: %s : %s : %s%s %s"),
                                      &tvec[0],
                                      &tvec[NUM_STR_REP_LEN],
                                      &tvec[NUM_STR_REP_LEN * 2],
                                      t->con.text,
                                      t->proptext);
        break;
    }
  }
  else {
    if (t->flag & T_2D_EDIT) {
      ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                    str_size - ofs,
                                    IFACE_("Scale X: %s   Y: %s%s %s"),
                                    &tvec[0],
                                    &tvec[NUM_STR_REP_LEN],
                                    t->con.text,
                                    t->proptext);
    }
    else {
      ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                    str_size - ofs,
                                    IFACE_("Scale X: %s   Y: %s  Z: %s%s %s"),
                                    &tvec[0],
                                    &tvec[NUM_STR_REP_LEN],
                                    &tvec[NUM_STR_REP_LEN * 2],
                                    t->con.text,
                                    t->proptext);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_utf8_rlen(
        str + ofs, str_size - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
  }
}

/**
 * \a smat is reference matrix only.
 *
 * \note this is a tricky area, before making changes see: #29633, #42444
 */
static void TransMat3ToSize(const float mat[3][3], const float smat[3][3], float size[3])
{
  float rmat[3][3];

  mat3_to_rot_size(rmat, size, mat);

  /* First tried with dot-product... but the sign flip is crucial. */
  if (dot_v3v3(rmat[0], smat[0]) < 0.0f) {
    size[0] = -size[0];
  }
  if (dot_v3v3(rmat[1], smat[1]) < 0.0f) {
    size[1] = -size[1];
  }
  if (dot_v3v3(rmat[2], smat[2]) < 0.0f) {
    size[2] = -size[2];
  }
}

void ElementResize(const TransInfo *t,
                   const TransDataContainer *tc,
                   int td_index,
                   const float mat[3][3])
{
  TransData *td = &tc->data[td_index];

  float tmat[3][3], smat[3][3], center[3];
  float vec[3];

  if (t->flag & T_EDIT) {
    mul_m3_m3m3(smat, mat, td->mtx);
    mul_m3_m3m3(tmat, td->smtx, smat);
  }
  else {
    copy_m3_m3(tmat, mat);
  }

  if (t->con.applySize) {
    t->con.applySize(t, tc, td, tmat);
  }

  /* Local constraint shouldn't alter center. */
  if (transdata_check_local_center(t, t->around)) {
    copy_v3_v3(center, td->center);
  }
  else if (t->options & CTX_MOVIECLIP) {
    if (td->flag & TD_INDIVIDUAL_SCALE) {
      copy_v3_v3(center, td->center);
    }
    else {
      copy_v3_v3(center, tc->center_local);
    }
  }
  else {
    copy_v3_v3(center, tc->center_local);
  }

  if (tc->data_ext) {
    TransDataExtension *td_ext = &tc->data_ext[td_index];

    /* Size checked needed since the 3D cursor only uses rotation fields. */
    if (td_ext->scale) {
      float fscale[3];

      if (ELEM(t->data_type,
               &TransConvertType_Sculpt,
               &TransConvertType_Object,
               &TransConvertType_ObjectTexSpace,
               &TransConvertType_Pose))
      {
        float ob_scale_mat[3][3];
        /* Reorient the size mat to fit the oriented object. */
        mul_m3_m3m3(ob_scale_mat, tmat, td->axismtx);
        // print_m3("ob_scale_mat", ob_scale_mat);
        TransMat3ToSize(ob_scale_mat, td->axismtx, fscale);
        // print_v3("fscale", fscale);
      }
      else {
        mat3_to_size(fscale, tmat);
      }

      protectedScaleBits(td->protectflag, fscale);

      if ((t->flag & T_V3D_ALIGN) == 0) { /* Align mode doesn't resize objects itself. */
        if ((td->flag & TD_SINGLE_SCALE) && !(t->con.mode & CON_APPLY)) {
          /* Scale val and reset scale. */
          *td->val = td->ival * (1 + (fscale[0] - 1) * td->factor);

          td_ext->scale[0] = td_ext->iscale[0];
          td_ext->scale[1] = td_ext->iscale[1];
          td_ext->scale[2] = td_ext->iscale[2];
        }
        else {
          /* Reset val if #TD_SINGLE_SCALE but using a constraint. */
          if (td->flag & TD_SINGLE_SCALE) {
            *td->val = td->ival;
          }

          td_ext->scale[0] = td_ext->iscale[0] * (1 + (fscale[0] - 1) * td->factor);
          td_ext->scale[1] = td_ext->iscale[1] * (1 + (fscale[1] - 1) * td->factor);
          td_ext->scale[2] = td_ext->iscale[2] * (1 + (fscale[2] - 1) * td->factor);
        }
      }

      constraintScaleLim(t, tc, td_index);
    }
  }

  /* For individual element center, Editmode need to use iloc. */
  if (t->flag & T_POINTS) {
    sub_v3_v3v3(vec, td->iloc, center);
  }
  else {
    sub_v3_v3v3(vec, td->center, center);
  }

  mul_m3_v3(tmat, vec);

  add_v3_v3(vec, center);
  if (t->flag & T_POINTS) {
    sub_v3_v3(vec, td->iloc);
  }
  else {
    sub_v3_v3(vec, td->center);
  }

  /* Grease pencil falloff.
   *
   * FIXME: This is bad on multiple levels!
   *
   * - #applyNumInput is not intended to be run for every element,
   *   this writes back into the number input in a way that doesn't make sense to run many times.
   *
   * - Writing into #TransInfo should be avoided since it means order of operations
   *   may impact the result and isn't thread-safe.
   *
   *   Operating on copies as a temporary solution.
   */
  if (t->options & CTX_GPENCIL_STROKES) {
    const float *gp_falloff_ptr = static_cast<const float *>(td->extra);
    const float gp_falloff = gp_falloff_ptr != nullptr ? *gp_falloff_ptr : 1.0f;
    mul_v3_fl(vec, td->factor * gp_falloff);

    /* Scale stroke thickness. */
    if (td->val) {
      NumInput num_evil = t->num;
      float values_final_evil[4];
      copy_v4_v4(values_final_evil, t->values_final);
      transform_snap_increment(t, values_final_evil);
      applyNumInput(&num_evil, values_final_evil);

      float ratio = values_final_evil[0];
      float transformed_value = td->ival * fabs(ratio);
      *td->val = math::max(math::interpolate(td->ival, transformed_value, gp_falloff), 0.001f);
    }
  }
  else {
    mul_v3_fl(vec, td->factor);
  }

  if (t->options & (CTX_OBJECT | CTX_POSE_BONE)) {
    if (t->options & CTX_POSE_BONE) {
      /* Without this, the resulting location of scaled bones aren't correct,
       * especially noticeable scaling root or disconnected bones around the cursor, see #92515. */
      mul_mat3_m4_v3(tc->poseobj->object_to_world().ptr(), vec);
    }
    mul_m3_v3(td->smtx, vec);
  }

  protectedTransBits(td->protectflag, vec);
  if (td->loc) {
    add_v3_v3v3(td->loc, td->iloc, vec);
  }

  constraintTransLim(t, tc, td);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Mode Initialization
 * \{ */

static TransModeInfo *mode_info_get(TransInfo *t, const int mode)
{
  switch (mode) {
    case TFM_TRANSLATION:
      return &TransMode_translate;
    case TFM_ROTATION:
      return &TransMode_rotate;
    case TFM_RESIZE:
      return &TransMode_resize;
    case TFM_SKIN_RESIZE:
      return &TransMode_skinresize;
    case TFM_TOSPHERE:
      return &TransMode_tosphere;
    case TFM_SHEAR:
      return &TransMode_shear;
    case TFM_BEND:
      return &TransMode_bend;
    case TFM_SHRINKFATTEN:
      return &TransMode_shrinkfatten;
    case TFM_TILT:
      return &TransMode_tilt;
    case TFM_CURVE_SHRINKFATTEN:
      return &TransMode_curveshrinkfatten;
    case TFM_MASK_SHRINKFATTEN:
      return &TransMode_maskshrinkfatten;
    case TFM_TRACKBALL:
      return &TransMode_trackball;
    case TFM_PUSHPULL:
      return &TransMode_pushpull;
    case TFM_EDGE_CREASE:
      return &TransMode_edgecrease;
    case TFM_VERT_CREASE:
      return &TransMode_vertcrease;
    case TFM_BONESIZE:
      return &TransMode_bboneresize;
    case TFM_BONE_ENVELOPE:
    case TFM_BONE_ENVELOPE_DIST:
      return &TransMode_boneenvelope;
    case TFM_EDGE_SLIDE:
      return &TransMode_edgeslide;
    case TFM_VERT_SLIDE:
      return &TransMode_vertslide;
    case TFM_BONE_ROLL:
      return &TransMode_boneroll;
    case TFM_TIME_TRANSLATE:
      return &TransMode_timetranslate;
    case TFM_TIME_SLIDE:
      return &TransMode_timeslide;
    case TFM_TIME_SCALE:
      return &TransMode_timescale;
    case TFM_TIME_EXTEND:
      /* Do TFM_TIME_TRANSLATE (for most Animation Editors because they have only 1D transforms for
       * time values) or TFM_TRANSLATION (for Graph/NLA Editors only since they uses 'standard'
       * transforms to get 2D movement) depending on which editor this was called from. */
      if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
        return &TransMode_translate;
      }
      return &TransMode_timetranslate;
    case TFM_BAKE_TIME:
      return &TransMode_baketime;
    case TFM_MIRROR:
      return &TransMode_mirror;
    case TFM_BWEIGHT:
      return &TransMode_bevelweight;
    case TFM_ALIGN:
      return &TransMode_align;
    case TFM_SEQ_SLIDE:
      return &TransMode_seqslide;
    case TFM_NORMAL_ROTATION:
      return &TransMode_rotatenormal;
    case TFM_GPENCIL_OPACITY:
      return &TransMode_gpopacity;
  }
  return nullptr;
}

void transform_mode_init(TransInfo *t, wmOperator *op, const int mode)
{
  t->mode = eTfmMode(mode);
  t->mode_info = mode_info_get(t, mode);

  if (t->mode_info) {
    t->flag |= eTFlag(t->mode_info->flags);
    t->mode_info->init_fn(t, op);
  }

  if (t->data_type == &TransConvertType_Mesh) {
    /* Init Custom Data correction.
     * Ideally this should be called when creating the TransData. */
    transform_convert_mesh_customdatacorrect_init(t);
  }

  transform_gizmo_3d_model_from_constraint_and_mode_set(t);

  /* TODO(@mano-wii): Some of these operations change the `t->mode`.
   * This can be bad for Redo. */
  // BLI_assert(t->mode == mode);
}

void transform_mode_default_modal_orientation_set(TransInfo *t, int type)
{
  /* Currently only these types are supported. */
  BLI_assert(ELEM(type, V3D_ORIENT_GLOBAL, V3D_ORIENT_VIEW));

  if (t->is_orient_default_overwrite) {
    return;
  }

  if (!(t->flag & T_MODAL)) {
    return;
  }

  if (t->orient[O_DEFAULT].type == type) {
    return;
  }

  View3D *v3d = nullptr;
  RegionView3D *rv3d = nullptr;
  if ((type == V3D_ORIENT_VIEW) && (t->spacetype == SPACE_VIEW3D) && t->region &&
      (t->region->regiontype == RGN_TYPE_WINDOW))
  {
    v3d = static_cast<View3D *>(t->view);
    rv3d = static_cast<RegionView3D *>(t->region->regiondata);
  }

  t->orient[O_DEFAULT].type = calc_orientation_from_type_ex(t->scene,
                                                            t->view_layer,
                                                            v3d,
                                                            rv3d,
                                                            nullptr,
                                                            nullptr,
                                                            type,
                                                            V3D_AROUND_CENTER_BOUNDS,
                                                            t->orient[O_DEFAULT].matrix);

  if (t->orient_curr == O_DEFAULT) {
    /* Update Orientation. */
    transform_orientations_current_set(t, O_DEFAULT);
  }
}

void transform_mode_rotation_axis_get(const TransInfo *t, float3 &r_axis)
{
  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, nullptr, nullptr, r_axis);
  }
  else {
    r_axis = t->spacemtx[t->orient_axis];
  }
}

bool transform_mode_is_axis_pointing_to_screen(const TransInfo *t, const float3 &axis)
{
  float view_vector[3];
  view_vector_calc(t, t->center_global, view_vector);
  return dot_v3v3(axis, view_vector) > 0.0f;
}

/** \} */

}  // namespace blender::ed::transform
