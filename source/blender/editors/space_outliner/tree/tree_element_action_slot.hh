/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender {

struct bAction;
namespace animrig {
class Slot;
}

namespace ed::outliner {

class TreeElementActionSlot final : public AbstractTreeElement {

 public:
  static constexpr eTreeStoreElemType element_type = TSE_ACTION_SLOT;

  TreeElementActionSlot(TreeElement &legacy_te, animrig::Slot &slot);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_ACTION_SLOT;
  }
};

}  // namespace ed::outliner
}  // namespace blender
