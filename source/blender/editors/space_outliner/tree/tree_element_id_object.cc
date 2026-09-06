/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.hh"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "BKE_deform.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_constraint.hh"
#include "tree_element_defgroup.hh"
#include "tree_element_gpencil_effect.hh"
#include "tree_element_id_object.hh"
#include "tree_element_modifier.hh"
#include "tree_element_pose.hh"

namespace blender {

struct bConstraint;

namespace ed::outliner {

TreeElementIDObject::TreeElementIDObject(TreeElement &legacy_te, Object &object)
    : TreeElementID(legacy_te, object.id), object_(object)
{
}

void TreeElementIDObject::expand(SpaceOutliner & /*space_outliner*/) const
{
  /* tuck pointer back in object, to construct hierarchy */
  object_.id.newid = reinterpret_cast<ID *>(&legacy_te_);

  expand_animation_data(object_.adt);
  expand_pose();
  expand_data();
  expand_materials();
  expand_constraints();
  expand_modifiers();
  expand_gpencil_modifiers();
  expand_gpencil_effects();
  expand_vertex_groups();
  expand_duplicated_group();
}

void TreeElementIDObject::expand_data() const
{
  add_id_element({}, object_.data);
}

void TreeElementIDObject::expand_pose() const
{
  if (!object_.pose) {
    return;
  }
  add_element<TreeElementPoseBase>({}, object_);
}

void TreeElementIDObject::expand_materials() const
{
  for (int a = 0; a < object_.totcol; a++) {
    add_id_element({.index = a}, reinterpret_cast<ID *>(object_.mat[a]));
  }
}

void TreeElementIDObject::expand_constraints() const
{
  if (object_.constraints.is_empty()) {
    return;
  }
  TreeElement *tenla = add_element<TreeElementConstraintBase>({}, object_);

  for (const auto [index, con] : object_.constraints.enumerate()) {
    add_element<TreeElementConstraint>({.parent = tenla, .index = index}, object_, con);
    /* possible add all other types links? */
  }
}

void TreeElementIDObject::expand_modifiers() const
{
  if (object_.modifiers.is_empty()) {
    return;
  }
  add_element<TreeElementModifierBase>({}, object_);
}

void TreeElementIDObject::expand_gpencil_modifiers() const
{
  if (object_.greasepencil_modifiers.is_empty()) {
    return;
  }
  add_element<TreeElementModifierBase>({}, object_);
}

void TreeElementIDObject::expand_gpencil_effects() const
{
  if (object_.shader_fx.is_empty()) {
    return;
  }
  add_element<TreeElementGPencilEffectBase>({}, object_);
}

void TreeElementIDObject::expand_vertex_groups() const
{
  if (!ELEM(object_.type, OB_MESH, OB_LATTICE, OB_GREASE_PENCIL)) {
    return;
  }
  const ListBaseT<bDeformGroup> *defbase = BKE_object_defgroup_list(&object_);
  if (defbase->is_empty()) {
    return;
  }
  add_element<TreeElementDeformGroupBase>({}, object_);
}

void TreeElementIDObject::expand_duplicated_group() const
{
  if (object_.instance_collection && (object_.transflag & OB_DUPLICOLLECTION)) {
    add_id_element({}, reinterpret_cast<ID *>(object_.instance_collection));
  }
}

}  // namespace ed::outliner
}  // namespace blender
