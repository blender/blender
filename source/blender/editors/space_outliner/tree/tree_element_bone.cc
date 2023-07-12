/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_armature_types.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_bone.hh"

namespace blender::ed::outliner {

TreeElementBone::TreeElementBone(TreeElement &legacy_te, ID & /*armature_id*/, Bone &bone)
    : AbstractTreeElement(legacy_te) /*, armature_id_(armature_id)*/, bone_(bone)
{
  BLI_assert(legacy_te.store_elem->type == TSE_BONE);
  legacy_te.name = bone_.name;
  legacy_te.directdata = &bone_;
}

}  // namespace blender::ed::outliner
