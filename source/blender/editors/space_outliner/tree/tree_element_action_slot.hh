/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct bAction;
namespace blender::animrig {
class Slot;
}

namespace blender::ed::outliner {

class TreeElementActionSlot final : public AbstractTreeElement {

 public:
  TreeElementActionSlot(TreeElement &legacy_te, blender::animrig::Slot &slot);
};

}  // namespace blender::ed::outliner
