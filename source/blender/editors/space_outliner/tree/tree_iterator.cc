/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_space_types.h"

#include "BLI_listbase.h"

#include "../outliner_intern.hh"

#include "tree_iterator.hh"

namespace blender::ed::outliner::tree_iterator {

void all(const SpaceOutliner &space_outliner, const VisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all(const ListBase &subtree, const VisitorFn visitor)
{
  LISTBASE_FOREACH_MUTABLE (TreeElement *, element, &subtree) {
    /* Get needed data out in case element gets freed. */
    const ListBase subtree = element->subtree;

    visitor(element);
    /* Don't access element from now on, it may be freed. */

    all(subtree, visitor);
  }
}

void all_open(const SpaceOutliner &space_outliner, const VisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all_open(const SpaceOutliner &space_outliner,
              const ListBase &subtree,
              const VisitorFn visitor)
{
  LISTBASE_FOREACH_MUTABLE (TreeElement *, element, &subtree) {
    /* Get needed data out in case element gets freed. */
    const TreeStoreElem *tselem = TREESTORE(element);
    const ListBase subtree = element->subtree;

    visitor(element);
    /* Don't access element from now on, it may be freed. Note that the open/collapsed state may
     * also have been changed in the visitor callback. */

    if (TSELEM_OPEN(tselem, &space_outliner)) {
      all_open(space_outliner, subtree, visitor);
    }
  }
}

}  // namespace blender::ed::outliner::tree_iterator
