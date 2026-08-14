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
struct Bone;

namespace ed::outliner {

class TreeElementBone final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // ID &armature_id_;
  Bone &bone_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_BONE;

  TreeElementBone(TreeElement &legacy_te, ID &armature_id, Bone &bone);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(ID &armature_id, Bone & /*bone*/)
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
