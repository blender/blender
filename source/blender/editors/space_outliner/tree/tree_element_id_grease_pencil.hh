/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct GreasePencil;

namespace blender::ed::outliner {

class TreeElementIDGreasePencil final : public TreeElementID {
  GreasePencil &grease_pencil_;

 public:
  TreeElementIDGreasePencil(TreeElement &legacy_te, GreasePencil &grease_pencil_);

  void expand(SpaceOutliner &) const override;

 private:
  void expand_layer_tree(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner