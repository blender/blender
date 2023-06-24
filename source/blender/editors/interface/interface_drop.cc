/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "UI_interface.hh"

namespace blender::ui {

bool drop_target_apply_drop(bContext &C,
                            const DropTargetInterface &drop_target,
                            const ListBase &drags)
{

  const char *disabled_hint_dummy = nullptr;
  LISTBASE_FOREACH (const wmDrag *, drag, &drags) {
    if (drop_target.can_drop(*drag, &disabled_hint_dummy)) {
      return drop_target.on_drop(&C, *drag);
    }
  }

  return false;
}

char *drop_target_tooltip(const DropTargetInterface &drop_target, const wmDrag &drag)
{
  const std::string tooltip = drop_target.drop_tooltip(drag);
  return tooltip.empty() ? nullptr : BLI_strdup(tooltip.c_str());
}

}  // namespace blender::ui
