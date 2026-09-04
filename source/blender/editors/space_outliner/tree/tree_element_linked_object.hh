/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementLinkedObject final : public AbstractTreeElement {

 public:
  static constexpr eTreeStoreElemType element_type = TSE_LINKED_OB;

  TreeElementLinkedObject(TreeElement &legacy_te, ID &id);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(ID &id)
  {
    return &id;
  }

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_OBJECT_DATA;
  }
};

}  // namespace blender::ed::outliner
