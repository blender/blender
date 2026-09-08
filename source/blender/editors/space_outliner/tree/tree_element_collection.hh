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

class TreeElementCollectionBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_SCENE_COLLECTION_BASE;

  TreeElementCollectionBase(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Scene &scene);
};

}  // namespace ed::outliner
}  // namespace blender
