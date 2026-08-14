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

struct bArmature;
struct BoneCollection;

namespace ed::outliner {

class TreeElementBoneCollectionBase final : public AbstractTreeElement {
  bArmature &armature_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_BONE_COLLECTION_BASE;

  TreeElementBoneCollectionBase(TreeElement &legacy_te, bArmature &armature);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(bArmature &armature);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_GROUP_BONE;
  }
};

class TreeElementBoneCollection final : public AbstractTreeElement {
  bArmature &armature_;
  BoneCollection &bcoll_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_BONE_COLLECTION;

  TreeElementBoneCollection(TreeElement &legacy_te, bArmature &armature, BoneCollection &bcoll);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(bArmature &armature, BoneCollection &bcoll);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_GROUP_BONE;
  }
};

}  // namespace ed::outliner
}  // namespace blender
