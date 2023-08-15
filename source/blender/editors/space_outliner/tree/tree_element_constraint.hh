/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct bConstraint;

namespace blender::ed::outliner {

class TreeElementConstraintBase final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;

 public:
  TreeElementConstraintBase(TreeElement &legacy_te, Object &object);
};

class TreeElementConstraint final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bConstraint &con_;

 public:
  TreeElementConstraint(TreeElement &legacy_te, Object &object, bConstraint &con);
};

}  // namespace blender::ed::outliner
