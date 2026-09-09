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
#include "DNA_outliner_types.h"

namespace blender {

namespace ed::outliner {

class TreeElementShapeKeyBase final : public AbstractTreeElement {
  Key &key_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_SHAPE_KEY_BASE;

  TreeElementShapeKeyBase(TreeElement &legacy_te, Key &key);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Key &key)
  {
    return &key.id;
  }

  void expand(SpaceOutliner & /*space_outliner*/) const override;
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHAPEKEY_DATA;
  }
};

class TreeElementShapeKey final : public AbstractTreeElement {
  KeyBlock &keyblock_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_SHAPE_KEY_BLOCK;

  TreeElementShapeKey(TreeElement &legacy_te, KeyBlock &keyblock);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHAPEKEY_DATA;
  }
};

}  // namespace ed::outliner
}  // namespace blender
