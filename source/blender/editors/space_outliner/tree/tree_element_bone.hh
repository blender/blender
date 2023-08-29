/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct ID;
struct Bone;

namespace blender::ed::outliner {

class TreeElementBone final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // ID &armature_id_;
  Bone &bone_;

 public:
  TreeElementBone(TreeElement &legacy_te, ID &armature_id, Bone &bone);
};

}  // namespace blender::ed::outliner
