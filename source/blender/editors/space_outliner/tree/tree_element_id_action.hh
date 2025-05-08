/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct bAction;

namespace blender::ed::outliner {

class TreeElementIDAction final : public TreeElementID {
  bAction &action_;

 public:
  TreeElementIDAction(TreeElement &legacy_te, bAction &action);

  void expand(SpaceOutliner &space_outliner) const override;
};

}  // namespace blender::ed::outliner
