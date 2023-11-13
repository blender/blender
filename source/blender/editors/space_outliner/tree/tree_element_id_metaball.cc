/* SPDX-FileCopyrightText: 2023 Blender Authors
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

void TreeElementIDMetaBall::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(metaball_.adt);

  expand_materials();
}

void TreeElementIDMetaBall::expand_materials() const
{
  for (int a = 0; a < metaball_.totcol; a++) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(metaball_.mat[a]),
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                a);
  }
}

}  // namespace blender::ed::outliner
