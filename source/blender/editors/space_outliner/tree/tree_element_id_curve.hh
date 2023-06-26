/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDCurve final : public TreeElementID {
  Curve &curve_;

 public:
  TreeElementIDCurve(TreeElement &legacy_te, Curve &curve);

  void expand(SpaceOutliner &) const override;

 private:
  void expandMaterials(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
