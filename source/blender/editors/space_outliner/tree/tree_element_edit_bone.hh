/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct ID;
struct EditBone;

namespace blender::ed::outliner {

class TreeElementEditBone final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // ID &armature_id_;
  EditBone &ebone_;

 public:
  TreeElementEditBone(TreeElement &legacy_te, ID &armature_id, EditBone &ebone);
};

}  // namespace blender::ed::outliner
