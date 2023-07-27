/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BKE_deform.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_defgroup.hh"

namespace blender::ed::outliner {

TreeElementDeformGroupBase::TreeElementDeformGroupBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  legacy_te.name = IFACE_("Vertex Groups");
}

void TreeElementDeformGroupBase::expand(SpaceOutliner &space_outliner) const
{
  const ListBase *defbase = BKE_object_defgroup_list(&object_);

  int index;
  LISTBASE_FOREACH_INDEX (bDeformGroup *, defgroup, defbase, index) {

    DeformGroupElementCreateData defgroup_data = {&object_, defgroup};

    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &defgroup_data, &legacy_te_, TSE_DEFGROUP, index);
  }
}

TreeElementDeformGroup::TreeElementDeformGroup(TreeElement &legacy_te,
                                               Object & /* object */,
                                               bDeformGroup &defgroup)
    : AbstractTreeElement(legacy_te), /* object_(object), */ defgroup_(defgroup)
{
  legacy_te.name = defgroup_.name;
  legacy_te.directdata = &defgroup_;
}

}  // namespace blender::ed::outliner
