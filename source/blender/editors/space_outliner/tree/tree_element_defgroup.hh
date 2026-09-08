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

struct Object;
struct bDeformGroup;

namespace ed::outliner {

class TreeElementDeformGroupBase final : public AbstractTreeElement {
  Object &object_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_DEFGROUP_BASE;

  TreeElementDeformGroupBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_GROUP_VERTEX;
  }
};

class TreeElementDeformGroup final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bDeformGroup &defgroup_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_DEFGROUP;

  TreeElementDeformGroup(TreeElement &legacy_te, Object &object, bDeformGroup &defgroup);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, bDeformGroup &defgroup);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_GROUP_VERTEX;
  }
};

}  // namespace ed::outliner
}  // namespace blender
