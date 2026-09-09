/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender {

struct bConstraint;
struct Object;

namespace ed::outliner {

class TreeElementConstraintBase final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_CONSTRAINT_BASE;

  TreeElementConstraintBase(TreeElement &legacy_te, Object &object);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_CONSTRAINT;
  }
};

class TreeElementConstraint final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bConstraint &con_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_CONSTRAINT;

  TreeElementConstraint(TreeElement &legacy_te, Object &object, bConstraint &con);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, bConstraint &con);

  std::optional<BIFIconID> get_icon() const override;
};

}  // namespace ed::outliner
}  // namespace blender
