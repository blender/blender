/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct bGPDlayer;

namespace ed::outliner {

class TreeElementGPencilLayer final : public AbstractTreeElement {
 public:
  TreeElementGPencilLayer(TreeElement &legacy_te, bGPDlayer &gplayer);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_OUTLINER_DATA_GREASEPENCIL;
  }
};

}  // namespace ed::outliner
}  // namespace blender
