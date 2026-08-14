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

struct LayerCollection;

namespace ed::outliner {

class TreeElementLayerCollection final : public AbstractTreeElement {
  LayerCollection &lc_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_LAYER_COLLECTION;

  TreeElementLayerCollection(TreeElement &legacy_te, LayerCollection &lc);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(LayerCollection &lc);
};

}  // namespace ed::outliner
}  // namespace blender
