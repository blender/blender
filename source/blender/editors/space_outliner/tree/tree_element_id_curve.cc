/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_curve_types.h"
#include "DNA_listBase.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_curve.hh"

namespace blender::ed::outliner {

TreeElementIDCurve::TreeElementIDCurve(TreeElement &legacy_te, Curve &curve)
    : TreeElementID(legacy_te, curve.id), curve_(curve)
{
}

void TreeElementIDCurve::expand(SpaceOutliner &space_outliner) const
{
  expand_animation_data(space_outliner, curve_.adt);

  expand_materials(space_outliner);
}

void TreeElementIDCurve::expand_materials(SpaceOutliner &space_outliner) const
{
  for (int a = 0; a < curve_.totcol; a++) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, curve_.mat[a], &legacy_te_, TSE_SOME_ID, a);
  }
}

}  // namespace blender::ed::outliner
