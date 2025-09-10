/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_bits.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface_types.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Curve Shrink/Fatten)
 * \{ */

static void applyCurveShrinkFatten(TransInfo *t)
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values_final[0] = ratio;

  /* Header print for NumInput. */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, t->scene->unit);
    SNPRINTF_UTF8(str, IFACE_("Shrink/Fatten: %s"), c);
  }
  else {
    SNPRINTF_UTF8(str, IFACE_("Shrink/Fatten: %3f"), ratio);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        if (td->ival == 0.0f && ratio > 1.0f) {
          /* Allow Shrink/Fatten for zero radius. */
          *td->val = (ratio - 1.0f) * uint_as_float(POINTER_AS_UINT(t->custom.mode.data));
        }
        else {
          *td->val = td->ival * ratio;
        }

        /* Apply proportional editing. */
        *td->val = interpf(*td->val, td->ival, td->factor);
        CLAMP_MIN(*td->val, 0.0f);
      }
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initCurveShrinkFatten(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_CURVE_SHRINKFATTEN;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->increment[0] = 0.1f;
  t->increment_precision = 0.1f;

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  float scale_factor = 0.0f;
  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    /* For cases where only one point on the curve is being transformed and the radius of that
     * point is zero [that is actually only checked for in #applyCurveShrinkFatten()], use the
     * factor to multiply the offset of the ratio and allow scaling. Note that for bezier curves, 3
     * TransData equals 1 point in most cases. Handles (as opposed to control points) have their
     * #TransData.val set to nullptr (set in #createTransCurveVerts() /
     * #curve_populate_trans_data_structs() since we only want to apply values to _control points_
     * [this is again checked for in #applyCurveShrinkFatten()]. Only calculate the scale_factor if
     * we are working on a control point. */
    bool use_scaling_factor = false;
    if (t->data_len_all == 1) {
      /* Either a single control point of a non-bezier curve or single handle of a bezier curve
       * selected. */
      use_scaling_factor = TRANS_DATA_CONTAINER_FIRST_OK(t)->data[0].val != nullptr;
    }
    if (t->data_len_all == 3) {
      /* Either a single control point of a bezier curve (or its handles as well) selected, also
       * true for three individual handles selected. Note the layout/order of TransData is
       * different for Curve vs. Curves (Curves have the control point first, Curve has it in the
       * middle), so check this explicitly. */
      TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
      TransData td_0 = tc->data[0];
      TransData td_1 = tc->data[1];
      TransData td_2 = tc->data[2];

      if (t->data_type == &TransConvertType_Curve) {
        use_scaling_factor = td_0.val == nullptr && td_1.val != nullptr && td_2.val == nullptr;
      }
      else if (ELEM(t->data_type,
                    &curves::TransConvertType_Curves,
                    &greasepencil::TransConvertType_GreasePencil))
      {
        use_scaling_factor = td_0.val != nullptr && td_1.val == nullptr && td_2.val == nullptr;
      }
    }

    if (use_scaling_factor) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
      scale_factor = rv3d->pixsize * t->mouse.factor * t->zfac;
    }
  }
  t->custom.mode.data = POINTER_FROM_UINT(float_as_uint(scale_factor));
}

/** \} */

TransModeInfo TransMode_curveshrinkfatten = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initCurveShrinkFatten,
    /*transform_fn*/ applyCurveShrinkFatten,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};

}  // namespace blender::ed::transform
