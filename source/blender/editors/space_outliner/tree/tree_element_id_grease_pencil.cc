/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"

#include "BKE_grease_pencil.hh"

#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_grease_pencil.hh"

namespace blender::ed::outliner {

TreeElementIDGreasePencil::TreeElementIDGreasePencil(TreeElement &legacy_te,
                                                     GreasePencil &grease_pencil)
    : TreeElementID(legacy_te, grease_pencil.id), grease_pencil_(grease_pencil)
{
}

void TreeElementIDGreasePencil::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(grease_pencil_.adt);

  expand_layer_tree();
}

void TreeElementIDGreasePencil::expand_layer_tree() const
{
  LISTBASE_FOREACH_BACKWARD (
      GreasePencilLayerTreeNode *, child, &grease_pencil_.root_group().children)
  {
    add_element(
        &legacy_te_.subtree, &grease_pencil_.id, child, &legacy_te_, TSE_GREASE_PENCIL_NODE, 0);
  }
}

}  // namespace blender::ed::outliner
