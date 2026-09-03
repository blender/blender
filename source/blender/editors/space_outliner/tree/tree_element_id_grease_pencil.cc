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
    add_element(
        &legacy_te_.subtree, &grease_pencil_.id, &child, &legacy_te_, TSE_GREASE_PENCIL_NODE, 0);
  }
}

void TreeElementIDGreasePencil::expand_materials() const
{
  for (const int i : IndexRange(grease_pencil_.material_array_num)) {
    add_element(&legacy_te_.subtree,
                &grease_pencil_.material_array[i]->id,
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                i);
  }
}

}  // namespace blender::ed::outliner
