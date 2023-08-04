/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_unit.h"

#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

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
                                         const TransDataContainer * /*tc*/,
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
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  TransDataArgs_ShrinkFatten *data = static_cast<TransDataArgs_ShrinkFatten *>(iter_data_v);
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

static eRedrawFlag shrinkfatten_handleEvent(TransInfo *t, const wmEvent *event)
{
  BLI_assert(t->mode == TFM_SHRINKFATTEN);
  const wmKeyMapItem *kmi = static_cast<const wmKeyMapItem *>(t->custom.mode.data);
  if (kmi && event->type == kmi->type && event->val == kmi->val) {
    /* Allows the "Even Thickness" effect to be enabled as a toggle. */
    t->flag ^= T_ALT_TRANSFORM;
    return TREDRAW_HARD;
  }
  return TREDRAW_NOTHING;
}

static void applyShrinkFatten(TransInfo *t)
{
  float distance;
  int i;
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  UnitSettings *unit = &t->scene->unit;

  distance = t->values[0] + t->values_modal_offset[0];

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
    if (unit != nullptr) {
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

  const wmKeyMapItem *kmi = static_cast<const wmKeyMapItem *>(t->custom.mode.data);
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
      TransDataArgs_ShrinkFatten data{};
      data.t = t;
      data.tc = tc;
      data.distance = distance;
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_shrink_fatten_fn, &settings);
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initShrinkFatten(TransInfo *t, wmOperator * /*op*/)
{
  if ((t->flag & T_EDIT) == 0 || (t->obedit_type != OB_MESH)) {
    BKE_report(t->reports, RPT_ERROR, "'Shrink/Fatten' meshes is only supported in edit mode");
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_SHRINKFATTEN;

  initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 1.0f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_LENGTH;

  if (t->keymap) {
    /* Workaround to use the same key as the modal keymap. */
    t->custom.mode.data = (void *)WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
  }
}

/** \} */

TransModeInfo TransMode_shrinkfatten = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initShrinkFatten,
    /*transform_fn*/ applyShrinkFatten,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ shrinkfatten_handleEvent,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
