/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_action_types.h"

#include "../outliner_intern.hh"

#include "tree_element_action_slot.hh"

#include "ANIM_action.hh"

namespace blender::ed::outliner {

TreeElementActionSlot::TreeElementActionSlot(TreeElement &legacy_te, blender::animrig::Slot &slot)
    : AbstractTreeElement(legacy_te)
{
  legacy_te.name = &slot.identifier[2];
  legacy_te.directdata = &slot;
}

}  // namespace blender::ed::outliner
