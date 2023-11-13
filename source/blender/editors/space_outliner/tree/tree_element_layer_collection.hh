/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementLayerCollection final : public AbstractTreeElement {
  LayerCollection &lc_;

 public:
  TreeElementLayerCollection(TreeElement &legacy_te, LayerCollection &lc);
};

}  // namespace blender::ed::outliner
