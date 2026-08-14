/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_object_types.h"
#include "DNA_outliner_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_listbase.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_gpencil_effect.hh"
#include "tree_element_linked_object.hh"

namespace blender::ed::outliner {

TreeElementGPencilEffectBase::TreeElementGPencilEffectBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GPENCIL_EFFECT_BASE);
  legacy_te.name = IFACE_("Effects");
}

ID *TreeElementGPencilEffectBase::owner_id(Object &object)
{
  return &object.id;
}

void TreeElementGPencilEffectBase::expand(SpaceOutliner & /*space_outliner*/) const
{

  for (const auto [index, fx] : object_.shader_fx.enumerate()) {
    add_element<TreeElementGPencilEffect>({.index = index}, object_, fx);
  }
}

TreeElementGPencilEffect::TreeElementGPencilEffect(TreeElement &legacy_te,
                                                   Object & /*object*/,
                                                   ShaderFxData &fx)
    : AbstractTreeElement(legacy_te), /* object_(object), */ fx_(fx)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GPENCIL_EFFECT);
  legacy_te.name = fx_.name;
  legacy_te.directdata = &fx_;
}

ID *TreeElementGPencilEffect::owner_id(Object &object, ShaderFxData & /*fx*/)
{
  return &object.id;
}

void TreeElementGPencilEffect::expand(SpaceOutliner & /*space_outliner*/) const
{
  if (fx_.type == eShaderFxType_Swirl) {
    /* The effect may reference no object, in which case there is nothing to show. */
    if (Object *object = (reinterpret_cast<SwirlShaderFxData *>(&fx_))->object) {
      add_element<TreeElementLinkedObject>({}, *reinterpret_cast<ID *>(object));
    }
  }
}

}  // namespace blender::ed::outliner
