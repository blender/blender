/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.hh"

#include "BKE_grease_pencil.hh"

#include "DNA_material_types.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_grease_pencil_node.hh"
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
  expand_materials();
}

void TreeElementIDGreasePencil::expand_layer_tree() const
{
  for (GreasePencilLayerTreeNode &child : grease_pencil_.root_group().children.items_reversed()) {
    add_element<TreeElementGreasePencilNode>({}, grease_pencil_, child.wrap());
  }
}

void TreeElementIDGreasePencil::expand_materials() const
{
  for (const int i : IndexRange(grease_pencil_.material_array_num)) {
    add_id_element({.index = i}, &grease_pencil_.material_array[i]->id);
  }
}

}  // namespace blender::ed::outliner
