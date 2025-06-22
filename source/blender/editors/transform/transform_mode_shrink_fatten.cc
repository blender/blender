/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>
#include <fmt/format.h>

#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_report.hh"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Shrink-Fatten)
 * \{ */

enum eShrinkFattenMode {
  EVEN_THICKNESS_OFF = 0,
  EVEN_THICKNESS_ON = 1,
};

/**
 * Custom data, stored in #TransInfo.custom.mode.data
 */
struct ShrinkFattenCustomData {
  const wmKeyMapItem *kmi;
  eShrinkFattenMode mode;
  wmOperator *op;
  bool use_alt_press_to_disable;
};

static void transdata_elem_shrink_fatten(const TransInfo *t,
                                         const TransDataContainer * /*tc*/,
                                         TransData *td,
                                         TransDataExtension *td_ext,
                                         const float distance)
{
  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(t->custom.mode.data);

  /* Get the final offset. */
  float tdistance = distance * td->factor;
  if (td_ext && custom_data->mode == EVEN_THICKNESS_ON) {
    tdistance *= td_ext->iscale[0]; /* Shell factor. */
  }

  madd_v3_v3v3fl(td->loc, td->iloc, td->axismtx[2], tdistance);
}

static eRedrawFlag shrinkfatten_handleEvent(TransInfo *t, const wmEvent *event)
{
  BLI_assert(t->mode == TFM_SHRINKFATTEN);
  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(t->custom.mode.data);
  const wmKeyMapItem *kmi = custom_data->kmi;

  if (ELEM(event->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY)) {
    bool use_even_thickness = custom_data->use_alt_press_to_disable != (event->val == KM_PRESS);
    custom_data->mode = use_even_thickness ? EVEN_THICKNESS_ON : EVEN_THICKNESS_OFF;
    return TREDRAW_HARD;
  }
  else if (kmi && event->type == kmi->type && event->val == kmi->val) {
    /* Allows the "Even Thickness" effect to be enabled as a toggle. */
    custom_data->mode = custom_data->mode == EVEN_THICKNESS_ON ? EVEN_THICKNESS_OFF :
                                                                 EVEN_THICKNESS_ON;

    /* Also toggle the Alt press state. */
    custom_data->use_alt_press_to_disable = !custom_data->use_alt_press_to_disable;
    return TREDRAW_HARD;
  }
  return TREDRAW_NOTHING;
}

static void applyShrinkFatten(TransInfo *t)
{
  float distance;
  fmt::memory_buffer str;
  const UnitSettings &unit = t->scene->unit;
  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(t->custom.mode.data);

  distance = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values_final[0] = distance;

  /* Header print for NumInput. */
  fmt::format_to(fmt::appender(str), "{}", IFACE_("Shrink/Fatten: "));
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, unit);
    fmt::format_to(fmt::appender(str), "{}", c);
  }
  else {
    /* Default header print. */
    if (unit.system != USER_UNIT_NONE) {
      char unit_str[64];
      BKE_unit_value_as_string_scaled(
          unit_str, sizeof(unit_str), distance, -4, B_UNIT_LENGTH, unit, true);
      fmt::format_to(fmt::appender(str), "{}", unit_str);
    }
    else {
      fmt::format_to(fmt::appender(str), "{:.4f}", distance);
    }
  }

  if (t->proptext[0]) {
    fmt::format_to(fmt::appender(str), " {}", t->proptext);
  }

  /* Done with header string. */

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        TransData *td = &tc->data[i];
        TransDataExtension *td_ext = tc->data_ext ? &tc->data_ext[i] : nullptr;
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_shrink_fatten(t, tc, td, td_ext, distance);
      }
    });
  }

  recalc_data(t);

  ED_area_status_text(t->area, fmt::to_string(str).c_str());

  if (custom_data->op) {
    WorkspaceStatus status(t->context);

    status.opmodal(IFACE_("Confirm"), custom_data->op->type, TFM_MODAL_CONFIRM);
    status.opmodal(IFACE_("Cancel"), custom_data->op->type, TFM_MODAL_CANCEL);
    status.opmodal(
        IFACE_("Snap"), custom_data->op->type, TFM_MODAL_SNAP_TOGGLE, t->modifiers & MOD_SNAP);
    status.opmodal(IFACE_("Snap Invert"),
                   custom_data->op->type,
                   TFM_MODAL_SNAP_INV_ON,
                   t->modifiers & MOD_SNAP_INVERT);
    status.opmodal(IFACE_("Precision"),
                   custom_data->op->type,
                   TFM_MODAL_PRECISION,
                   t->modifiers & MOD_PRECISION);
    status.opmodal(IFACE_("Even Thickness"),
                   custom_data->op->type,
                   TFM_MODAL_RESIZE,
                   custom_data->mode == EVEN_THICKNESS_ON);
    status.item(IFACE_("Even Thickness Invert"), ICON_EVENT_ALT);

    if (t->proptext[0]) {
      status.opmodal({}, custom_data->op->type, TFM_MODAL_PROPSIZE_UP);
      status.opmodal(IFACE_("Proportional Size"), custom_data->op->type, TFM_MODAL_PROPSIZE_DOWN);
    }
  }
}

static void initShrinkFatten(TransInfo *t, wmOperator *op)
{
  if ((t->flag & T_EDIT) == 0 || (t->obedit_type != OB_MESH)) {
    BKE_report(t->reports, RPT_ERROR, "'Shrink/Fatten' meshes is only supported in edit mode");
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_SHRINKFATTEN;

  initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->increment[0] = 1.0f;
  t->increment_precision = 0.1f;

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_LENGTH;

  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(
      MEM_callocN(sizeof(*custom_data), __func__));
  t->custom.mode.data = custom_data;
  t->custom.mode.free_cb = [](TransInfo *t, TransDataContainer *, TransCustomData *custom_data) {
    ShrinkFattenCustomData *data = static_cast<ShrinkFattenCustomData *>(custom_data->data);

    /* WORKAROUND: Use #T_ALT_TRANSFORM to indicate the value of the "use_even_offset" property in
     * `saveTransform`. */
    SET_FLAG_FROM_TEST(t->flag, data->mode == EVEN_THICKNESS_ON, T_ALT_TRANSFORM);
    MEM_freeN(data);
    custom_data->data = nullptr;
  };

  if (t->keymap) {
    /* Workaround to use the same key as the modal keymap. */
    custom_data->kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
  }

  if (op) {
    custom_data->op = op;
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_even_offset");
    if (RNA_property_is_set(op->ptr, prop) && RNA_property_boolean_get(op->ptr, prop)) {
      /* TODO: Check if the Alt button is already pressed. */
      custom_data->mode = EVEN_THICKNESS_ON;
      custom_data->use_alt_press_to_disable = true;
    }
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

}  // namespace blender::ed::transform
