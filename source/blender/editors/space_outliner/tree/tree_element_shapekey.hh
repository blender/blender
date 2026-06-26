/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"
#include "tree_element_id.hh"

#include "DNA_key_types.h"

namespace blender {

namespace ed::outliner {

class TreeElementShapeKeyBase final : public AbstractTreeElement {
  Key &key_;

 public:
  TreeElementShapeKeyBase(TreeElement &legacy_te, Key &key);
  void expand(SpaceOutliner & /*space_outliner*/) const override;
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHAPEKEY_DATA;
  }
};

class TreeElementShapeKey final : public AbstractTreeElement {
  KeyBlock &keyblock_;

 public:
  TreeElementShapeKey(TreeElement &legacy_te, KeyBlock &keyblock);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHAPEKEY_DATA;
  }
};

}  // namespace ed::outliner
}  // namespace blender
