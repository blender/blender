/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementLinkedNodeTree final : public AbstractTreeElement {

 public:
  TreeElementLinkedNodeTree(TreeElement &legacy_te, ID &id);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NODETREE;
  }
};

}  // namespace blender::ed::outliner
