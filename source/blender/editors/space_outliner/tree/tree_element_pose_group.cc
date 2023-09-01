/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_pose_group.hh"

namespace blender::ed::outliner {

TreeElementPoseGroupBase::TreeElementPoseGroupBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  BLI_assert(legacy_te.store_elem->type == TSE_POSEGRP_BASE);
  legacy_te.name = IFACE_("Bone Groups");
}

void TreeElementPoseGroupBase::expand(SpaceOutliner &space_outliner) const
{
  int index;
  LISTBASE_FOREACH_INDEX (bActionGroup *, agrp, &object_.pose->agroups, index) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &object_.id, agrp, &legacy_te_, TSE_POSEGRP, index);
  }
}

TreeElementPoseGroup::TreeElementPoseGroup(TreeElement &legacy_te,
                                           Object & /* object */,
                                           bActionGroup &agrp)
    : AbstractTreeElement(legacy_te), /* object_(object), */ agrp_(agrp)
{
  BLI_assert(legacy_te.store_elem->type == TSE_POSEGRP);
  legacy_te.name = agrp_.name;
  legacy_te.directdata = &agrp_;
}

}  // namespace blender::ed::outliner
