/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_pose.hh"

namespace blender::ed::outliner {

TreeElementPoseBase::TreeElementPoseBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  BLI_assert(legacy_te.store_elem->type == TSE_POSE_BASE);
  legacy_te.name = IFACE_("Pose");
}

void TreeElementPoseBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  bArmature *arm = static_cast<bArmature *>(object_.data);

  /* channels undefined in editmode, but we want the 'tenla' pose icon itself */
  if ((arm->edbo == nullptr) && (object_.mode & OB_MODE_POSE)) {
    int const_index = 1000; /* ensure unique id for bone constraints */
    int a;
    LISTBASE_FOREACH_INDEX (bPoseChannel *, pchan, &object_.pose->chanbase, a) {
      TreeElement *ten = add_element(
          &legacy_te_.subtree, &object_.id, pchan, &legacy_te_, TSE_POSE_CHANNEL, a);
      pchan->temp = (void *)ten;

      if (!BLI_listbase_is_empty(&pchan->constraints)) {
        // Object *target;
        TreeElement *tenla1 = add_element(
            &ten->subtree, &object_.id, nullptr, ten, TSE_CONSTRAINT_BASE, 0);
        // char *str;

        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          add_element(&tenla1->subtree, &object_.id, con, tenla1, TSE_CONSTRAINT, const_index);
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
        bPoseChannel *pchan = (bPoseChannel *)ten->directdata;
        if (pchan->parent) {
          BLI_remlink(&legacy_te_.subtree, ten);
          par = (TreeElement *)pchan->parent->temp;
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

}  // namespace blender::ed::outliner
