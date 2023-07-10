/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_armature.h"

#include "../outliner_intern.hh"

#include "tree_element_edit_bone.hh"

namespace blender::ed::outliner {

TreeElementEditBone::TreeElementEditBone(TreeElement &legacy_te, ID &armature_id, EditBone &ebone)
    : AbstractTreeElement(legacy_te), armature_id_(armature_id), ebone_(ebone)
{
  legacy_te.directdata = &ebone;
  legacy_te.name = ebone.name;
}

}  // namespace blender::ed::outliner
