/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>
#include <fmt/format.h>

#include "BLI_math_vector_c.hh"
#include "BLI_string_utils.hh"
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
  bool use_alt_press_to_disable;
};

static const float *shrinkfatten_shell_factor_ensure(TransDataContainer *tc)
{
  if (tc->custom.mode.data == nullptr) {
    tc->custom.mode.data = transform_convert_mesh_shell_factor_calc(tc);
    tc->custom.mode.use_free = true;
  }
  return static_cast<const float *>(tc->custom.mode.data);
}

static void transdata_elem_shrink_fatten(TransData *td, const float distance)
{
  /* Get the final offset. */
  const float tdistance = distance * td->factor;

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
  if (kmi && event->type == kmi->type && event->val == kmi->val) {
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
      const int precision = t->modifiers & MOD_PRECISION ? 6 : 4;
      BKE_unit_value_as_string_scaled(
          unit_str, distance, precision * -1, B_UNIT_LENGTH, unit, true, true);
      fmt::format_to(fmt::appender(str), "{}", BLI_string_pad_number_sign(unit_str).c_str());
    }
    else {
      fmt::format_to(fmt::appender(str), "{:.4f}", distance);
    }
  }

  if (t->proptext[0]) {
    fmt::format_to(fmt::appender(str), " {}", t->proptext);
  }

  /* Done with header string. */

  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(t->custom.mode.data);
  if (custom_data->mode == EVEN_THICKNESS_ON && t->data_container[0].custom.mode.data == nullptr) {
    restoreTransObjects(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    const float *shell_factors = (custom_data->mode == EVEN_THICKNESS_ON) ?
                                     shrinkfatten_shell_factor_ensure(tc) :
                                     nullptr;
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        TransData *td = &tc->data[i];
        if (td->flag & TD_SKIP) {
          continue;
        }
        float elem_distance = distance;
        if (shell_factors) {
          elem_distance *= shell_factors[i];
        }
        transdata_elem_shrink_fatten(td, elem_distance);
      }
    });
  }

  recalc_data(t);

  ED_area_status_text(t->area, fmt::to_string(str).c_str());
}

static void shrink_fatten_status(TransInfo *t)
{
  if (t->keymap == nullptr) {
    return;
  }
  const wmKeyMap &keymap = *t->keymap;

  ShrinkFattenCustomData *custom_data = static_cast<ShrinkFattenCustomData *>(t->custom.mode.data);
  WorkspaceStatus status(t->context);

  status.modal_keymap(IFACE_("Confirm"), keymap, TFM_MODAL_CONFIRM);
  status.modal_keymap(IFACE_("Cancel"), keymap, TFM_MODAL_CANCEL);
  status.modal_keymap(IFACE_("Snap"), keymap, TFM_MODAL_SNAP_TOGGLE, t->modifiers & MOD_SNAP);
  status.modal_keymap(
      IFACE_("Snap Invert"), keymap, TFM_MODAL_SNAP_INV_ON, t->modifiers & MOD_SNAP_INVERT);
  status.modal_keymap(
      IFACE_("Precision"), keymap, TFM_MODAL_PRECISION, t->modifiers & MOD_PRECISION);
  status.modal_keymap(
      IFACE_("Even Thickness"), keymap, TFM_MODAL_RESIZE, custom_data->mode == EVEN_THICKNESS_ON);
  status.item(IFACE_("Even Thickness Invert"), ICON_EVENT_ALT);

  if (t->proptext[0]) {
    status.modal_keymap({}, keymap, TFM_MODAL_PROPSIZE_UP);
    status.modal_keymap(IFACE_("Proportional Size"), keymap, TFM_MODAL_PROPSIZE_DOWN);
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

  ShrinkFattenCustomData *custom_data = MEM_new_zeroed<ShrinkFattenCustomData>(__func__);
  t->custom.mode.data = custom_data;
  t->custom.mode.free_cb = [](TransInfo *t, TransDataContainer *, TransCustomData *custom_data) {
    ShrinkFattenCustomData *data = static_cast<ShrinkFattenCustomData *>(custom_data->data);

    /* WORKAROUND: Use #T_ALT_TRANSFORM to indicate the value of the "use_even_offset" property in
     * `saveTransform`. */
    SET_FLAG_FROM_TEST(t->flag, data->mode == EVEN_THICKNESS_ON, T_ALT_TRANSFORM);
    MEM_delete(data);
    custom_data->data = nullptr;
  };

  if (t->keymap) {
    /* Workaround to use the same key as the modal keymap. */
    custom_data->kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
  }

  if (op) {
    if (PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_even_offset")) {
      if (RNA_property_is_set(op->ptr, prop) && RNA_property_boolean_get(op->ptr, prop)) {
        /* TODO: Check if the Alt button is already pressed. */
        custom_data->mode = EVEN_THICKNESS_ON;
        custom_data->use_alt_press_to_disable = true;
      }
    }
    else {
      BLI_assert(STREQ(op->idname, "TRANSFORM_OT_transform"));
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
    /*status_fn*/ shrink_fatten_status,
};

}  // namespace blender::ed::transform
