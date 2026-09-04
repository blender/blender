/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.hh"

#include "BKE_armature.hh"

#include "../outliner_intern.hh"
#include "tree_display.hh"

#include "tree_element_bone.hh"
#include "tree_element_bone_collection.hh"
#include "tree_element_edit_bone.hh"
#include "tree_element_id_armature.hh"

namespace blender::ed::outliner {

TreeElementIDArmature::TreeElementIDArmature(TreeElement &legacy_te, bArmature &arm)
    : TreeElementID(legacy_te, arm.id), arm_(arm)
{
}

void TreeElementIDArmature::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(arm_.adt);

  if (arm_.edbo) {
    expand_edit_bones();
  }
  else {
    /* Do not extend Armature when we have pose-mode. */
    TreeStoreElem *tselem = TREESTORE(legacy_te_.parent);
    if (TSE_IS_REAL_ID(tselem) && GS(tselem->id->name) == ID_OB &&
        (id_cast<Object *>(tselem->id))->mode & OB_MODE_POSE)
    {
      /* pass */
    }
    else {
      expand_bones();
    }
  }

  if (arm_.collection_array_num > 0) {
    add_element<TreeElementBoneCollectionBase>({}, arm_);
  }
}

void TreeElementIDArmature::expand_edit_bones() const
{

  for (const auto [a, ebone] : (arm_.edbo)->enumerate()) {
    TreeElement *ten = add_element<TreeElementEditBone>({.index = a}, arm_.id, ebone);
    ebone.temp.p = ten;
  }
  /* make hierarchy */
  TreeElement *ten = arm_.edbo->first ? static_cast<TreeElement *>(
                                            (static_cast<EditBone *>(arm_.edbo->first))->temp.p) :
                                        nullptr;
  while (ten) {
    TreeElement *nten = ten->next, *par;
    EditBone *ebone = static_cast<EditBone *>(ten->directdata);
    if (ebone->parent) {
      BLI_remlink(&legacy_te_.subtree, ten);
      par = static_cast<TreeElement *>(ebone->parent->temp.p);
      BLI_addtail(&par->subtree, ten);
      ten->parent = par;
    }
    ten = nten;
  }
}

/* special handling of hierarchical non-lib data */
static void outliner_add_bone(AbstractTreeDisplay &tree_display,
                              ListBaseT<TreeElement> *lb,
                              ID *id,
                              Bone *curBone,
                              TreeElement *parent,
                              int *a)
{
  TreeElement *te = tree_display.add_element<TreeElementBone>(
      {.lb = lb, .parent = parent, .index = *a}, *id, *curBone);

  (*a)++;

  for (Bone &child_bone : curBone->childbase) {
    outliner_add_bone(tree_display, &te->subtree, id, &child_bone, te, a);
  }
}

void TreeElementIDArmature::expand_bones() const
{
  int a = 0;
  for (Bone &bone : arm_.bonebase) {
    outliner_add_bone(*display_, &legacy_te_.subtree, &arm_.id, &bone, &legacy_te_, &a);
  }
}

}  // namespace blender::ed::outliner
