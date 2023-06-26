/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"
#include "DNA_meta_types.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_metaball.hh"

namespace blender::ed::outliner {

TreeElementIDMetaBall::TreeElementIDMetaBall(TreeElement &legacy_te, MetaBall &metaball)
    : TreeElementID(legacy_te, metaball.id), metaball_(metaball)
{
}

void TreeElementIDMetaBall::expand(SpaceOutliner &space_outliner) const
{
  expand_animation_data(space_outliner, metaball_.adt);

  expandMaterials(space_outliner);
}

void TreeElementIDMetaBall::expandMaterials(SpaceOutliner &space_outliner) const
{
  for (int a = 0; a < metaball_.totcol; a++) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, metaball_.mat[a], &legacy_te_, TSE_SOME_ID, a);
  }
}

}  // namespace blender::ed::outliner
