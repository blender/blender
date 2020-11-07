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

#include "DNA_listBase.h"

#include "tree_view.hh"

namespace outliner = blender::outliner;
/* Convenience. */
using blender::outliner::AbstractTreeView;

TreeView *outliner_tree_view_create(eSpaceOutliner_Mode mode, SpaceOutliner *space_outliner)
{
  AbstractTreeView *tree_view = nullptr;

  switch (mode) {
    case SO_SCENES:
    case SO_LIBRARIES:
    case SO_SEQUENCE:
    case SO_DATA_API:
    case SO_ID_ORPHANS:
      break;
    case SO_VIEW_LAYER:
      tree_view = new outliner::TreeViewViewLayer(*space_outliner);
      break;
  }

  return reinterpret_cast<TreeView *>(tree_view);
}

void outliner_tree_view_destroy(TreeView **tree_view)
{
  delete reinterpret_cast<AbstractTreeView *>(*tree_view);
  *tree_view = nullptr;
}

ListBase outliner_tree_view_build_tree(TreeView *tree_view, TreeSourceData *source_data)
{
  return reinterpret_cast<AbstractTreeView *>(tree_view)->buildTree(*source_data);
}
