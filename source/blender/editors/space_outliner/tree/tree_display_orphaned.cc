/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_utildefines.h"

#include "BKE_main.h"

#include "../outliner_intern.h"
#include "tree_display.hh"

namespace blender::ed::outliner {

/* Convenience/readability. */
template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayIDOrphans::TreeDisplayIDOrphans(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayIDOrphans::buildTree(const TreeSourceData &source_data)
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
      te = outliner_add_element(&space_outliner_, &tree, lbarray[a], nullptr, TSE_ID_BASE, 0);
      te->directdata = lbarray[a];
      te->name = outliner_idcode_to_plural(GS(id->name));
    }

    /* Add the orphaned data-blocks - these will not be added with any subtrees attached. */
    for (ID *id : List<ID>(lbarray[a])) {
      if (ID_REAL_USERS(id) <= 0) {
        outliner_add_element(&space_outliner_, (te) ? &te->subtree : &tree, id, te, 0, 0);
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
