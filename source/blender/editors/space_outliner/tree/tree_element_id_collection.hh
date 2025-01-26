/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct Collection;

namespace blender::ed::outliner {

class TreeElementIDCollection final : public TreeElementID {
  Collection &collection_;

 public:
  TreeElementIDCollection(TreeElement &legacy_te, Collection &collection);

  void expand(SpaceOutliner & /*soops*/) const override;
};

}  // namespace blender::ed::outliner
