/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementViewCollectionBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  TreeElementViewCollectionBase(TreeElement &legacy_te, Scene &scene);
};

}  // namespace blender::ed::outliner
