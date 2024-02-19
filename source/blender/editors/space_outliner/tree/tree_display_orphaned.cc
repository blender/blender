/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_utildefines.h"

#include "BKE_main.hh"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"

namespace blender::ed::outliner {

template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayIDOrphans::TreeDisplayIDOrphans(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayIDOrphans::build_tree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};
  ListBase *lbarray[INDEX_ID_MAX];
  short filter_id_type = (space_outliner_.filter & SO_FILTER_ID_TYPE) ?
                             space_outliner_.filter_id_type :
                             0;

  int tot;
  if (filter_id_type) {
    lbarray[0] = which_libbase(source_data.bmain, filter_id_type);
    tot = 1;
  }
  else {
    tot = set_listbasepointers(source_data.bmain, lbarray);
  }

  for (int a = 0; a < tot; a++) {
    if (BLI_listbase_is_empty(lbarray[a])) {
      continue;
    }
    if (!datablock_has_orphans(*lbarray[a])) {
      continue;
    }

    /* Header for this type of data-block. */
    TreeElement *te = nullptr;
    if (!filter_id_type) {
      ID *id = (ID *)lbarray[a]->first;
      te = add_element(&tree, nullptr, lbarray[a], nullptr, TSE_ID_BASE, 0);
      te->directdata = lbarray[a];
      te->name = outliner_idcode_to_plural(GS(id->name));
    }

    /* Add the orphaned data-blocks - these will not be added with any subtrees attached. */
    for (ID *id : List<ID>(lbarray[a])) {
      if (ID_REAL_USERS(id) <= 0) {
        add_element((te) ? &te->subtree : &tree, id, nullptr, te, TSE_SOME_ID, 0);
      }
    }
  }

  return tree;
}

bool TreeDisplayIDOrphans::datablock_has_orphans(ListBase &lb) const
{
  for (ID *id : List<ID>(lb)) {
    if (ID_REAL_USERS(id) <= 0) {
      return true;
    }
  }
  return false;
}

}  // namespace blender::ed::outliner
