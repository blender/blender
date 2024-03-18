/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"
#include "BLI_mempool.h"

#include "DNA_space_types.h"

#include "RNA_access.hh"

#include "../outliner_intern.hh"
#include "tree_display.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

TreeDisplayDataAPI::TreeDisplayDataAPI(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayDataAPI::build_tree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  PointerRNA mainptr = RNA_main_pointer_create(source_data.bmain);

  TreeElement *te = add_element(&tree, nullptr, (void *)&mainptr, nullptr, TSE_RNA_STRUCT, -1);

  /* On first view open parent data elements */
  const int show_opened = !space_outliner_.treestore ||
                          !BLI_mempool_len(space_outliner_.treestore);
  if (show_opened) {
    TreeStoreElem *tselem = TREESTORE(te);
    tselem->flag &= ~TSE_CLOSED;
  }
  return tree;
}

bool TreeDisplayDataAPI::is_lazy_built() const
{
  return true;
}

}  // namespace blender::ed::outliner
