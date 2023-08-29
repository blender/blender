/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_modifier.hh"

namespace blender::ed::outliner {

TreeElementModifierBase::TreeElementModifierBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  legacy_te.name = IFACE_("Modifiers");
}

void TreeElementModifierBase::expand(SpaceOutliner &space_outliner) const
{
  int index;
  LISTBASE_FOREACH_INDEX (ModifierData *, md, &object_.modifiers, index) {
    ModifierDataStoreElem md_store(md);

    ModifierCreateElementData md_data = {&object_, &md_store};

    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &md_data, &legacy_te_, TSE_MODIFIER, index);
  }
  LISTBASE_FOREACH_INDEX (GpencilModifierData *, md, &object_.greasepencil_modifiers, index) {
    ModifierDataStoreElem md_store(md);

    ModifierCreateElementData md_data = {&object_, &md_store};

    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &md_data, &legacy_te_, TSE_MODIFIER, index);
  }
}

TreeElementModifier::TreeElementModifier(TreeElement &legacy_te,
                                         Object &object,
                                         ModifierDataStoreElem &md)
    : AbstractTreeElement(legacy_te), object_(object), md_(md)
{
  if (md_.type == MODIFIER_TYPE) {
    legacy_te.name = md_.md->name;
    legacy_te.directdata = md_.md;
  }
  if (md_.type == GPENCIL_MODIFIER_TYPE) {
    legacy_te.name = md_.gp_md->name;
    legacy_te.directdata = md_.gp_md;
  }
}

void TreeElementModifier::expand(SpaceOutliner &space_outliner) const
{
  if (md_.type == MODIFIER_TYPE) {
    ModifierData *md = md_.md;
    if (md->type == eModifierType_Lattice) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((LatticeModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Curve) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((CurveModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Armature) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((ArmatureModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_Hook) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((HookModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eModifierType_ParticleSystem) {
      ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

      ParticleSystemElementCreateData psys_data = {&object_, psys};

      outliner_add_element(
          &space_outliner, &legacy_te_.subtree, &psys_data, &legacy_te_, TSE_LINKED_PSYS, 0);
    }
  }
  if (md_.type == GPENCIL_MODIFIER_TYPE) {
    GpencilModifierData *md = md_.gp_md;
    if (md->type == eGpencilModifierType_Armature) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((ArmatureGpencilModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eGpencilModifierType_Hook) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((HookGpencilModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
    else if (md->type == eGpencilModifierType_Lattice) {
      outliner_add_element(&space_outliner,
                           &legacy_te_.subtree,
                           ((LatticeGpencilModifierData *)md)->object,
                           &legacy_te_,
                           TSE_LINKED_OB,
                           0);
    }
  }
}

}  // namespace blender::ed::outliner
