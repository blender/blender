/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

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

void TreeElementIDGreasePencil::expand(SpaceOutliner &space_outliner) const
{
  expand_animation_data(space_outliner, grease_pencil_.adt);

  expand_layer_tree(space_outliner);
}

void TreeElementIDGreasePencil::expand_layer_tree(SpaceOutliner &space_outliner) const
{
  LISTBASE_FOREACH_BACKWARD (
      GreasePencilLayerTreeNode *, child, &grease_pencil_.root_group().children)
  {
    outliner_add_element(&space_outliner,
                         &legacy_te_.subtree,
                         &grease_pencil_.id,
                         child,
                         &legacy_te_,
                         TSE_GREASE_PENCIL_NODE,
                         0);
  }
}

}  // namespace blender::ed::outliner
