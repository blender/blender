/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDTexture final : public TreeElementID {
  Tex &texture_;

 public:
  TreeElementIDTexture(TreeElement &legacy_te, Tex &texture);

  void expand(SpaceOutliner &) const override;

 private:
  void expandImage(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
