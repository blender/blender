/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct bGPDlayer;

namespace blender::ed::outliner {

class TreeElementGPencilLayer final : public AbstractTreeElement {
 public:
  TreeElementGPencilLayer(TreeElement &legacy_te, bGPDlayer &gplayer);
};

}  // namespace blender::ed::outliner
