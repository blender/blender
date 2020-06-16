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
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
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
#include "transform_snap.h"

/* Own include. */
#include "transform_mode.h"

bool transdata_check_local_center(TransInfo *t, short around)
{
  return ((around == V3D_AROUND_LOCAL_ORIGINS) &&
          ((t->flag & (T_OBJECT | T_POSE)) ||
           /* implicit: (t->flag & T_EDIT) */
           (ELEM(t->obedit_type, OB_MESH, OB_CURVE, OB_MBALL, OB_ARMATURE, OB_GPENCIL)) ||
           (t->spacetype == SPACE_GRAPH) ||
           (t->options & (CTX_MOVIECLIP | CTX_MASK | CTX_PAINT_CURVE))));
}

/* Informs if the mode can be switched during modal. */
bool transform_mode_is_changeable(const int mode)
{
  return ELEM(mode,
              TFM_ROTATION,
              TFM_RESIZE,
              TFM_TRACKBALL,
              TFM_TRANSLATION,
              TFM_EDGE_SLIDE,
              TFM_VERT_SLIDE);
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
    short protectflag, float axis[3], float *angle, float oldAxis[3], float oldAngle)
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

static void protectedSizeBits(short protectflag, float size[3])
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

void constraintTransLim(TransInfo *t, TransData *td)
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

static void constraintRotLim(TransInfo *UNUSED(t), TransData *td)
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

        /* skip incompatible spacetypes */
        if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
          continue;
        }

        /* only do conversion if necessary, to preserve quats and eulers */
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

static void constraintSizeLim(TransInfo *t, TransData *td)
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
      return;  // TODO: fix this case
    }
    else {
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
    }

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
      return;  // TODO: fix this case
    }
    else {
      /* Reset val if SINGLESIZE but using a constraint */
      if (td->flag & TD_SINGLESIZE) {
        return;
      }

      /* extrace scale from matrix and apply back sign */
      mat4_to_size(td->ext->size, cob.matrix);
      mul_v3_v3(td->ext->size, size_sign);
    }
  }
}

/* -------------------------------------------------------------------- */
/* Transform (Rotation Utils) */

/** \name Transform Rotation Utils
 * \{ */
/* Used by Transform Rotation and Transform Normal Rotation */
void headerRotation(TransInfo *t, char str[UI_MAX_DRAW_STR], float final)
{
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_("Rot: %s %s %s"), &c[0], t->con.text, t->proptext);
  }
  else {
    ofs += BLI_snprintf(str + ofs,
                        UI_MAX_DRAW_STR - ofs,
                        TIP_("Rot: %.2f%s %s"),
                        RAD2DEGF(final),
                        t->con.text,
                        t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

void postInputRotation(TransInfo *t, float values[3])
{
  float axis_final[3];
  copy_v3_v3(axis_final, t->spacemtx[t->orient_axis]);
  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, NULL, NULL, axis_final, values);
  }
}

/**
 * Applies values of rotation to `td->loc` and `td->ext->quat`
 * based on a rotation matrix (mat) and a pivot (center).
 *
 * Protected axis and other transform settings are taken into account.
 */
void ElementRotation_ex(TransInfo *t,
                        TransDataContainer *tc,
                        TransData *td,
                        const float mat[3][3],
                        const float *center)
{
  float vec[3], totmat[3][3], smat[3][3];
  float eul[3], fmat[3][3], quat[4];

  if (t->flag & T_POINTS) {
    mul_m3_m3m3(totmat, mat, td->mtx);
    mul_m3_m3m3(smat, td->smtx, totmat);

    /* apply gpencil falloff */
    if (t->options & CTX_GPENCIL_STROKES) {
      bGPDstroke *gps = (bGPDstroke *)td->extra;
      float sx = smat[0][0];
      float sy = smat[1][1];
      float sz = smat[2][2];

      mul_m3_fl(smat, gps->runtime.multi_frame_falloff);
      /* fix scale */
      smat[0][0] = sx;
      smat[1][1] = sy;
      smat[2][2] = sz;
    }

    sub_v3_v3v3(vec, td->iloc, center);
    mul_m3_v3(smat, vec);

    add_v3_v3v3(td->loc, vec, center);

    sub_v3_v3v3(vec, td->loc, td->iloc);
    protectedTransBits(td->protectflag, vec);
    add_v3_v3v3(td->loc, td->iloc, vec);

    if (td->flag & TD_USEQUAT) {
      mul_m3_series(fmat, td->smtx, mat, td->mtx);
      mat3_to_quat(quat, fmat);  // Actual transform

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
  else if (t->flag & T_POSE) {
    // Extract and invert armature object matrix

    if ((td->flag & TD_NO_LOC) == 0) {
      sub_v3_v3v3(vec, td->center, center);

      mul_m3_v3(tc->mat3, vec);   // To Global space
      mul_m3_v3(mat, vec);        // Applying rotation
      mul_m3_v3(tc->imat3, vec);  // To Local space

      add_v3_v3(vec, center);
      /* vec now is the location where the object has to be */

      sub_v3_v3v3(vec, vec, td->center);  // Translation needed from the initial location

      /* special exception, see TD_PBONE_LOCAL_MTX definition comments */
      if (td->flag & TD_PBONE_LOCAL_MTX_P) {
        /* do nothing */
      }
      else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
        mul_m3_v3(tc->mat3, vec);         // To Global space
        mul_m3_v3(td->ext->l_smtx, vec);  // To Pose space (Local Location)
      }
      else {
        mul_m3_v3(tc->mat3, vec);  // To Global space
        mul_m3_v3(td->smtx, vec);  // To Pose space
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

        /* calculate the total rotatation in eulers */
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
    if ((t->flag & T_V3D_ALIGN) == 0) {  // align mode doesn't rotate objects itself
      /* euler or quaternion? */
      if ((td->ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
        /* can be called for texture space translate for example, then opt out */
        if (td->ext->quat) {
          mul_m3_series(fmat, td->smtx, mat, td->mtx);
          mat3_to_quat(quat, fmat);  // Actual transform

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
        mat3_to_quat(quat, fmat);  // Actual transform
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
        float obmat[3][3];

        mul_m3_m3m3(totmat, mat, td->mtx);
        mul_m3_m3m3(smat, td->smtx, totmat);

        /* calculate the total rotatation in eulers */
        add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* correct for delta rot */
        eulO_to_mat3(obmat, eul, td->ext->rotOrder);
        /* mat = transform, obmat = object rotation */
        mul_m3_m3m3(fmat, smat, obmat);

        mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

        /* correct back for delta rot */
        sub_v3_v3v3(eul, eul, td->ext->drot);

        /* and apply */
        protectedRotateBits(td->protectflag, eul, td->ext->irot);
        copy_v3_v3(td->ext->rot, eul);
      }

      constraintRotLim(t, td);
    }
  }
}

void ElementRotation(
    TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3], const short around)
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
/* Transform (Resize Utils) */

/** \name Transform Resize Utils
 * \{ */
void headerResize(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
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
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            TIP_("Scale: %s%s %s"),
                            &tvec[0],
                            t->con.text,
                            t->proptext);
        break;
      case 1:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            TIP_("Scale: %s : %s%s %s"),
                            &tvec[0],
                            &tvec[NUM_STR_REP_LEN],
                            t->con.text,
                            t->proptext);
        break;
      case 2:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
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
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          TIP_("Scale X: %s   Y: %s%s %s"),
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          t->con.text,
                          t->proptext);
    }
    else {
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          TIP_("Scale X: %s   Y: %s  Z: %s%s %s"),
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          &tvec[NUM_STR_REP_LEN * 2],
                          t->con.text,
                          t->proptext);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

/**
 * \a smat is reference matrix only.
 *
 * \note this is a tricky area, before making changes see: T29633, T42444
 */
static void TransMat3ToSize(float mat[3][3], float smat[3][3], float size[3])
{
  float rmat[3][3];

  mat3_to_rot_size(rmat, size, mat);

  /* first tried with dotproduct... but the sign flip is crucial */
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

void ElementResize(TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3])
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

    if (ELEM(t->data_type, TC_SCULPT, TC_OBJECT, TC_OBJECT_TEXSPACE, TC_POSE)) {
      float obsizemat[3][3];
      /* Reorient the size mat to fit the oriented object. */
      mul_m3_m3m3(obsizemat, tmat, td->axismtx);
      /* print_m3("obsizemat", obsizemat); */
      TransMat3ToSize(obsizemat, td->axismtx, fsize);
      /* print_v3("fsize", fsize); */
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

  /* grease pencil falloff */
  if (t->options & CTX_GPENCIL_STROKES) {
    bGPDstroke *gps = (bGPDstroke *)td->extra;
    mul_v3_fl(vec, td->factor * gps->runtime.multi_frame_falloff);

    /* scale stroke thickness */
    if (td->val) {
      snapGridIncrement(t, t->values_final);
      applyNumInput(&t->num, t->values_final);

      float ratio = t->values_final[0];
      *td->val = td->ival * ratio * gps->runtime.multi_frame_falloff;
      CLAMP_MIN(*td->val, 0.001f);
    }
  }
  else {
    mul_v3_fl(vec, td->factor);
  }

  if (t->flag & (T_OBJECT | T_POSE)) {
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
/* Transform (Frame Utils) */

/** \name Transform Frame Utils
 * \{ */

/* This function returns the snapping 'mode' for Animation Editors only
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 */
// XXX these modifier checks should be keymappable
short getAnimEdit_SnapMode(TransInfo *t)
{
  short autosnap = SACTSNAP_OFF;

  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;

    if (saction) {
      autosnap = saction->autosnap;
    }
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;

    if (sipo) {
      autosnap = sipo->autosnap;
    }
  }
  else if (t->spacetype == SPACE_NLA) {
    SpaceNla *snla = (SpaceNla *)t->area->spacedata.first;

    if (snla) {
      autosnap = snla->autosnap;
    }
  }
  else {
    autosnap = SACTSNAP_OFF;
  }

  /* toggle autosnap on/off
   * - when toggling on, prefer nearest frame over 1.0 frame increments
   */
  if (t->modifiers & MOD_SNAP_INVERT) {
    if (autosnap) {
      autosnap = SACTSNAP_OFF;
    }
    else {
      autosnap = SACTSNAP_FRAME;
    }
  }

  return autosnap;
}

/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
void doAnimEdit_SnapFrame(
    TransInfo *t, TransData *td, TransData2D *td2d, AnimData *adt, short autosnap)
{
  if (autosnap != SACTSNAP_OFF) {
    float val;

    /* convert frame to nla-action time (if needed) */
    if (adt && (t->spacetype != SPACE_SEQ)) {
      val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
    }
    else {
      val = *(td->val);
    }

    snapFrameTransform(t, autosnap, true, val, &val);

    /* convert frame out of nla-action time */
    if (adt && (t->spacetype != SPACE_SEQ)) {
      *(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
    }
    else {
      *(td->val) = val;
    }
  }

  /* If the handles are to be moved too
   * (as side-effect of keyframes moving, to keep the general effect)
   * offset them by the same amount so that the general angles are maintained
   * (i.e. won't change while handles are free-to-roam and keyframes are snap-locked).
   */
  if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
    td2d->h1[0] = td2d->ih1[0] + *td->val - td->ival;
  }
  if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
    td2d->h2[0] = td2d->ih2[0] + *td->val - td->ival;
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Mode Initialization
 * \{ */

void transform_mode_init(TransInfo *t, wmOperator *op, const int mode)
{
  t->mode = mode;

  switch (mode) {
    case TFM_TRANSLATION:
      initTranslation(t);
      break;
    case TFM_ROTATION:
      initRotation(t);
      break;
    case TFM_RESIZE:
      initResize(t);
      break;
    case TFM_SKIN_RESIZE:
      initSkinResize(t);
      break;
    case TFM_TOSPHERE:
      initToSphere(t);
      break;
    case TFM_SHEAR:
      initShear(t);
      break;
    case TFM_BEND:
      initBend(t);
      break;
    case TFM_SHRINKFATTEN:
      initShrinkFatten(t);
      break;
    case TFM_TILT:
      initTilt(t);
      break;
    case TFM_CURVE_SHRINKFATTEN:
      initCurveShrinkFatten(t);
      break;
    case TFM_MASK_SHRINKFATTEN:
      initMaskShrinkFatten(t);
      break;
    case TFM_GPENCIL_SHRINKFATTEN:
      initGPShrinkFatten(t);
      break;
    case TFM_TRACKBALL:
      initTrackball(t);
      break;
    case TFM_PUSHPULL:
      initPushPull(t);
      break;
    case TFM_CREASE:
      initCrease(t);
      break;
    case TFM_BONESIZE: { /* used for both B-Bone width (bonesize) as for deform-dist (envelope) */
      /* Note: we have to pick one, use the active object. */
      TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
      bArmature *arm = tc->poseobj->data;
      if (arm->drawtype == ARM_ENVELOPE) {
        initBoneEnvelope(t);
        t->mode = TFM_BONE_ENVELOPE_DIST;
      }
      else {
        initBoneSize(t);
      }
      break;
    }
    case TFM_BONE_ENVELOPE:
      initBoneEnvelope(t);
      break;
    case TFM_BONE_ENVELOPE_DIST:
      initBoneEnvelope(t);
      t->mode = TFM_BONE_ENVELOPE_DIST;
      break;
    case TFM_EDGE_SLIDE:
    case TFM_VERT_SLIDE: {
      const bool use_even = (op ? RNA_boolean_get(op->ptr, "use_even") : false);
      const bool flipped = (op ? RNA_boolean_get(op->ptr, "flipped") : false);
      const bool use_clamp = (op ? RNA_boolean_get(op->ptr, "use_clamp") : true);
      if (mode == TFM_EDGE_SLIDE) {
        const bool use_double_side = (op ? !RNA_boolean_get(op->ptr, "single_side") : true);
        initEdgeSlide_ex(t, use_double_side, use_even, flipped, use_clamp);
      }
      else {
        initVertSlide_ex(t, use_even, flipped, use_clamp);
      }
      break;
    }
    case TFM_BONE_ROLL:
      initBoneRoll(t);
      break;
    case TFM_TIME_TRANSLATE:
      initTimeTranslate(t);
      break;
    case TFM_TIME_SLIDE:
      initTimeSlide(t);
      break;
    case TFM_TIME_SCALE:
      initTimeScale(t);
      break;
    case TFM_TIME_DUPLICATE:
      /* same as TFM_TIME_EXTEND, but we need the mode info for later
       * so that duplicate-culling will work properly
       */
      if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
        initTranslation(t);
      }
      else {
        initTimeTranslate(t);
      }
      break;
    case TFM_TIME_EXTEND:
      /* now that transdata has been made, do like for TFM_TIME_TRANSLATE (for most Animation
       * Editors because they have only 1D transforms for time values) or TFM_TRANSLATION
       * (for Graph/NLA Editors only since they uses 'standard' transforms to get 2D movement)
       * depending on which editor this was called from
       */
      if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
        initTranslation(t);
      }
      else {
        initTimeTranslate(t);
      }
      break;
    case TFM_BAKE_TIME:
      initBakeTime(t);
      break;
    case TFM_MIRROR:
      initMirror(t);
      break;
    case TFM_BWEIGHT:
      initBevelWeight(t);
      break;
    case TFM_ALIGN:
      initAlign(t);
      break;
    case TFM_SEQ_SLIDE:
      initSeqSlide(t);
      break;
    case TFM_NORMAL_ROTATION:
      initNormalRotation(t);
      break;
    case TFM_GPENCIL_OPACITY:
      initGPOpacity(t);
      break;
  }

  /* TODO(germano): Some of these operations change the `t->mode`.
   * This can be bad for Redo.
   * BLI_assert(t->mode == mode); */
}

/** \} */
