/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

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

  ofs += BLI_snprintf_rlen(
      str + ofs, UI_MAX_DRAW_STR - ofs, IFACE_("Sequence Slide: %s%s"), &tvec[0], t->con.text);
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

static void applySeqSlide(TransInfo *t)
{
  char str[UI_MAX_DRAW_STR];
  float values_final[3] = {0.0f};

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
  else {
    copy_v2_v2(values_final, t->values);
    transform_snap_mixed_apply(t, values_final);
    transform_convert_sequencer_channel_clamp(t, values_final);

    if (t->con.mode & CON_APPLY) {
      t->con.applyVec(t, nullptr, nullptr, values_final, values_final);
    }
  }

  values_final[0] = floorf(values_final[0] + 0.5f);
  values_final[1] = floorf(values_final[1] + 0.5f);
  copy_v2_v2(t->values_final, values_final);

  headerSeqSlide(t, t->values_final, str);
  applySeqSlideValue(t, t->values_final);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

struct SeqSlideParams {
  bool use_restore_handle_selection;
};

static void initSeqSlide(TransInfo *t, wmOperator *op)
{
  SeqSlideParams *ssp = MEM_cnew<SeqSlideParams>(__func__);
  t->custom.mode.data = ssp;
  t->custom.mode.use_free = true;
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_restore_handle_selection");
  if (op != nullptr && prop != nullptr) {
    ssp->use_restore_handle_selection = RNA_property_boolean_get(op->ptr, prop);
  }

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

bool transform_mode_edge_seq_slide_use_restore_handle_selection(const TransInfo *t)
{
  if ((U.sequencer_editor_flag & USER_SEQ_ED_SIMPLE_TWEAKING) == 0) {
    return false;
  }
  SeqSlideParams *ssp = static_cast<SeqSlideParams *>(t->custom.mode.data);
  if (ssp == nullptr) {
    return false;
  }
  return ssp->use_restore_handle_selection;
}

/** \} */

TransModeInfo TransMode_seqslide = {
    /*flags*/ 0,
    /*init_fn*/ initSeqSlide,
    /*transform_fn*/ applySeqSlide,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ transform_snap_sequencer_apply_translate,
    /*draw_fn*/ nullptr,
};
