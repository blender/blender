/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct bPoseChannel;
struct Object;

namespace ed::outliner {

class TreeElementPoseBase final : public AbstractTreeElement {
  Object &object_;

 public:
  TreeElementPoseBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;
};

class TreeElementPoseChannel final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  bPoseChannel &pchan_;

 public:
  TreeElementPoseChannel(TreeElement &legacy_te, Object &object, bPoseChannel &pchan);
};

}  // namespace ed::outliner
}  // namespace blender
