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

struct ID;
struct EditBone;

namespace ed::outliner {

class TreeElementEditBone final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // ID &armature_id_;
  EditBone &ebone_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_EBONE;

  TreeElementEditBone(TreeElement &legacy_te, ID &armature_id, EditBone &ebone);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(ID &armature_id, EditBone & /*ebone*/)
  {
    return &armature_id;
  }

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_BONE_DATA;
  }
};

}  // namespace ed::outliner
}  // namespace blender
