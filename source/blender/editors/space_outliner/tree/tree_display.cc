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

#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "tree_display.hh"

using namespace blender::ed::outliner;

namespace blender::ed::outliner {

AbstractTreeDisplay *outliner_tree_display_create(int /*eSpaceOutliner_Mode*/ mode,
                                                  SpaceOutliner *space_outliner)
{
  AbstractTreeDisplay *tree_display = nullptr;

  switch ((eSpaceOutliner_Mode)mode) {
    case SO_SCENES:
      tree_display = new TreeDisplayScenes(*space_outliner);
      break;
    case SO_LIBRARIES:
      tree_display = new TreeDisplayLibraries(*space_outliner);
      break;
    case SO_SEQUENCE:
      tree_display = new TreeDisplaySequencer(*space_outliner);
      break;
    case SO_DATA_API:
      tree_display = new TreeDisplayDataAPI(*space_outliner);
      break;
    case SO_ID_ORPHANS:
      tree_display = new TreeDisplayIDOrphans(*space_outliner);
      break;
    case SO_OVERRIDES_LIBRARY:
      tree_display = new TreeDisplayOverrideLibrary(*space_outliner);
      break;
    case SO_VIEW_LAYER:
      /* FIXME(Julian): this should not be the default! Return nullptr and handle that as valid
       * case. */
    default:
      tree_display = new TreeDisplayViewLayer(*space_outliner);
      break;
  }

  return tree_display;
}

void outliner_tree_display_destroy(AbstractTreeDisplay **tree_display)
{
  delete *tree_display;
  *tree_display = nullptr;
}

bool AbstractTreeDisplay::hasWarnings() const
{
  return has_warnings;
}

}  // namespace blender::ed::outliner
