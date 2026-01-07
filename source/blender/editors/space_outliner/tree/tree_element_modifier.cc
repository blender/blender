/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_modifier.hh"

namespace blender::ed::outliner {

TreeElementModifierBase::TreeElementModifierBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  legacy_te.name = IFACE_("Modifiers");
}

void TreeElementModifierBase::expand(SpaceOutliner & /*space_outliner*/) const
{

  for (const auto [index, md] : object_.modifiers.enumerate()) {
    ModifierDataStoreElem md_store(&md);

    add_element(&legacy_te_.subtree, &object_.id, &md_store, &legacy_te_, TSE_MODIFIER, index);
  }
  for (const auto [index, md] : object_.greasepencil_modifiers.enumerate()) {
    ModifierDataStoreElem md_store(&md);

    add_element(&legacy_te_.subtree, &object_.id, &md_store, &legacy_te_, TSE_MODIFIER, index);
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

void TreeElementModifier::expand(SpaceOutliner & /*space_outliner*/) const
{
  if (md_.type == MODIFIER_TYPE) {
    ModifierData *md = md_.md;
    if (md->type == eModifierType_Lattice) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>((reinterpret_cast<LatticeModifierData *>(md))->object),
                  nullptr,
                  &legacy_te_,
                  TSE_LINKED_OB,
                  0);
    }
    else if (md->type == eModifierType_Curve) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>((reinterpret_cast<CurveModifierData *>(md))->object),
                  nullptr,
                  &legacy_te_,
                  TSE_LINKED_OB,
                  0);
    }
    else if (md->type == eModifierType_Armature) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>((reinterpret_cast<ArmatureModifierData *>(md))->object),
                  nullptr,
                  &legacy_te_,
                  TSE_LINKED_OB,
                  0);
    }
    else if (md->type == eModifierType_Hook) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>((reinterpret_cast<HookModifierData *>(md))->object),
                  nullptr,
                  &legacy_te_,
                  TSE_LINKED_OB,
                  0);
    }
    else if (md->type == eModifierType_Nodes) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>((reinterpret_cast<NodesModifierData *>(md))->node_group),
                  nullptr,
                  &legacy_te_,
                  TSE_LINKED_NODE_TREE,
                  0);
    }
    else if (md->type == eModifierType_ParticleSystem) {
      ParticleSystem *psys = (reinterpret_cast<ParticleSystemModifierData *>(md))->psys;

      add_element(&legacy_te_.subtree, &object_.id, psys, &legacy_te_, TSE_LINKED_PSYS, 0);
    }
  }
  if (md_.type == GPENCIL_MODIFIER_TYPE) {
    GpencilModifierData *md = md_.gp_md;
    if (md->type == eGpencilModifierType_Armature) {
      add_element(
          &legacy_te_.subtree,
          reinterpret_cast<ID *>((reinterpret_cast<ArmatureGpencilModifierData *>(md))->object),
          nullptr,
          &legacy_te_,
          TSE_LINKED_OB,
          0);
    }
    else if (md->type == eGpencilModifierType_Hook) {
      add_element(
          &legacy_te_.subtree,
          reinterpret_cast<ID *>((reinterpret_cast<HookGpencilModifierData *>(md))->object),
          nullptr,
          &legacy_te_,
          TSE_LINKED_OB,
          0);
    }
    else if (md->type == eGpencilModifierType_Lattice) {
      add_element(
          &legacy_te_.subtree,
          reinterpret_cast<ID *>((reinterpret_cast<LatticeGpencilModifierData *>(md))->object),
          nullptr,
          &legacy_te_,
          TSE_LINKED_OB,
          0);
    }
  }
}

}  // namespace blender::ed::outliner
