/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_armature_types.h"
#include "DNA_outliner_types.h"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_bone_collection.hh"

namespace blender::ed::outliner {

TreeElementBoneCollectionBase::TreeElementBoneCollectionBase(TreeElement &legacy_te,
                                                             bArmature &armature)
    : AbstractTreeElement(legacy_te), armature_(armature)
{
  BLI_assert(legacy_te.store_elem->type == TSE_BONE_COLLECTION_BASE);
  legacy_te.name = IFACE_("Bone Collections");
}

ID *TreeElementBoneCollectionBase::owner_id(bArmature &armature)
{
  return &armature.id;
}

void TreeElementBoneCollectionBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  int index = 0;
  for (BoneCollection *bcoll : armature_.collections_roots()) {
    add_element<TreeElementBoneCollection>({.index = index}, armature_, *bcoll);
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

ID *TreeElementBoneCollection::owner_id(bArmature &armature, BoneCollection & /*bcoll*/)
{
  return &armature.id;
}

void TreeElementBoneCollection::expand(SpaceOutliner & /*space_outliner*/) const
{
  int index = 0;
  for (BoneCollection *child_bcoll : armature_.collection_children(&bcoll_)) {
    add_element<TreeElementBoneCollection>({.index = index}, armature_, *child_bcoll);
    index++;
  }
}

}  // namespace blender::ed::outliner
