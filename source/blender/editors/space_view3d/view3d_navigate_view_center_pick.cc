/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BLT_translation.hh"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.h"
#include "BKE_screen.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_mesh.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_resources.hh"

#include "view3d_intern.h"

#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Center Pick Operator
 * \{ */

static int viewcenter_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *region = CTX_wm_region(C);

  if (rv3d) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    float new_ofs[3];
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, region);

    view3d_operator_needs_opengl(C);

    if (ED_view3d_autodist(depsgraph, region, v3d, event->mval, new_ofs, false, nullptr)) {
      /* pass */
    }
    else {
      /* fallback to simple pan */
      negate_v3_v3(new_ofs, rv3d->ofs);
      ED_view3d_win_to_3d_int(v3d, region, new_ofs, event->mval, new_ofs);
    }
    negate_v3(new_ofs);

    V3D_SmoothParams sview = {nullptr};
    sview.ofs = new_ofs;
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

  /* api callbacks */
  ot->invoke = viewcenter_pick_invoke;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */
