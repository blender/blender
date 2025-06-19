/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math_vector.h"

#include "BKE_context.hh"

#include "WM_api.hh"

#include "view3d_intern.hh"

#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Center Pick Operator
 * \{ */

static wmOperatorStatus viewcenter_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *region = CTX_wm_region(C);

  if (rv3d) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    float ofs_new[3];
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, region);

    view3d_operator_needs_gpu(C);

    /* Ensure the depth buffer is updated for #ED_view3d_autodist. */
    ED_view3d_depth_override(depsgraph, region, v3d, nullptr, V3D_DEPTH_NO_GPENCIL, true, nullptr);

    if (ED_view3d_autodist(region, v3d, event->mval, ofs_new, nullptr)) {
      /* pass */
    }
    else {
      /* fall back to simple pan */
      negate_v3_v3(ofs_new, rv3d->ofs);
      ED_view3d_win_to_3d_int(v3d, region, ofs_new, event->mval, ofs_new);
    }
    negate_v3(ofs_new);

    V3D_SmoothParams sview = {nullptr};
    sview.ofs = ofs_new;
    sview.undo_str = op->type->name;

    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Mouse";
  ot->description = "Center the view to the Z-depth position under the mouse cursor";
  ot->idname = "VIEW3D_OT_view_center_pick";

  /* API callbacks. */
  ot->invoke = viewcenter_pick_invoke;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */
