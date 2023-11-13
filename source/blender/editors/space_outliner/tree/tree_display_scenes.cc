/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_mempool.h"

#include "BKE_main.h"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayScenes::TreeDisplayScenes(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

bool TreeDisplayScenes::supports_mode_column() const
{
  return true;
}

ListBase TreeDisplayScenes::build_tree(const TreeSourceData &source_data)
{
  /* On first view we open scenes. */
  const int show_opened = !space_outliner_.treestore ||
                          !BLI_mempool_len(space_outliner_.treestore);
  ListBase tree = {nullptr};

  for (ID *id : List<ID>(source_data.bmain->scenes)) {
    Scene *scene = reinterpret_cast<Scene *>(id);
    TreeElement *te = add_element(&tree, id, nullptr, nullptr, TSE_SOME_ID, 0);
    TreeStoreElem *tselem = TREESTORE(te);

    /* New scene elements open by default */
    if ((scene == source_data.scene && show_opened) || !tselem->used) {
      tselem->flag &= ~TSE_CLOSED;
    }

    outliner_make_object_parent_hierarchy(&te->subtree);
  }

  return tree;
}

}  // namespace blender::ed::outliner
