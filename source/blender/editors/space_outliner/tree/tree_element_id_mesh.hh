/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDMesh final : public TreeElementID {
  Mesh &mesh_;

 public:
  TreeElementIDMesh(TreeElement &legacy_te_, Mesh &mesh);

  void expand(SpaceOutliner &) const override;
  bool isExpandValid() const override;

 private:
  void expandKey(SpaceOutliner &) const;
  void expandMaterials(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
