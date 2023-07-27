/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_listBase.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"
#include "DNA_particle_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_deform.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_id_object.hh"

namespace blender::ed::outliner {

TreeElementIDObject::TreeElementIDObject(TreeElement &legacy_te, Object &object)
    : TreeElementID(legacy_te, object.id), object_(object)
{
}

void TreeElementIDObject::expand(SpaceOutliner &space_outliner) const
{
  /* tuck pointer back in object, to construct hierarchy */
  object_.id.newid = (ID *)(&legacy_te_);

  expand_animation_data(space_outliner, object_.adt);

  expand_data(space_outliner);
  expand_pose(space_outliner);
  expand_materials(space_outliner);
  expand_constraints(space_outliner);
  expand_modifiers(space_outliner);
  expand_gpencil_modifiers(space_outliner);
  expand_gpencil_effects(space_outliner);
  expand_vertex_groups(space_outliner);
  expand_duplicated_group(space_outliner);
}

void TreeElementIDObject::expand_data(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, object_.data, &legacy_te_, TSE_SOME_ID, 0);
}

void TreeElementIDObject::expand_pose(SpaceOutliner &space_outliner) const
{
  if (!object_.pose) {
    return;
  }
  bArmature *arm = static_cast<bArmature *>(object_.data);
  TreeElement *tenla = outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_POSE_BASE, 0);
  tenla->name = IFACE_("Pose");

  /* channels undefined in editmode, but we want the 'tenla' pose icon itself */
  if ((arm->edbo == nullptr) && (object_.mode & OB_MODE_POSE)) {
    int const_index = 1000; /* ensure unique id for bone constraints */
    int a;
    LISTBASE_FOREACH_INDEX (bPoseChannel *, pchan, &object_.pose->chanbase, a) {
      TreeElement *ten = outliner_add_element(
          &space_outliner, &tenla->subtree, &object_, tenla, TSE_POSE_CHANNEL, a);
      ten->name = pchan->name;
      ten->directdata = pchan;
      pchan->temp = (void *)ten;

      if (!BLI_listbase_is_empty(&pchan->constraints)) {
        /* Object *target; */
        TreeElement *tenla1 = outliner_add_element(
            &space_outliner, &ten->subtree, &object_, ten, TSE_CONSTRAINT_BASE, 0);
        tenla1->name = IFACE_("Constraints");
        /* char *str; */

        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          TreeElement *ten1 = outliner_add_element(
              &space_outliner, &tenla1->subtree, &object_, tenla1, TSE_CONSTRAINT, const_index);
          ten1->name = con->name;
          ten1->directdata = con;
          /* possible add all other types links? */
        }
        const_index++;
      }
    }
    /* make hierarchy */
    TreeElement *ten = static_cast<TreeElement *>(tenla->subtree.first);
    while (ten) {
      TreeElement *nten = ten->next, *par;
      TreeStoreElem *tselem = TREESTORE(ten);
      if (tselem->type == TSE_POSE_CHANNEL) {
        bPoseChannel *pchan = (bPoseChannel *)ten->directdata;
        if (pchan->parent) {
          BLI_remlink(&tenla->subtree, ten);
          par = (TreeElement *)pchan->parent->temp;
          BLI_addtail(&par->subtree, ten);
          ten->parent = par;
        }
      }
      ten = nten;
    }
  }

  /* Pose Groups */
  if (!BLI_listbase_is_empty(&object_.pose->agroups)) {
    TreeElement *ten_bonegrp = outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_POSEGRP_BASE, 0);
    ten_bonegrp->name = IFACE_("Bone Groups");

    int index;
    LISTBASE_FOREACH_INDEX (bActionGroup *, agrp, &object_.pose->agroups, index) {
      TreeElement *ten = outliner_add_element(
          &space_outliner, &ten_bonegrp->subtree, &object_, ten_bonegrp, TSE_POSEGRP, index);
      ten->name = agrp->name;
      ten->directdata = agrp;
    }
  }
}

void TreeElementIDObject::expand_materials(SpaceOutliner &space_outliner) const
{
  for (int a = 0; a < object_.totcol; a++) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, object_.mat[a], &legacy_te_, TSE_SOME_ID, a);
  }
}

void TreeElementIDObject::expand_constraints(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&object_.constraints)) {
    return;
  }
  TreeElement *tenla = outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_CONSTRAINT_BASE, 0);
  tenla->name = IFACE_("Constraints");

  int index;
  LISTBASE_FOREACH_INDEX (bConstraint *, con, &object_.constraints, index) {
    TreeElement *ten = outliner_add_element(
        &space_outliner, &tenla->subtree, &object_, tenla, TSE_CONSTRAINT, index);
    ten->name = con->name;
    ten->directdata = con;
    /* possible add all other types links? */
  }
}

void TreeElementIDObject::expand_modifiers(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&object_.modifiers)) {
    return;
  }
  TreeElement *ten_mod = outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_MODIFIER_BASE, 0);
  ten_mod->name = IFACE_("Modifiers");

  int index;
  LISTBASE_FOREACH_INDEX (ModifierData *, md, &object_.modifiers, index) {
    TreeElement *ten = outliner_add_element(
        &space_outliner, &ten_mod->subtree, &object_, ten_mod, TSE_MODIFIER, index);
    ten->name = md->name;
    ten->directdata = md;

    if (md->type == eModifierType_Lattice) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((LatticeModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Curve) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((CurveModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Armature) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((ArmatureModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Hook) {
      outliner_add_element(
          &space_outliner, &ten->subtree, ((HookModifierData *)md)->object, ten, TSE_LINKED_OB, 0);
    }
    else if (md->type == eModifierType_ParticleSystem) {
      ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

      ParticleSystemElementCreateData psys_data = {&object_, psys};

      outliner_add_element(
          &space_outliner, &ten->subtree, &psys_data, &legacy_te_, TSE_LINKED_PSYS, 0);
    }
  }
}

void TreeElementIDObject::expand_gpencil_modifiers(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&object_.greasepencil_modifiers)) {
    return;
  }
  TreeElement *ten_mod = outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_MODIFIER_BASE, 0);
  ten_mod->name = IFACE_("Modifiers");

  int index;
  LISTBASE_FOREACH_INDEX (GpencilModifierData *, md, &object_.greasepencil_modifiers, index) {
    TreeElement *ten = outliner_add_element(
        &space_outliner, &ten_mod->subtree, &object_, ten_mod, TSE_MODIFIER, index);
    ten->name = md->name;
    ten->directdata = md;

    if (md->type == eGpencilModifierType_Armature) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((ArmatureGpencilModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eGpencilModifierType_Hook) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((HookGpencilModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eGpencilModifierType_Lattice) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((LatticeGpencilModifierData *)md)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
  }
}

void TreeElementIDObject::expand_gpencil_effects(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&object_.shader_fx)) {
    return;
  }
  TreeElement *ten_fx = outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_GPENCIL_EFFECT_BASE, 0);
  ten_fx->name = IFACE_("Effects");

  int index;
  LISTBASE_FOREACH_INDEX (ShaderFxData *, fx, &object_.shader_fx, index) {
    TreeElement *ten = outliner_add_element(
        &space_outliner, &ten_fx->subtree, &object_, ten_fx, TSE_GPENCIL_EFFECT, index);
    ten->name = fx->name;
    ten->directdata = fx;

    if (fx->type == eShaderFxType_Swirl) {
      outliner_add_element(&space_outliner,
                           &ten->subtree,
                           ((SwirlShaderFxData *)fx)->object,
                           ten,
                           TSE_LINKED_OB,
                           0);
    }
  }
}

void TreeElementIDObject::expand_vertex_groups(SpaceOutliner &space_outliner) const
{
  if (!ELEM(object_.type, OB_MESH, OB_GPENCIL_LEGACY, OB_LATTICE)) {
    return;
  }
  const ListBase *defbase = BKE_object_defgroup_list(&object_);
  if (BLI_listbase_is_empty(defbase)) {
    return;
  }
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_DEFGROUP_BASE, 0);
}

void TreeElementIDObject::expand_duplicated_group(SpaceOutliner &space_outliner) const
{
  if (object_.instance_collection && (object_.transflag & OB_DUPLICOLLECTION)) {
    outliner_add_element(&space_outliner,
                         &legacy_te_.subtree,
                         object_.instance_collection,
                         &legacy_te_,
                         TSE_SOME_ID,
                         0);
  }
}

}  // namespace blender::ed::outliner
