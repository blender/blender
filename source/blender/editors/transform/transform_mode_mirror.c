/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "BLI_math.h"
#include "BLI_math_bits.h"
#include "BLI_string.h"

#include "BKE_armature.h"
#include "BKE_context.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"

#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/** \name Transform (Mirror)
 * \{ */

/**
 * Mirrors an object by negating the scale of the object on the mirror axis, reflecting the
 * location and adjusting the rotation.
 *
 * \param axis: Either the axis to mirror on (0 = x, 1 = y, 2 = z) in transform space or -1 for no
 * axis mirror.
 * \param flip: If true, a mirror on all axis will be performed additionally (point
 * reflection).
 */
static void ElementMirror(TransInfo *t, TransDataContainer *tc, TransData *td, int axis, bool flip)
{
  if ((t->flag & T_V3D_ALIGN) == 0 && td->ext) {
    /* Size checked needed since the 3D cursor only uses rotation fields. */
    if (td->ext->size) {
      float fsize[] = {1.0, 1.0, 1.0};

      if (axis >= 0) {
        fsize[axis] = -fsize[axis];
      }
      if (flip) {
        negate_v3(fsize);
      }

      protectedSizeBits(td->protectflag, fsize);

      mul_v3_v3v3(td->ext->size, td->ext->isize, fsize);

      constraintSizeLim(t, td);
    }

    float rmat[3][3];
    if (axis >= 0) {
      float imat[3][3];
      mul_m3_m3m3(rmat, t->spacemtx_inv, td->axismtx);
      rmat[axis][0] = -rmat[axis][0];
      rmat[axis][1] = -rmat[axis][1];
      rmat[axis][2] = -rmat[axis][2];
      rmat[0][axis] = -rmat[0][axis];
      rmat[1][axis] = -rmat[1][axis];
      rmat[2][axis] = -rmat[2][axis];
      invert_m3_m3(imat, td->axismtx);
      mul_m3_m3m3(rmat, rmat, imat);
      mul_m3_m3m3(rmat, t->spacemtx, rmat);

      ElementRotation_ex(t, tc, td, rmat, td->center);

      if (td->ext->rotAngle) {
        *td->ext->rotAngle = -td->ext->irotAngle;
      }
    }
    else {
      unit_m3(rmat);
      ElementRotation_ex(t, tc, td, rmat, td->center);

      if (td->ext->rotAngle) {
        *td->ext->rotAngle = td->ext->irotAngle;
      }
    }
  }

  if ((td->flag & TD_NO_LOC) == 0) {
    float center[3], vec[3];

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

    /* For individual element center, Editmode need to use iloc. */
    if (t->flag & T_POINTS) {
      sub_v3_v3v3(vec, td->iloc, center);
    }
    else {
      sub_v3_v3v3(vec, td->center, center);
    }

    if (axis >= 0) {
      /* Always do the mirror in global space. */
      if (t->flag & T_EDIT) {
        mul_m3_v3(td->mtx, vec);
      }
      reflect_v3_v3v3(vec, vec, t->spacemtx[axis]);
      if (t->flag & T_EDIT) {
        mul_m3_v3(td->smtx, vec);
      }
    }
    if (flip) {
      negate_v3(vec);
    }

    add_v3_v3(vec, center);
    if (t->flag & T_POINTS) {
      sub_v3_v3(vec, td->iloc);
    }
    else {
      sub_v3_v3(vec, td->center);
    }

    if (t->options & (CTX_OBJECT | CTX_POSE_BONE)) {
      mul_m3_v3(td->smtx, vec);
    }

    protectedTransBits(td->protectflag, vec);
    if (td->loc) {
      add_v3_v3v3(td->loc, td->iloc, vec);
    }

    constraintTransLim(t, td);
  }
}

static void applyMirror(TransInfo *t, const int UNUSED(mval[2]))
{
  int i;
  char str[UI_MAX_DRAW_STR];
  copy_v3_v3(t->values_final, t->values);

  /* OPTIMIZATION:
   * This still recalculates transformation on mouse move
   * while it should only recalculate on constraint change. */

  /* if an axis has been selected */
  if (t->con.mode & CON_APPLY) {
    /* #special_axis is either the constraint plane normal or the constraint axis.
     * Assuming that CON_AXIS0 < CON_AXIS1 < CON_AXIS2 and CON_AXIS2 is CON_AXIS0 << 2 */
    BLI_assert(CON_AXIS2 == CON_AXIS0 << 2);
    int axis_bitmap = (t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2)) / CON_AXIS0;
    int special_axis_bitmap = 0;
    int special_axis = -1;
    int bitmap_len = count_bits_i(axis_bitmap);
    if (LIKELY(!ELEM(bitmap_len, 0, 3))) {
      special_axis_bitmap = (bitmap_len == 2) ? ~axis_bitmap : axis_bitmap;
      special_axis = bitscan_forward_i(special_axis_bitmap);
    }

    SNPRINTF(str, TIP_("Mirror%s"), t->con.text);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementMirror(t, tc, td, special_axis, bitmap_len >= 2);
      }
    }

    recalcData(t);

    ED_area_status_text(t->area, str);
  }
  else {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementMirror(t, tc, td, -1, false);
      }
    }

    recalcData(t);

    if (t->flag & T_2D_EDIT) {
      ED_area_status_text(t->area, TIP_("Select a mirror axis (X, Y)"));
    }
    else {
      ED_area_status_text(t->area, TIP_("Select a mirror axis (X, Y, Z)"));
    }
  }
}

void initMirror(TransInfo *t)
{
  t->transform = applyMirror;
  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  t->flag |= T_NULL_ONE;
}

/** \} */
