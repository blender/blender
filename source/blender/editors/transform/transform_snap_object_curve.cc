/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.hh"

#include "DNA_curve_types.h"

#include "BKE_bvhutils.h"
#include "BKE_curve.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "ED_transform_snap_object_context.hh"

#include "transform_snap_object.hh"

using blender::float4x4;

eSnapMode snapCurve(SnapObjectContext *sctx, Object *ob_eval, const float4x4 &obmat)
{
  bool has_snap = false;

  /* Only vertex snapping mode (eg control points and handles) supported for now). */
  if ((sctx->runtime.snap_to_flag & SCE_SNAP_TO_POINT) == 0) {
    return SCE_SNAP_TO_NONE;
  }

  Curve *cu = static_cast<Curve *>(ob_eval->data);

  SnapData nearest2d(sctx, obmat);

  const bool use_obedit = BKE_object_is_in_editmode(ob_eval);

  if (use_obedit == false) {
    /* Test BoundBox */
    BoundBox *bb = BKE_curve_boundbox_get(ob_eval);
    if (bb && !nearest2d.snap_boundbox(bb->vec[0], bb->vec[6])) {
      return SCE_SNAP_TO_NONE;
    }
  }

  nearest2d.clip_planes_enable(sctx, true);

  bool skip_selected = (sctx->runtime.params.snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED) !=
                       0;

  LISTBASE_FOREACH (Nurb *, nu, (use_obedit ? &cu->editnurb->nurbs : &cu->nurb)) {
    for (int u = 0; u < nu->pntsu; u++) {
      if (use_obedit) {
        if (nu->bezt) {
          if (nu->bezt[u].hide) {
            /* Skip hidden. */
            continue;
          }

          bool is_selected = (nu->bezt[u].f2 & SELECT) != 0;
          if (is_selected && skip_selected) {
            continue;
          }
          has_snap |= nearest2d.snap_point(nu->bezt[u].vec[1]);

          /* Don't snap if handle is selected (moving),
           * or if it is aligning to a moving handle. */
          bool is_selected_h1 = (nu->bezt[u].f1 & SELECT) != 0;
          bool is_selected_h2 = (nu->bezt[u].f3 & SELECT) != 0;
          bool is_autoalign_h1 = (nu->bezt[u].h1 & HD_ALIGN) != 0;
          bool is_autoalign_h2 = (nu->bezt[u].h2 & HD_ALIGN) != 0;
          if (!skip_selected || !(is_selected_h1 || (is_autoalign_h1 && is_selected_h2))) {
            has_snap |= nearest2d.snap_point(nu->bezt[u].vec[0]);
          }

          if (!skip_selected || !(is_selected_h2 || (is_autoalign_h2 && is_selected_h1))) {
            has_snap |= nearest2d.snap_point(nu->bezt[u].vec[2]);
          }
        }
        else {
          if (nu->bp[u].hide) {
            /* Skip hidden. */
            continue;
          }

          bool is_selected = (nu->bp[u].f1 & SELECT) != 0;
          if (is_selected && skip_selected) {
            continue;
          }

          has_snap |= nearest2d.snap_point(nu->bp[u].vec);
        }
      }
      else {
        /* Curve is not visible outside editmode if nurb length less than two. */
        if (nu->pntsu > 1) {
          if (nu->bezt) {
            has_snap |= nearest2d.snap_point(nu->bezt[u].vec[1]);
          }
          else {
            has_snap |= nearest2d.snap_point(nu->bp[u].vec);
          }
        }
      }
    }
  }
  if (has_snap) {
    nearest2d.register_result(sctx, ob_eval, &cu->id);
    return SCE_SNAP_TO_POINT;
  }
  return SCE_SNAP_TO_NONE;
}
