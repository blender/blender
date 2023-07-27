/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct Object;
struct bDeformGroup;

namespace blender::ed::outliner {

class TreeElementDeformGroupBase final : public AbstractTreeElement {
  Object &object_;

 public:
  TreeElementDeformGroupBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner &) const override;
};

class TreeElementDeformGroup final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bDeformGroup &defgroup_;

 public:
  TreeElementDeformGroup(TreeElement &legacy_te, Object &object, bDeformGroup &defgroup);
};

}  // namespace blender::ed::outliner
