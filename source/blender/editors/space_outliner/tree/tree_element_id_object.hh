/* SPDX-FileCopyrightText: 2023 Blender Authors
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
  void expand_data(SpaceOutliner &) const;
  void expand_pose(SpaceOutliner &) const;
  void expand_materials(SpaceOutliner &) const;
  void expand_constraints(SpaceOutliner &) const;
  void expand_modifiers(SpaceOutliner &) const;
  void expand_gpencil_modifiers(SpaceOutliner &) const;
  void expand_gpencil_effects(SpaceOutliner &) const;
  void expand_vertex_groups(SpaceOutliner &) const;
  void expand_duplicated_group(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
