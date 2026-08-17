/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

namespace ed::outliner {

struct TreeElementIDBase final : public AbstractTreeElement {

 public:
  TreeElementIDBase(TreeElement &legacy_te);
};
}  // namespace ed::outliner
}  // namespace blender
