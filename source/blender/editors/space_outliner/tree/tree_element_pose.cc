/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_constraint.hh"
#include "tree_element_pose.hh"

namespace blender {

struct bConstraint;

namespace ed::outliner {

TreeElementPoseBase::TreeElementPoseBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  BLI_assert(legacy_te.store_elem->type == TSE_POSE_BASE);
  legacy_te.name = IFACE_("Pose");
}

ID *TreeElementPoseBase::owner_id(Object &object)
{
  return &object.id;
}

void TreeElementPoseBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  bArmature *arm = id_cast<bArmature *>(object_.data);

  /* channels undefined in editmode, but we want the 'tenla' pose icon itself */
  if ((arm->edbo == nullptr) && (object_.mode & OB_MODE_POSE)) {
    int const_index = 1000; /* ensure unique id for bone constraints */

    for (const auto [a, pchan] : object_.pose->chanbase.enumerate()) {
      TreeElement *ten = add_element<TreeElementPoseChannel>({.index = a}, object_, pchan);
      pchan.temp = static_cast<void *>(ten);

      if (!pchan.constraints.is_empty()) {
        // Object *target;
        TreeElement *tenla1 = add_element<TreeElementConstraintBase>({.parent = ten}, object_);
        // char *str;

        for (bConstraint &con : pchan.constraints) {
          add_element<TreeElementConstraint>(
              {.parent = tenla1, .index = const_index}, object_, con);
          /* possible add all other types links? */
        }
        const_index++;
      }
    }
    /* make hierarchy */
    TreeElement *ten = static_cast<TreeElement *>(legacy_te_.subtree.first);
    while (ten) {
      TreeElement *nten = ten->next, *par;
      TreeStoreElem *tselem = TREESTORE(ten);
      if (tselem->type == TSE_POSE_CHANNEL) {
        bPoseChannel *pchan = static_cast<bPoseChannel *>(ten->directdata);
        if (pchan->parent) {
          BLI_remlink(&legacy_te_.subtree, ten);
          par = static_cast<TreeElement *>(pchan->parent->temp);
          BLI_addtail(&par->subtree, ten);
          ten->parent = par;
        }
      }
      ten = nten;
    }
  }
}

/* -------------------------------------------------------------------- */

TreeElementPoseChannel::TreeElementPoseChannel(TreeElement &legacy_te,
                                               Object & /*object*/,
                                               bPoseChannel &pchan)
    : AbstractTreeElement(legacy_te), /* object_(object), */ pchan_(pchan)
{
  BLI_assert(legacy_te.store_elem->type == TSE_POSE_CHANNEL);
  legacy_te.name = pchan_.name;
  legacy_te.directdata = &pchan_;
}

ID *TreeElementPoseChannel::owner_id(Object &object, bPoseChannel & /*pchan*/)
{
  return &object.id;
}

}  // namespace ed::outliner
}  // namespace blender
