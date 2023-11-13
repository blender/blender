/* SPDX-FileCopyrightText: 2023 Blender Authors
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

void TreeElementIDCurve::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(curve_.adt);

  expand_materials();
}

void TreeElementIDCurve::expand_materials() const
{
  for (int a = 0; a < curve_.totcol; a++) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(curve_.mat[a]),
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                a);
  }
}

}  // namespace blender::ed::outliner
