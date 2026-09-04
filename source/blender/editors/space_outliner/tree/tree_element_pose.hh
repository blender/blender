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

struct bPoseChannel;
struct Object;

namespace ed::outliner {

class TreeElementPoseBase final : public AbstractTreeElement {
  Object &object_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_POSE_BASE;

  TreeElementPoseBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_ARMATURE_DATA;
  }
};

class TreeElementPoseChannel final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bPoseChannel &pchan_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_POSE_CHANNEL;

  TreeElementPoseChannel(TreeElement &legacy_te, Object &object, bPoseChannel &pchan);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, bPoseChannel &pchan);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_BONE_DATA;
  }
};

}  // namespace ed::outliner
}  // namespace blender
