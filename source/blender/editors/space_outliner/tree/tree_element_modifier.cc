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

#include "BKE_modifier.hh"

#include "BLI_listbase.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_linked_node_tree.hh"
#include "tree_element_linked_object.hh"
#include "tree_element_modifier.hh"
#include "tree_element_particle_system.hh"

namespace blender::ed::outliner {

TreeElementModifierBase::TreeElementModifierBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  legacy_te.name = IFACE_("Modifiers");
}

ID *TreeElementModifierBase::owner_id(Object &object)
{
  return &object.id;
}

void TreeElementModifierBase::expand(SpaceOutliner & /*space_outliner*/) const
{

  for (const auto [index, md] : object_.modifiers.enumerate()) {
    ModifierDataStoreElem md_store(&md);

    add_element<TreeElementModifier>({.index = index}, object_, md_store);
  }
  for (const auto [index, md] : object_.greasepencil_modifiers.enumerate()) {
    ModifierDataStoreElem md_store(&md);

    add_element<TreeElementModifier>({.index = index}, object_, md_store);
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

ID *TreeElementModifier::owner_id(Object &object, ModifierDataStoreElem & /*md*/)
{
  return &object.id;
}

void TreeElementModifier::add_linked_object(Object *object) const
{
  /* Modifiers may reference no object, in which case there is nothing to show. */
  if (object == nullptr) {
    return;
  }
  add_element<TreeElementLinkedObject>({}, *reinterpret_cast<ID *>(object));
}

void TreeElementModifier::expand(SpaceOutliner & /*space_outliner*/) const
{
  if (md_.type == MODIFIER_TYPE) {
    ModifierData *md = md_.md;
    if (md->type == eModifierType_Lattice) {
      add_linked_object((reinterpret_cast<LatticeModifierData *>(md))->object);
    }
    else if (md->type == eModifierType_Curve) {
      add_linked_object((reinterpret_cast<CurveModifierData *>(md))->object);
    }
    else if (md->type == eModifierType_Armature) {
      add_linked_object((reinterpret_cast<ArmatureModifierData *>(md))->object);
    }
    else if (md->type == eModifierType_Hook) {
      add_linked_object((reinterpret_cast<HookModifierData *>(md))->object);
    }
    else if (md->type == eModifierType_Nodes) {
      if (bNodeTree *node_group = (reinterpret_cast<NodesModifierData *>(md))->node_group) {
        add_element<TreeElementLinkedNodeTree>({}, *reinterpret_cast<ID *>(node_group));
      }
    }
    else if (md->type == eModifierType_ParticleSystem) {
      ParticleSystem *psys = (reinterpret_cast<ParticleSystemModifierData *>(md))->psys;

      add_element<TreeElementParticleSystem>({}, object_, *psys);
    }
  }
  if (md_.type == GPENCIL_MODIFIER_TYPE) {
    GpencilModifierData *md = md_.gp_md;
    if (md->type == eGpencilModifierType_Armature) {
      add_linked_object((reinterpret_cast<ArmatureGpencilModifierData *>(md))->object);
    }
    else if (md->type == eGpencilModifierType_Hook) {
      add_linked_object((reinterpret_cast<HookGpencilModifierData *>(md))->object);
    }
    else if (md->type == eGpencilModifierType_Lattice) {
      add_linked_object((reinterpret_cast<LatticeGpencilModifierData *>(md))->object);
    }
  }
}

std::optional<BIFIconID> TreeElementModifier::get_icon() const
{
  Object *ob = reinterpret_cast<Object *>(legacy_te_.store_elem->id);

  ModifierData *md = static_cast<ModifierData *>(
      BLI_findlink(&ob->modifiers, legacy_te_.store_elem->nr));
  if (const ModifierTypeInfo *modifier_type = BKE_modifier_get_info(ModifierType(md->type))) {
    return modifier_type->icon;
  }
  else {
    return ICON_DOT;
  }
}
}  // namespace blender::ed::outliner
