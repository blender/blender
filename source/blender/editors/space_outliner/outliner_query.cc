/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <functional>

#include "BLI_listbase.h"

#include "DNA_space_types.h"

#include "outliner_intern.hh"
#include "tree/tree_display.hh"

namespace blender::ed::outliner {

bool outliner_shows_mode_column(const SpaceOutliner &space_outliner)
{
  const AbstractTreeDisplay &tree_display = *space_outliner.runtime->tree_display;

  return tree_display.supportsModeColumn() && (space_outliner.flag & SO_MODE_COLUMN);
}

/**
 * Iterate over the entire tree (including collapsed sub-elements), probing if any of the elements
 * has a warning to be displayed.
 */
bool outliner_has_element_warnings(const SpaceOutliner &space_outliner)
{
  std::function<bool(const ListBase &)> recursive_fn;

  recursive_fn = [&](const ListBase &lb) {
    LISTBASE_FOREACH (const TreeElement *, te, &lb) {
      if (te->abstract_element && !te->abstract_element->getWarning().is_empty()) {
        return true;
      }

      if (recursive_fn(te->subtree)) {
        return true;
      }
    }

    return false;
  };

  return recursive_fn(space_outliner.tree);
}

}  // namespace blender::ed::outliner
