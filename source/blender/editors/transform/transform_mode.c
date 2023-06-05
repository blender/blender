/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_nla.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_gizmo.h"
#include "transform_orientations.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_mode.h"

eTfmMode transform_mode_really_used(bContext *C, eTfmMode mode)
{
  if (mode == TFM_BONESIZE) {
    Object *ob = CTX_data_active_object(C);
    BLI_assert(ob);
    if (ob->type != OB_ARMATURE) {
      return TFM_RESIZE;
    }
    bArmature *arm = ob->data;
    if (arm->drawtype == ARM_ENVELOPE) {
      return TFM_BONE_ENVELOPE_DIST;
    }
  }

  return mode;
}

bool transdata_check_local_center(const TransInfo *t, short around)
{
  return (
      (around == V3D_AROUND_LOCAL_ORIGINS) &&
      ((t->options & (CTX_OBJECT | CTX_POSE_BONE)) ||
       /* implicit: (t->flag & T_EDIT) */
       ELEM(t->obedit_type, OB_MESH, OB_CURVES_LEGACY, OB_MBALL, OB_ARMATURE, OB_GPENCIL_LEGACY) ||
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

/* this function only does the delta rotation */
static void protectedQuaternionBits(short protectflag, float quat[4], const float oldquat[4])
{
  /* check that protection flags are set */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* quaternions getting limited as 4D entities that they are... */
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
    /* quaternions get limited with euler... (compatibility mode) */
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

    /* restore original quat size */
    mul_qt_fl(quat, qlen);

    /* quaternions flip w sign to accumulate rotations correctly */
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

/* this function only does the delta rotation */
/* axis-angle is usually internally stored as quats... */
static void protectedAxisAngleBits(
    short protectflag, float axis[3], float *angle, const float oldAxis[3], float oldAngle)
{
  /* check that protection flags are set */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* axis-angle getting limited as 4D entities that they are... */
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
    /* axis-angle get limited with euler... */
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
      /* for now, rotate around y-axis then (so that it simply becomes the roll) */
      axis[1] = 1.0f;
    }
  }
}

void protectedSizeBits(short protectflag, float size[3])
{
  if (protectflag & OB_LOCK_SCALEX) {
    size[0] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEY) {
    size[1] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEZ) {
    size[2] = 1.0f;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Limits
 * \{ */

void constraintTransLim(const TransInfo *t, TransData *td)
{
  if (td->con) {
    const bConstraintTypeInfo *ctiLoc = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_LOCLIMIT);
    const bConstraintTypeInfo *ctiDist = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_DISTLIMIT);

    bConstraintOb cob = {NULL};
    bConstraint *con;
    float ctime = (float)(t->scene->r.cfra);

    /* Make a temporary bConstraintOb for using these limit constraints
     * - they only care that cob->matrix is correctly set ;-)
     * - current space should be local
     */
    unit_m4(cob.matrix);
    copy_v3_v3(cob.matrix[3], td->loc);

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      const bConstraintTypeInfo *cti = NULL;
      ListBase targets = {NULL, NULL};

      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* only use it if it's tagged for this purpose (and the right type) */
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
        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }
        else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
          /* skip... incompatible spacetype */
          continue;
        }

        /* Initialize the custom space for use in calculating the matrices. */
        BKE_constraint_custom_object_space_init(&cob, con);

        /* get constraint targets if needed */
        BKE_constraint_targets_for_solving_get(t->depsgraph, con, &cob, &targets, ctime);

        /* do constraint */
        cti->evaluate_constraint(con, &cob, &targets);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }

        /* free targets list */
        BLI_freelistN(&targets);
      }
    }

    /* copy results from cob->matrix */
    copy_v3_v3(td->loc, cob.matrix[3]);
  }
}

static void constraintob_from_transdata(bConstraintOb *cob, TransData *td)
{
  /* Make a temporary bConstraintOb for use by limit constraints
   * - they only care that cob->matrix is correctly set ;-)
   * - current space should be local
   */
  memset(cob, 0, sizeof(bConstraintOb));
  if (td->ext) {
    if (td->ext->rotOrder == ROT_MODE_QUAT) {
      /* quats */
      /* objects and bones do normalization first too, otherwise
       * we don't necessarily end up with a rotation matrix, and
       * then conversion back to quat gives a different result */
      float quat[4];
      normalize_qt_qt(quat, td->ext->quat);
      quat_to_mat4(cob->matrix, quat);
    }
    else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
      /* axis angle */
      axis_angle_to_mat4(cob->matrix, td->ext->rotAxis, *td->ext->rotAngle);
    }
    else {
      /* eulers */
      eulO_to_mat4(cob->matrix, td->ext->rot, td->ext->rotOrder);
    }
  }
}

static void constraintRotLim(const TransInfo *UNUSED(t), TransData *td)
{
  if (td->con) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_ROTLIMIT);
    bConstraintOb cob;
    bConstraint *con;
    bool do_limit = false;

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* we're only interested in Limit-Rotation constraints */
      if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
        bRotLimitConstraint *data = (bRotLimitConstraint *)con->data;

        /* only use it if it's tagged for this purpose */
        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }

        /* Skip incompatible space-types. */
        if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
          continue;
        }

        /* Only do conversion if necessary, to preserve quaternion and euler rotations. */
        if (do_limit == false) {
          constraintob_from_transdata(&cob, td);
          do_limit = true;
        }

        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }

        /* do constraint */
        cti->evaluate_constraint(con, &cob, NULL);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }
      }
    }

    if (do_limit) {
      /* copy results from cob->matrix */
      if (td->ext->rotOrder == ROT_MODE_QUAT) {
        /* quats */
        mat4_to_quat(td->ext->quat, cob.matrix);
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* axis angle */
        mat4_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, cob.matrix);
      }
      else {
        /* eulers */
        mat4_to_eulO(td->ext->rot, td->ext->rotOrder, cob.matrix);
      }
    }
  }
}

void constraintSizeLim(const TransInfo *t, TransData *td)
{
  if (td->con && td->ext) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_SIZELIMIT);
    bConstraintOb cob = {NULL};
    bConstraint *con;
    float size_sign[3], size_abs[3];
    int i;

    /* Make a temporary bConstraintOb for using these limit constraints
     * - they only care that cob->matrix is correctly set ;-)
     * - current space should be local
     */
    if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
      /* scale val and reset size */
      return; /* TODO: fix this case */
    }

    /* Reset val if SINGLESIZE but using a constraint */
    if (td->flag & TD_SINGLESIZE) {
      return;
    }

    /* separate out sign to apply back later */
    for (i = 0; i < 3; i++) {
      size_sign[i] = signf(td->ext->size[i]);
      size_abs[i] = fabsf(td->ext->size[i]);
    }

    size_to_mat4(cob.matrix, size_abs);

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* we're only interested in Limit-Scale constraints */
      if (con->type == CONSTRAINT_TYPE_SIZELIMIT) {
        bSizeLimitConstraint *data = con->data;

        /* only use it if it's tagged for this purpose */
        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }

        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }
        else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
          /* skip... incompatible spacetype */
          continue;
        }

        /* do constraint */
        cti->evaluate_constraint(con, &cob, NULL);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }
      }
    }

    /* copy results from cob->matrix */
    if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
      /* scale val and reset size */
      return; /* TODO: fix this case. */
    }

    /* Reset val if SINGLESIZE but using a constraint */
    if (td->flag & TD_SINGLESIZE) {
      return;
    }

    /* Extract scale from matrix and apply back sign. */
    mat4_to_size(td->ext->size, cob.matrix);
    mul_v3_v3(td->ext->size, size_sign);
  }
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

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf_rlen(
        str + ofs, str_size - ofs, TIP_("Rotation: %s %s %s"), &c[0], t->con.text, t->proptext);
  }
  else {
    ofs += BLI_snprintf_rlen(str + ofs,
                             str_size - ofs,
                             TIP_("Rotation: %.2f%s %s"),
                             RAD2DEGF(final),
                             t->con.text,
                             t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_rlen(
        str + ofs, str_size - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

void ElementRotation_ex(const TransInfo *t,
                        const TransDataContainer *tc,
                        TransData *td,
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
      bGPDstroke *gps = (bGPDstroke *)td->extra;
      if (gps->runtime.multi_frame_falloff != 1.0f) {
        float ident_mat[3][3];
        unit_m3(ident_mat);
        interp_m3_m3m3(smat, ident_mat, smat, gps->runtime.multi_frame_falloff);
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
      mat3_to_quat(quat, fmat); /* Actual transform */

      if (td->ext->quat) {
        mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);

        /* is there a reason not to have this here? -jahka */
        protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
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
    /* Extract and invert armature object matrix */

    if ((td->flag & TD_NO_LOC) == 0) {
      sub_v3_v3v3(vec, td->center, center);

      mul_m3_v3(tc->mat3, vec);  /* To Global space. */
      mul_m3_v3(mat, vec);       /* Applying rotation. */
      mul_m3_v3(tc->imat3, vec); /* To Local space. */

      add_v3_v3(vec, center);
      /* vec now is the location where the object has to be */

      sub_v3_v3v3(vec, vec, td->center); /* Translation needed from the initial location */

      /* special exception, see TD_PBONE_LOCAL_MTX definition comments */
      if (td->flag & TD_PBONE_LOCAL_MTX_P) {
        /* do nothing */
      }
      else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
        mul_m3_v3(tc->mat3, vec);        /* To Global space. */
        mul_m3_v3(td->ext->l_smtx, vec); /* To Pose space (Local Location). */
      }
      else {
        mul_m3_v3(tc->mat3, vec); /* To Global space. */
        mul_m3_v3(td->smtx, vec); /* To Pose space. */
      }

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);

      constraintTransLim(t, td);
    }

    /* rotation */
    /* MORE HACK: as in some cases the matrix to apply location and rot/scale is not the same,
     * and ElementRotation() might be called in Translation context (with align snapping),
     * we need to be sure to actually use the *rotation* matrix here...
     * So no other way than storing it in some dedicated members of td->ext! */
    if ((t->flag & T_V3D_ALIGN) == 0) { /* align mode doesn't rotate objects itself */
      /* euler or quaternion/axis-angle? */
      if (td->ext->rotOrder == ROT_MODE_QUAT) {
        mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);

        mat3_to_quat(quat, fmat); /* Actual transform */

        mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
        /* this function works on end result */
        protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* calculate effect based on quats */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);

        mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);
        mat3_to_quat(quat, fmat); /* Actual transform */
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);

        /* this function works on end result */
        protectedAxisAngleBits(td->protectflag,
                               td->ext->rotAxis,
                               td->ext->rotAngle,
                               td->ext->irotAxis,
                               td->ext->irotAngle);
      }
      else {
        float eulmat[3][3];

        mul_m3_m3m3(totmat, mat, td->ext->r_mtx);
        mul_m3_m3m3(smat, td->ext->r_smtx, totmat);

        /* Calculate the total rotation in eulers. */
        copy_v3_v3(eul, td->ext->irot);
        eulO_to_mat3(eulmat, eul, td->ext->rotOrder);

        /* mat = transform, obmat = bone rotation */
        mul_m3_m3m3(fmat, smat, eulmat);

        mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

        /* and apply (to end result only) */
        protectedRotateBits(td->protectflag, eul, td->ext->irot);
        copy_v3_v3(td->ext->rot, eul);
      }

      constraintRotLim(t, td);
    }
  }
  else {
    if ((td->flag & TD_NO_LOC) == 0) {
      /* translation */
      sub_v3_v3v3(vec, td->center, center);
      mul_m3_v3(mat, vec);
      add_v3_v3(vec, center);
      /* vec now is the location where the object has to be */
      sub_v3_v3(vec, td->center);
      mul_m3_v3(td->smtx, vec);

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);
    }

    constraintTransLim(t, td);

    /* rotation */
    if ((t->flag & T_V3D_ALIGN) == 0) { /* Align mode doesn't rotate objects itself. */
      /* euler or quaternion? */
      if ((td->ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
        /* can be called for texture space translate for example, then opt out */
        if (td->ext->quat) {
          mul_m3_series(fmat, td->smtx, mat, td->mtx);

          if (!is_zero_v3(td->ext->dquat)) {
            /* Correct for delta quat */
            float tmp_mat[3][3];
            quat_to_mat3(tmp_mat, td->ext->dquat);
            mul_m3_m3m3(fmat, fmat, tmp_mat);
          }

          mat3_to_quat(quat, fmat); /* Actual transform */

          if (!is_zero_v4(td->ext->dquat)) {
            /* Correct back for delta quat. */
            float idquat[4];
            invert_qt_qt_normalized(idquat, td->ext->dquat);
            mul_qt_qtqt(quat, idquat, quat);
          }

          mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);

          /* this function works on end result */
          protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
        }
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* calculate effect based on quats */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);

        mul_m3_series(fmat, td->smtx, mat, td->mtx);
        mat3_to_quat(quat, fmat); /* Actual transform */
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);

        /* this function works on end result */
        protectedAxisAngleBits(td->protectflag,
                               td->ext->rotAxis,
                               td->ext->rotAngle,
                               td->ext->irotAxis,
                               td->ext->irotAngle);
      }
      else {
        /* Calculate the total rotation in eulers. */
        float obmat[3][3];

        mul_m3_m3m3(totmat, mat, td->mtx);
        mul_m3_m3m3(smat, td->smtx, totmat);

        if (!is_zero_v3(td->ext->drot)) {
          /* Correct for delta rot */
          add_eul_euleul(eul, td->ext->irot, td->ext->drot, td->ext->rotOrder);
        }
        else {
          copy_v3_v3(eul, td->ext->irot);
        }

        eulO_to_mat3(obmat, eul, td->ext->rotOrder);
        mul_m3_m3m3(fmat, smat, obmat);
        mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

        if (!is_zero_v3(td->ext->drot)) {
          /* Correct back for delta rot. */
          sub_eul_euleul(eul, eul, td->ext->drot, td->ext->rotOrder);
        }

        /* and apply */
        protectedRotateBits(td->protectflag, eul, td->ext->irot);
        copy_v3_v3(td->ext->rot, eul);
      }

      constraintRotLim(t, td);
    }
  }
}

void ElementRotation(const TransInfo *t,
                     const TransDataContainer *tc,
                     TransData *td,
                     const float mat[3][3],
                     const short around)
{
  const float *center;

  /* local constraint shouldn't alter center */
  if (transdata_check_local_center(t, around)) {
    center = td->center;
  }
  else {
    center = tc->center_local;
  }

  ElementRotation_ex(t, tc, td, mat, center);
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
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
  }

  if (t->con.mode & CON_APPLY) {
    switch (t->num.idx_max) {
      case 0:
        ofs += BLI_snprintf_rlen(
            str + ofs, str_size - ofs, TIP_("Scale: %s%s %s"), &tvec[0], t->con.text, t->proptext);
        break;
      case 1:
        ofs += BLI_snprintf_rlen(str + ofs,
                                 str_size - ofs,
                                 TIP_("Scale: %s : %s%s %s"),
                                 &tvec[0],
                                 &tvec[NUM_STR_REP_LEN],
                                 t->con.text,
                                 t->proptext);
        break;
      case 2:
        ofs += BLI_snprintf_rlen(str + ofs,
                                 str_size - ofs,
                                 TIP_("Scale: %s : %s : %s%s %s"),
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
      ofs += BLI_snprintf_rlen(str + ofs,
                               str_size - ofs,
                               TIP_("Scale X: %s   Y: %s%s %s"),
                               &tvec[0],
                               &tvec[NUM_STR_REP_LEN],
                               t->con.text,
                               t->proptext);
    }
    else {
      ofs += BLI_snprintf_rlen(str + ofs,
                               str_size - ofs,
                               TIP_("Scale X: %s   Y: %s  Z: %s%s %s"),
                               &tvec[0],
                               &tvec[NUM_STR_REP_LEN],
                               &tvec[NUM_STR_REP_LEN * 2],
                               t->con.text,
                               t->proptext);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_rlen(
        str + ofs, str_size - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
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
                   TransData *td,
                   const float mat[3][3])
{
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

  /* local constraint shouldn't alter center */
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

  /* Size checked needed since the 3D cursor only uses rotation fields. */
  if (td->ext && td->ext->size) {
    float fsize[3];

    if (ELEM(t->data_type,
             &TransConvertType_Sculpt,
             &TransConvertType_Object,
             &TransConvertType_ObjectTexSpace,
             &TransConvertType_Pose))
    {
      float obsizemat[3][3];
      /* Reorient the size mat to fit the oriented object. */
      mul_m3_m3m3(obsizemat, tmat, td->axismtx);
      // print_m3("obsizemat", obsizemat);
      TransMat3ToSize(obsizemat, td->axismtx, fsize);
      // print_v3("fsize", fsize);
    }
    else {
      mat3_to_size(fsize, tmat);
    }

    protectedSizeBits(td->protectflag, fsize);

    if ((t->flag & T_V3D_ALIGN) == 0) { /* align mode doesn't resize objects itself */
      if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
        /* scale val and reset size */
        *td->val = td->ival * (1 + (fsize[0] - 1) * td->factor);

        td->ext->size[0] = td->ext->isize[0];
        td->ext->size[1] = td->ext->isize[1];
        td->ext->size[2] = td->ext->isize[2];
      }
      else {
        /* Reset val if SINGLESIZE but using a constraint */
        if (td->flag & TD_SINGLESIZE) {
          *td->val = td->ival;
        }

        td->ext->size[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
        td->ext->size[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
        td->ext->size[2] = td->ext->isize[2] * (1 + (fsize[2] - 1) * td->factor);
      }
    }

    constraintSizeLim(t, td);
  }

  /* For individual element center, Editmode need to use iloc */
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
    bGPDstroke *gps = (bGPDstroke *)td->extra;
    mul_v3_fl(vec, td->factor * gps->runtime.multi_frame_falloff);

    /* Scale stroke thickness. */
    if (td->val) {
      NumInput num_evil = t->num;
      float values_final_evil[4];
      copy_v4_v4(values_final_evil, t->values_final);
      transform_snap_increment(t, values_final_evil);
      applyNumInput(&num_evil, values_final_evil);

      float ratio = values_final_evil[0];
      float transformed_value = td->ival * fabs(ratio);
      *td->val = max_ff(interpf(transformed_value, td->ival, gps->runtime.multi_frame_falloff),
                        0.001f);
    }
  }
  else {
    mul_v3_fl(vec, td->factor);
  }

  if (t->options & (CTX_OBJECT | CTX_POSE_BONE)) {
    if (t->options & CTX_POSE_BONE) {
      /* Without this, the resulting location of scaled bones aren't correct,
       * especially noticeable scaling root or disconnected bones around the cursor, see #92515. */
      mul_mat3_m4_v3(tc->poseobj->object_to_world, vec);
    }
    mul_m3_v3(td->smtx, vec);
  }

  protectedTransBits(td->protectflag, vec);
  if (td->loc) {
    add_v3_v3v3(td->loc, td->iloc, vec);
  }

  constraintTransLim(t, td);
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
    case TFM_GPENCIL_SHRINKFATTEN:
      return &TransMode_gpshrinkfatten;
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
  return NULL;
}

void transform_mode_init(TransInfo *t, wmOperator *op, const int mode)
{
  t->mode = mode;
  t->mode_info = mode_info_get(t, mode);

  if (t->mode_info) {
    t->flag |= t->mode_info->flags;
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

  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;
  if ((type == V3D_ORIENT_VIEW) && (t->spacetype == SPACE_VIEW3D) && t->region &&
      (t->region->regiontype == RGN_TYPE_WINDOW))
  {
    v3d = t->view;
    rv3d = t->region->regiondata;
  }

  t->orient[O_DEFAULT].type = ED_transform_calc_orientation_from_type_ex(
      t->scene,
      t->view_layer,
      v3d,
      rv3d,
      NULL,
      NULL,
      type,
      V3D_AROUND_CENTER_BOUNDS,
      t->orient[O_DEFAULT].matrix);

  if (t->orient_curr == O_DEFAULT) {
    /* Update Orientation. */
    transform_orientations_current_set(t, O_DEFAULT);
  }
}

/** \} */
