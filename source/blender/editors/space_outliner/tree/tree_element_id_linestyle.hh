/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct FreestyleLineStyle;

namespace blender::ed::outliner {

class TreeElementIDLineStyle final : public TreeElementID {
  FreestyleLineStyle &linestyle_;

 public:
  TreeElementIDLineStyle(TreeElement &legacy_te, FreestyleLineStyle &linestyle);

  void expand(SpaceOutliner &) const override;

 private:
  void expand_textures(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
