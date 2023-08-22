/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementLinkedObject final : public AbstractTreeElement {

 public:
  TreeElementLinkedObject(TreeElement &legacy_te, ID &id);
};

}  // namespace blender::ed::outliner
