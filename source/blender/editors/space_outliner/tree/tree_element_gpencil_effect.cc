/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_object_types.h"
#include "DNA_outliner_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_gpencil_effect.hh"

namespace blender::ed::outliner {

TreeElementGPencilEffectBase::TreeElementGPencilEffectBase(TreeElement &legacy_te, Object &object)
    : AbstractTreeElement(legacy_te), object_(object)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GPENCIL_EFFECT_BASE);
  legacy_te.name = IFACE_("Effects");
}

void TreeElementGPencilEffectBase::expand(SpaceOutliner &space_outliner) const
{
  int index;
  LISTBASE_FOREACH_INDEX (ShaderFxData *, fx, &object_.shader_fx, index) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &object_, &legacy_te_, TSE_GPENCIL_EFFECT, index);
  }
}

TreeElementGPencilEffect::TreeElementGPencilEffect(TreeElement &legacy_te,
                                                   Object & /* object */,
                                                   ShaderFxData &fx)
    : AbstractTreeElement(legacy_te), /* object_(object), */ fx_(fx)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GPENCIL_EFFECT);
  legacy_te.name = fx_.name;
  legacy_te.directdata = &fx_;
}

void TreeElementGPencilEffect::expand(SpaceOutliner &space_outliner) const
{
  if (fx_.type == eShaderFxType_Swirl) {
    outliner_add_element(&space_outliner,
                         &legacy_te_.subtree,
                         ((SwirlShaderFxData *)(&fx_))->object,
                         &legacy_te_,
                         TSE_LINKED_OB,
                         0);
  }
}

}  // namespace blender::ed::outliner
