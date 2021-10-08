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
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_snap.h"

#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/** \name Transform (Shrink-Fatten) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_ShrinkFatten {
  const TransInfo *t;
  const TransDataContainer *tc;
  float distance;
};

static void transdata_elem_shrink_fatten(const TransInfo *t,
                                         const TransDataContainer *UNUSED(tc),
                                         TransData *td,
                                         const float distance)
{
  /* Get the final offset. */
  float tdistance = distance * td->factor;
  if (td->ext && (t->flag & T_ALT_TRANSFORM) != 0) {
    tdistance *= td->ext->isize[0]; /* shell factor */
  }

  madd_v3_v3v3fl(td->loc, td->iloc, td->axismtx[2], tdistance);
}

static void transdata_elem_shrink_fatten_fn(void *__restrict iter_data_v,
                                            const int iter,
                                            const TaskParallelTLS *__restrict UNUSED(tls))
{
  struct TransDataArgs_ShrinkFatten *data = iter_data_v;
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_shrink_fatten(data->t, data->tc, td, data->distance);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Shrink-Fatten)
 * \{ */

static eRedrawFlag shrinkfatten_handleEvent(struct TransInfo *t, const wmEvent *event)
{
  BLI_assert(t->mode == TFM_SHRINKFATTEN);
  const wmKeyMapItem *kmi = t->custom.mode.data;
  if (kmi && event->type == kmi->type && event->val == kmi->val) {
    /* Allows the "Even Thickness" effect to be enabled as a toggle. */
    t->flag ^= T_ALT_TRANSFORM;
    return TREDRAW_HARD;
  }
  return TREDRAW_NOTHING;
}

static void applyShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
  float distance;
  int i;
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  UnitSettings *unit = &t->scene->unit;

  distance = t->values[0];

  transform_snap_increment(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values_final[0] = distance;

  /* header print for NumInput */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Shrink/Fatten: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, unit);
    ofs += BLI_snprintf_rlen(str + ofs, sizeof(str) - ofs, "%s", c);
  }
  else {
    /* default header print */
    if (unit != NULL) {
      ofs += BKE_unit_value_as_string(str + ofs,
                                      sizeof(str) - ofs,
                                      distance * unit->scale_length,
                                      4,
                                      B_UNIT_LENGTH,
                                      unit,
                                      true);
    }
    else {
      ofs += BLI_snprintf_rlen(str + ofs, sizeof(str) - ofs, "%.4f", distance);
    }
  }

  if (t->proptext[0]) {
    ofs += BLI_snprintf_rlen(str + ofs, sizeof(str) - ofs, " %s", t->proptext);
  }
  ofs += BLI_strncpy_rlen(str + ofs, ", (", sizeof(str) - ofs);

  const wmKeyMapItem *kmi = t->custom.mode.data;
  if (kmi) {
    ofs += WM_keymap_item_to_string(kmi, false, str + ofs, sizeof(str) - ofs);
  }

  BLI_snprintf(str + ofs,
               sizeof(str) - ofs,
               TIP_(" or Alt) Even Thickness %s"),
               WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
  /* done with header string */

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_shrink_fatten(t, tc, td, distance);
      }
    }
    else {
      struct TransDataArgs_ShrinkFatten data = {
          .t = t,
          .tc = tc,
          .distance = distance,
      };
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_shrink_fatten_fn, &settings);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initShrinkFatten(TransInfo *t)
{
  /* If not in mesh edit mode, fallback to Resize. */
  if ((t->flag & T_EDIT) == 0 || (t->obedit_type != OB_MESH)) {
    float no_mouse_dir_constraint[3];
    zero_v3(no_mouse_dir_constraint);
    initResize(t, no_mouse_dir_constraint);
  }
  else {
    t->mode = TFM_SHRINKFATTEN;
    t->transform = applyShrinkFatten;
    t->handleEvent = shrinkfatten_handleEvent;

    initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

    t->idx_max = 0;
    t->num.idx_max = 0;
    t->snap[0] = 1.0f;
    t->snap[1] = t->snap[0] * 0.1f;

    copy_v3_fl(t->num.val_inc, t->snap[0]);
    t->num.unit_sys = t->scene->unit.system;
    t->num.unit_type[0] = B_UNIT_LENGTH;

    t->flag |= T_NO_CONSTRAINT;

    if (t->keymap) {
      /* Workaround to use the same key as the modal keymap. */
      t->custom.mode.data = (void *)WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
    }
  }
}
/** \} */
