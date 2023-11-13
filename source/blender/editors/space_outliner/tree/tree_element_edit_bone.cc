/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_armature.h"

#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_edit_bone.hh"

namespace blender::ed::outliner {

TreeElementEditBone::TreeElementEditBone(TreeElement &legacy_te,
                                         ID & /*armature_id*/,
                                         EditBone &ebone)
    : AbstractTreeElement(legacy_te) /*, armature_id_(armature_id)*/, ebone_(ebone)
{
  BLI_assert(legacy_te.store_elem->type == TSE_EBONE);
  legacy_te.directdata = &ebone_;
  legacy_te.name = ebone_.name;
}

}  // namespace blender::ed::outliner
