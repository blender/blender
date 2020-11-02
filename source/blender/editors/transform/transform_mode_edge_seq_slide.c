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
#include "BKE_unit.h"

#include "ED_screen.h"

#include "WM_api.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/** \name Transform (Sequencer Slide)
 * \{ */

static void headerSeqSlide(TransInfo *t, const float val[2], char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.0f, %.0f", val[0], val[1]);
  }

  ofs += BLI_snprintf(
      str + ofs, UI_MAX_DRAW_STR - ofs, TIP_("Sequence Slide: %s%s, ("), &tvec[0], t->con.text);

  if (t->keymap) {
    wmKeyMapItem *kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_TRANSLATE);
    if (kmi) {
      ofs += WM_keymap_item_to_string(kmi, false, str + ofs, UI_MAX_DRAW_STR - ofs);
    }
  }
  ofs += BLI_snprintf(str + ofs,
                      UI_MAX_DRAW_STR - ofs,
                      TIP_(" or Alt) Expand to fit %s"),
                      WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
}

static void applySeqSlideValue(TransInfo *t, const float val[2])
{
  int i;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      madd_v2_v2v2fl(td->loc, td->iloc, val, td->factor);
    }
  }
}

static void applySeqSlide(TransInfo *t, const int mval[2])
{
  char str[UI_MAX_DRAW_STR];
  float values_final[2] = {0.0f};

  snapSequenceBounds(t, mval);
  if (applyNumInput(&t->num, values_final)) {
    if (t->con.mode & CON_APPLY) {
      if (t->con.mode & CON_AXIS0) {
        mul_v2_v2fl(values_final, t->spacemtx[0], values_final[0]);
      }
      else {
        mul_v2_v2fl(values_final, t->spacemtx[1], values_final[0]);
      }
    }
  }
  else if (t->con.mode & CON_APPLY) {
    t->con.applyVec(t, NULL, NULL, t->values, values_final);
  }
  else {
    copy_v2_v2(values_final, t->values);
  }

  values_final[0] = floorf(values_final[0] + 0.5f);
  values_final[1] = floorf(values_final[1] + 0.5f);
  copy_v2_v2(t->values_final, values_final);

  headerSeqSlide(t, t->values_final, str);
  applySeqSlideValue(t, t->values_final);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initSeqSlide(TransInfo *t)
{
  t->transform = applySeqSlide;

  initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

  t->idx_max = 1;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  t->snap[0] = floorf(t->scene->r.frs_sec / t->scene->r.frs_sec_base);
  t->snap[1] = 10.0f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  /* Would be nice to have a time handling in units as well
   * (supporting frames in addition to "natural" time...). */
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
}
/** \} */
