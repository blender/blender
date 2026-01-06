/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct bAction;
namespace animrig {
class Slot;
}

namespace ed::outliner {

class TreeElementActionSlot final : public AbstractTreeElement {

 public:
  TreeElementActionSlot(TreeElement &legacy_te, animrig::Slot &slot);
};

}  // namespace ed::outliner
}  // namespace blender
