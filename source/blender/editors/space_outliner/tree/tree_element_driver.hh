/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct AnimData;

namespace ed::outliner {

class TreeElementDriverBase final : public AbstractTreeElement {
  AnimData &anim_data_;

 public:
  TreeElementDriverBase(TreeElement &legacy_te, AnimData &anim_data);

  void expand(SpaceOutliner &space_outliner) const override;
};

}  // namespace ed::outliner
}  // namespace blender
