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

ListBase TreeDisplayScenes::buildTree(const TreeSourceData &source_data)
{
  /* On first view we open scenes. */
  const int show_opened = !space_outliner_.treestore ||
                          !BLI_mempool_len(space_outliner_.treestore);
  ListBase tree = {nullptr};

  for (ID *id : List<ID>(source_data.bmain->scenes)) {
    Scene *scene = reinterpret_cast<Scene *>(id);
    TreeElement *te = outliner_add_element(
        &space_outliner_, &tree, scene, nullptr, TSE_SOME_ID, 0);
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
