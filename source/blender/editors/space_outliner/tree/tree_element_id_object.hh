/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDObject final : public TreeElementID {
  Object &object_;

 public:
  TreeElementIDObject(TreeElement &legacy_te, Object &object);

  void expand(SpaceOutliner &) const override;

 private:
  void expandData(SpaceOutliner &) const;
  void expandPose(SpaceOutliner &) const;
  void expandMaterials(SpaceOutliner &) const;
  void expandConstraints(SpaceOutliner &) const;
  void expandModifiers(SpaceOutliner &) const;
  void expandGPencilModifiers(SpaceOutliner &) const;
  void expandGPencilEffects(SpaceOutliner &) const;
  void expandVertexGroups(SpaceOutliner &) const;
  void expandDuplicatedGroup(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
