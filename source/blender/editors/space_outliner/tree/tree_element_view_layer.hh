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
struct ViewLayer;

namespace ed::outliner {

class TreeElementViewLayerBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_R_LAYER_BASE;

  TreeElementViewLayerBase(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Scene &scene);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_RENDERLAYERS;
  }
};

class TreeElementViewLayer final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Scene &scene_;
  ViewLayer &view_layer_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_R_LAYER;

  TreeElementViewLayer(TreeElement &legacy_te, Scene &scene, ViewLayer &view_layer);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Scene &scene, ViewLayer &view_layer);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_RENDER_RESULT;
  }
};

}  // namespace ed::outliner
}  // namespace blender
