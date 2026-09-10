/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_math_vector_c.hh"
#include "BLI_string_utf8.hh"

#include "BKE_context.hh"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_types.hh"

#include "BLT_translation.hh"

#include "ED_sequencer.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Sequencer Slide)
 * \{ */

struct SeqSlideParams {
  wmOperator *op;
  bool use_restore_handle_selection;
};

static void headerSeqSlide(TransInfo *t, const float val[2], char str[UI_MAX_DRAW_STR])
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  char tvec[NUM_STR_REP_LEN * 3];
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, scene->unit);
  }
  else {
    BLI_snprintf_utf8(&tvec[0], NUM_STR_REP_LEN, "%.0f, %.0f", val[0], val[1]);
  }

  ofs += BLI_snprintf_utf8_rlen(
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
  float values_final[3] = {0.0f}, values_clamped[3] = {0.0f};

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

    if (t->con.mode & CON_APPLY) {
      t->con.applyVec(t, nullptr, nullptr, values_final, values_final);
    }
  }

  values_final[0] = round_fl_to_int(values_final[0]);
  values_final[1] = round_fl_to_int(values_final[1]);

  copy_v2_v2(values_clamped, values_final);
  transform_convert_sequencer_clamp(t, values_clamped);
  headerSeqSlide(t, values_clamped, str);

  copy_v2_v2(t->values_final, values_final);
  applySeqSlideValue(t, t->values_final);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void seq_slide_status(TransInfo *t)
{
  SeqSlideParams *ssp = static_cast<SeqSlideParams *>(t->custom.mode.data);
  wmOperator *op = ssp->op;
  if (!op || t->data_container_len == 0) {
    return;
  }

  const TransSeq *ts = static_cast<const TransSeq *>(
      TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data);

  WorkspaceStatus status(t->context);
  status.opmodal(IFACE_("Confirm"), op->type, TFM_MODAL_CONFIRM);
  status.opmodal(IFACE_("Cancel"), op->type, TFM_MODAL_CANCEL);
  status.opmodal(IFACE_("Snap"), op->type, TFM_MODAL_SNAP_TOGGLE, t->modifiers & MOD_SNAP);
  status.opmodal(
      IFACE_("Snap Invert"), op->type, TFM_MODAL_SNAP_INV_ON, t->modifiers & MOD_SNAP_INVERT);
  status.opmodal(IFACE_("Precision"), op->type, TFM_MODAL_PRECISION, t->modifiers & MOD_PRECISION);

  const bool has_constraint = (t->con.mode & CON_APPLY) != 0;
  if ((t->flag & T_NO_CONSTRAINT) == 0) {
    status.opmodal({}, op->type, TFM_MODAL_AXIS_X, has_constraint && (t->con.mode & CON_AXIS0));
    status.opmodal(
        IFACE_("Axis"), op->type, TFM_MODAL_AXIS_Y, has_constraint && (t->con.mode & CON_AXIS1));
  }

  const bool y_locked = ts->offset_clamp.ymin == 0 && ts->offset_clamp.ymax == 0;
  const bool handles_selected = y_locked && t->data_type == &TransConvertType_Sequencer;
  if (has_constraint) {
    status.opmodal(IFACE_("Clear Constraints"), op->type, TFM_MODAL_CONS_OFF);
  }
  else if (handles_selected) {
    status.opmodal(IFACE_("Clamp Handles"),
                   op->type,
                   TFM_MODAL_STRIP_CLAMP,
                   t->modifiers & MOD_STRIP_CLAMP_HOLDS);
  }
}

static void initSeqSlide(TransInfo *t, wmOperator *op)
{
  SeqSlideParams *ssp = MEM_new_zeroed<SeqSlideParams>(__func__);
  t->custom.mode.data = ssp;
  t->custom.mode.use_free = true;
  ssp->op = op;
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_restore_handle_selection");
  if (op != nullptr && prop != nullptr) {
    ssp->use_restore_handle_selection = RNA_property_boolean_get(op->ptr, prop);
  }

  Scene *scene = CTX_data_sequencer_scene(t->context);

  initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

  t->idx_max = 1;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  t->increment = float3(floorf(scene->r.frs_sec / scene->r.frs_sec_base));
  t->increment_precision = 10.0f / t->increment[0];

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = scene->unit.system;
  /* Would be nice to have a time handling in units as well
   * (supporting frames in addition to "natural" time...). */
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
}

bool transform_mode_edge_seq_slide_use_restore_handle_selection(const TransInfo *t)
{
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
    /*snap_apply_fn*/ snap_sequencer_apply_seqslide,
    /*draw_fn*/ nullptr,
    /*status_fn*/ seq_slide_status,
};

}  // namespace blender::ed::transform
