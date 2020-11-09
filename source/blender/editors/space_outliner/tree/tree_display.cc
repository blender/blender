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

#include "tree_display.hh"

using namespace blender::ed::outliner;

TreeDisplay *outliner_tree_display_create(eSpaceOutliner_Mode mode, SpaceOutliner *space_outliner)
{
  AbstractTreeDisplay *tree_display = nullptr;

  switch (mode) {
    case SO_SCENES:
      break;
    case SO_LIBRARIES:
      tree_display = new TreeDisplayLibraries(*space_outliner);
      break;
    case SO_SEQUENCE:
    case SO_DATA_API:
    case SO_ID_ORPHANS:
      break;
    case SO_VIEW_LAYER:
      tree_display = new TreeDisplayViewLayer(*space_outliner);
      break;
  }

  return reinterpret_cast<TreeDisplay *>(tree_display);
}

void outliner_tree_display_destroy(TreeDisplay **tree_display)
{
  delete reinterpret_cast<AbstractTreeDisplay *>(*tree_display);
  *tree_display = nullptr;
}

ListBase outliner_tree_display_build_tree(TreeDisplay *tree_display, TreeSourceData *source_data)
{
  return reinterpret_cast<AbstractTreeDisplay *>(tree_display)->buildTree(*source_data);
}
