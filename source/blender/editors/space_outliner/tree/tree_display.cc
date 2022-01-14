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

std::unique_ptr<AbstractTreeDisplay> AbstractTreeDisplay::createFromDisplayMode(
    int /*eSpaceOutliner_Mode*/ mode, SpaceOutliner &space_outliner)
{
  switch ((eSpaceOutliner_Mode)mode) {
    case SO_SCENES:
      return std::make_unique<TreeDisplayScenes>(space_outliner);
    case SO_LIBRARIES:
      return std::make_unique<TreeDisplayLibraries>(space_outliner);
    case SO_SEQUENCE:
      return std::make_unique<TreeDisplaySequencer>(space_outliner);
    case SO_DATA_API:
      return std::make_unique<TreeDisplayDataAPI>(space_outliner);
    case SO_ID_ORPHANS:
      return std::make_unique<TreeDisplayIDOrphans>(space_outliner);
    case SO_OVERRIDES_LIBRARY:
      return std::make_unique<TreeDisplayOverrideLibrary>(space_outliner);
    case SO_VIEW_LAYER:
      /* FIXME(Julian): this should not be the default! Return nullptr and handle that as valid
       * case. */
    default:
      return std::make_unique<TreeDisplayViewLayer>(space_outliner);
  }
}

bool AbstractTreeDisplay::hasWarnings() const
{
  return has_warnings;
}

}  // namespace blender::ed::outliner
