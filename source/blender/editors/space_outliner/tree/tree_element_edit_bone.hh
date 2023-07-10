/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct EditBone;

namespace blender::ed::outliner {

class TreeElementEditBone final : public AbstractTreeElement {
  ID &armature_id_;
  EditBone &ebone_;

 public:
  TreeElementEditBone(TreeElement &legacy_te, ID &armature_id, EditBone &ebone);
};

}  // namespace blender::ed::outliner
