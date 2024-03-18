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

#include "tree_element_bone_collection.hh"

namespace blender::ed::outliner {

TreeElementBoneCollectionBase::TreeElementBoneCollectionBase(TreeElement &legacy_te,
                                                             bArmature &armature)
    : AbstractTreeElement(legacy_te), armature_(armature)
{
  BLI_assert(legacy_te.store_elem->type == TSE_BONE_COLLECTION_BASE);
  legacy_te.name = IFACE_("Bone Collections");
}

void TreeElementBoneCollectionBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  int index = 0;
  for (BoneCollection *bcoll : armature_.collections_roots()) {
    add_element(
        &legacy_te_.subtree, &armature_.id, bcoll, &legacy_te_, TSE_BONE_COLLECTION, index);
    index++;
  }
}

TreeElementBoneCollection::TreeElementBoneCollection(TreeElement &legacy_te,
                                                     bArmature &armature,
                                                     BoneCollection &bcoll)
    : AbstractTreeElement(legacy_te), armature_(armature), bcoll_(bcoll)
{
  BLI_assert(legacy_te.store_elem->type == TSE_BONE_COLLECTION);
  legacy_te.name = bcoll_.name;
  legacy_te.directdata = &bcoll_;
}

void TreeElementBoneCollection::expand(SpaceOutliner & /*space_outliner*/) const
{
  int index = 0;
  for (BoneCollection *child_bcoll : armature_.collection_children(&bcoll_)) {
    add_element(
        &legacy_te_.subtree, &armature_.id, child_bcoll, &legacy_te_, TSE_BONE_COLLECTION, index);
    index++;
  }
}

}  // namespace blender::ed::outliner
