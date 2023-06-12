/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_linestyle_types.h"
#include "DNA_listBase.h"
#include "DNA_outliner_types.h"
#include "DNA_texture_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_linestyle.hh"

namespace blender::ed::outliner {

TreeElementIDLineStyle::TreeElementIDLineStyle(TreeElement &legacy_te,
                                               FreestyleLineStyle &linestyle)
    : TreeElementID(legacy_te, linestyle.id), linestyle_(linestyle)
{
}

void TreeElementIDLineStyle::expand(SpaceOutliner &space_outliner) const
{
  expand_animation_data(space_outliner, linestyle_.adt);

  expandTextures(space_outliner);
}

bool TreeElementIDLineStyle::isExpandValid() const
{
  return true;
}

void TreeElementIDLineStyle::expandTextures(SpaceOutliner &space_outliner) const
{
  for (int a = 0; a < MAX_MTEX; a++) {
    if (linestyle_.mtex[a]) {
      outliner_add_element(
          &space_outliner, &legacy_te_.subtree, (linestyle_.mtex[a])->tex, &legacy_te_, TSE_SOME_ID, a);
    }
  }
}

}  // namespace blender::ed::outliner
