/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BKE_context.hh"

#include "ED_screen.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Align)
 * \{ */

static void applyAlign(TransInfo *t)
{
  float center[3];
  int i;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* saving original center */
    copy_v3_v3(center, tc->center_local);
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float mat[3][3], invmat[3][3];

      if (td->flag & TD_SKIP) {
        continue;
      }

      /* around local centers */
      if (t->options & (CTX_OBJECT | CTX_POSE_BONE)) {
        copy_v3_v3(tc->center_local, td->center);
      }
      else {
        if (t->settings->selectmode & SCE_SELECT_FACE) {
          copy_v3_v3(tc->center_local, td->center);
        }
      }

      invert_m3_m3(invmat, td->axismtx);

      mul_m3_m3m3(mat, t->spacemtx, invmat);

      ElementRotation(t, tc, td, mat, t->around);
    }
    /* restoring original center */
    copy_v3_v3(tc->center_local, center);
  }

  recalc_data(t);

  ED_area_status_text(t->area, IFACE_("Align"));
}

static void initAlign(TransInfo *t, wmOperator * /*op*/)
{
  initMouseInputMode(t, &t->mouse, INPUT_NONE);
}

/** \} */

TransModeInfo TransMode_align = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initAlign,
    /*transform_fn*/ applyAlign,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
