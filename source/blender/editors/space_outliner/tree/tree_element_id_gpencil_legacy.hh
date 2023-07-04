/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDGPLegacy final : public TreeElementID {
  bGPdata &gpd_;

 public:
  TreeElementIDGPLegacy(TreeElement &legacy_te, bGPdata &gpd);

  void expand(SpaceOutliner &) const override;

 private:
  void expand_layers(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
