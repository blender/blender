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

#include "BLI_listbase.h"
#include "BLI_mempool.h"

#include "DNA_space_types.h"

#include "RNA_access.h"

#include "../outliner_intern.hh"
#include "tree_display.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

TreeDisplayDataAPI::TreeDisplayDataAPI(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayDataAPI::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  PointerRNA mainptr;
  RNA_main_pointer_create(source_data.bmain, &mainptr);

  TreeElement *te = outliner_add_element(
      &space_outliner_, &tree, (void *)&mainptr, nullptr, TSE_RNA_STRUCT, -1);

  /* On first view open parent data elements */
  const int show_opened = !space_outliner_.treestore ||
                          !BLI_mempool_len(space_outliner_.treestore);
  if (show_opened) {
    TreeStoreElem *tselem = TREESTORE(te);
    tselem->flag &= ~TSE_CLOSED;
  }
  return tree;
}

}  // namespace blender::ed::outliner
