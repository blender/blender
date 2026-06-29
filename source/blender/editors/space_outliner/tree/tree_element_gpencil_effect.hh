/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct Object;
struct ShaderFxData;

namespace ed::outliner {

class TreeElementGPencilEffectBase final : public AbstractTreeElement {
  Object &object_;

 public:
  TreeElementGPencilEffectBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;
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
  TreeElementGPencilEffect(TreeElement &legacy_te, Object &object, ShaderFxData &fx);
  void expand(SpaceOutliner & /*soops*/) const override;
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SHADERFX;
  }
};

}  // namespace ed::outliner
}  // namespace blender
