/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"

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
      switch ((eSpaceOutliner_LibOverrideViewMode)space_outliner.lib_override_view_mode) {
        case SO_LIB_OVERRIDE_VIEW_PROPERTIES:
          return std::make_unique<TreeDisplayOverrideLibraryProperties>(space_outliner);
        case SO_LIB_OVERRIDE_VIEW_HIERARCHIES:
          return std::make_unique<TreeDisplayOverrideLibraryHierarchies>(space_outliner);
      }
      break;
    case SO_VIEW_LAYER:
      return std::make_unique<TreeDisplayViewLayer>(space_outliner);
  }

  BLI_assert_unreachable();
  return nullptr;
}

bool AbstractTreeDisplay::supportsModeColumn() const
{
  return false;
}

bool AbstractTreeDisplay::is_lazy_built() const
{
  return false;
}

}  // namespace blender::ed::outliner
