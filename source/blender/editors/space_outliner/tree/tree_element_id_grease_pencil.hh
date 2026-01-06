/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender {

struct GreasePencil;

namespace ed::outliner {

class TreeElementIDGreasePencil final : public TreeElementID {
  GreasePencil &grease_pencil_;

 public:
  TreeElementIDGreasePencil(TreeElement &legacy_te, GreasePencil &grease_pencil_);

  void expand(SpaceOutliner & /*soops*/) const override;

 private:
  void expand_layer_tree() const;
};

}  // namespace ed::outliner
}  // namespace blender
