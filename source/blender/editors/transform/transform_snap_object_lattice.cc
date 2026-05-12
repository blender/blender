/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_span.hh"

#include "BKE_lattice.hh"
#include "BKE_object.hh"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"

#include "ED_transform_snap_object_context.hh"

#include "transform_snap_object.hh"

namespace blender::ed::transform {

eSnapMode snapLattice(SnapObjectContext *sctx, const Object *ob_eval, const float4x4 &obmat)
{
  bool has_snap = false;

  /* Only vertex snapping is supported for now. */
  if ((sctx->runtime.snap_to_flag & SCE_SNAP_TO_POINT) == 0) {
    return SCE_SNAP_TO_NONE;
  }

  Lattice *lt_id = id_cast<Lattice *>(ob_eval->data);

  SnapData nearest2d(sctx, obmat);

  const bool use_obedit = BKE_object_is_in_editmode(ob_eval);
  Lattice *lt = use_obedit ? lt_id->editlatt->latt : lt_id;

  if (!use_obedit) {
    std::optional<Bounds<float3>> bounds = BKE_lattice_minmax(lt);
    if (bounds && !nearest2d.snap_boundbox(bounds->min, bounds->max)) {
      return SCE_SNAP_TO_NONE;
    }
  }

  nearest2d.clip_planes_enable(sctx, ob_eval, true);

  bool skip_selected = (sctx->runtime.params.snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED) !=
                       0;

  const int totpoint = lt->pntsu * lt->pntsv * lt->pntsw;
  Span<BPoint> points(lt->def, totpoint);

  for (const BPoint &bp : points) {
    if (use_obedit) {
      if (bp.hide) {
        continue;
      }

      bool is_selected = (bp.f1 & SELECT) != 0;
      if (is_selected && skip_selected) {
        continue;
      }
    }
    has_snap |= nearest2d.snap_point(bp.vec);
  }

  if (has_snap) {
    nearest2d.register_result(sctx, ob_eval, &lt->id);
    return SCE_SNAP_TO_POINT;
  }
  return SCE_SNAP_TO_NONE;
}

}  // namespace blender::ed::transform
