/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_listBase.h"

#include "../outliner_intern.hh"

#include "tree_element_id_collection.hh"

namespace blender::ed::outliner {

TreeElementIDCollection::TreeElementIDCollection(TreeElement &legacy_te, Collection &collection)
    : TreeElementID(legacy_te, collection.id), collection_(collection)
{
}

bool TreeElementIDCollection::isExpandValid() const
{
  return true;
}

void TreeElementIDCollection::expand(SpaceOutliner &space_outliner) const
{
  /* Don't expand for instances, creates too many elements. */
  if (!(legacy_te_.parent && legacy_te_.parent->idcode == ID_OB)) {
    outliner_add_collection_recursive(&space_outliner, &collection_, &legacy_te_);
  }
}

}  // namespace blender::ed::outliner
