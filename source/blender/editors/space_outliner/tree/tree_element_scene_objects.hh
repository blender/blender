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

struct Scene;

namespace ed::outliner {

class TreeElementSceneObjectsBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_SCENE_OBJECTS_BASE;

  TreeElementSceneObjectsBase(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Scene &scene);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_OUTLINER_OB_GROUP_INSTANCE;
  }
};

}  // namespace ed::outliner
}  // namespace blender
