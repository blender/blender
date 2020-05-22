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

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/* Transform (Mirror) */

/** \name Transform Mirror
 * \{ */

static void applyMirror(TransInfo *t, const int UNUSED(mval[2]))
{
  float size[3], mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];
  copy_v3_v3(t->values_final, t->values);

  /* OPTIMIZATION:
   * This still recalculates transformation on mouse move
   * while it should only recalculate on constraint change. */

  /* if an axis has been selected */
  if (t->con.mode & CON_APPLY) {
    size[0] = size[1] = size[2] = -1;

    size_to_mat3(mat, size);

    if (t->con.applySize) {
      t->con.applySize(t, NULL, NULL, mat);
    }

    BLI_snprintf(str, sizeof(str), TIP_("Mirror%s"), t->con.text);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementResize(t, tc, td, mat);
      }
    }

    recalcData(t);

    ED_area_status_text(t->area, str);
  }
  else {
    size[0] = size[1] = size[2] = 1;

    size_to_mat3(mat, size);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementResize(t, tc, td, mat);
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
  if ((t->flag & T_EDIT) == 0) {
    t->flag |= T_NO_ZERO;
  }
}
/** \} */
