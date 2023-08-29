/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct Bone;
struct EditBone;
struct bArmature;

namespace blender::ed::outliner {

class TreeElementIDArmature final : public TreeElementID {
  bArmature &arm_;

 public:
  TreeElementIDArmature(TreeElement &legacy_te, bArmature &arm);

  void expand(SpaceOutliner &) const override;

 private:
  void expand_edit_bones(SpaceOutliner &) const;
  void expand_bones(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
