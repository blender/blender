/* SPDX-FileCopyrightText: 2023 Blender Authors
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

void all(const SpaceOutliner &space_outliner, const ConstVisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all(SpaceOutliner &space_outliner, const VisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all(const ListBaseT<TreeElement> &subtree, const ConstVisitorFn visitor)
{
  for (const TreeElement &element : subtree) {
    visitor(&element);
    all(element.subtree, visitor);
  }
}

void all(ListBaseT<TreeElement> &subtree, const VisitorFn visitor)
{
  for (TreeElement &element : subtree.items_mutable()) {
    /* Get needed data out in case element gets freed. */
    ListBaseT<TreeElement> subtree = element.subtree;

    visitor(&element);
    /* Don't access element from now on, it may be freed. */

    all(subtree, visitor);
  }
}

void all_open(const SpaceOutliner &space_outliner, const ConstVisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all_open(SpaceOutliner &space_outliner, const VisitorFn visitor)
{
  all_open(space_outliner, space_outliner.tree, visitor);
}

void all_open(const SpaceOutliner &space_outliner,
              const ListBaseT<TreeElement> &subtree,
              const ConstVisitorFn visitor)
{
  for (const TreeElement &element : subtree) {
    visitor(&element);

    const TreeStoreElem *tselem = TREESTORE(&element);
    if (TSELEM_OPEN(tselem, &space_outliner)) {
      all_open(space_outliner, element.subtree, visitor);
    }
  }
}

void all_open(SpaceOutliner &space_outliner,
              ListBaseT<TreeElement> &subtree,
              const VisitorFn visitor)
{
  for (TreeElement &element : subtree.items_mutable()) {
    /* Get needed data out in case element gets freed. */
    const TreeStoreElem *tselem = TREESTORE(&element);
    ListBaseT<TreeElement> subtree = element.subtree;

    visitor(&element);
    /* Don't access element from now on, it may be freed. Note that the open/collapsed state may
     * also have been changed in the visitor callback. */

    if (TSELEM_OPEN(tselem, &space_outliner)) {
      all_open(space_outliner, subtree, visitor);
    }
  }
}

}  // namespace blender::ed::outliner::tree_iterator
