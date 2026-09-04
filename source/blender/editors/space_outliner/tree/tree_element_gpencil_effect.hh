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
struct ShaderFxData;

namespace ed::outliner {

class TreeElementGPencilEffectBase final : public AbstractTreeElement {
  Object &object_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_GPENCIL_EFFECT_BASE;

  TreeElementGPencilEffectBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHADERFX;
  }
};

class TreeElementGPencilEffect final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  ShaderFxData &fx_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_GPENCIL_EFFECT;

  TreeElementGPencilEffect(TreeElement &legacy_te, Object &object, ShaderFxData &fx);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, ShaderFxData &fx);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHADERFX;
  }
};

}  // namespace ed::outliner
}  // namespace blender
