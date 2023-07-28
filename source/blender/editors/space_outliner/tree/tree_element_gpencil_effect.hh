/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct Object;
struct ShaderFxData;

namespace blender::ed::outliner {

class TreeElementGPencilEffectBase final : public AbstractTreeElement {
  Object &object_;

 public:
  TreeElementGPencilEffectBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner &) const override;
};

class TreeElementGPencilEffect final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  ShaderFxData &fx_;

 public:
  TreeElementGPencilEffect(TreeElement &legacy_te, Object &object, ShaderFxData &fx);
  void expand(SpaceOutliner &) const override;
};

}  // namespace blender::ed::outliner
