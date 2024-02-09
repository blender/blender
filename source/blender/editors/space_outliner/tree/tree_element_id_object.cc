/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"
#include "DNA_particle_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_deform.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_id_object.hh"

namespace blender::ed::outliner {

TreeElementIDObject::TreeElementIDObject(TreeElement &legacy_te, Object &object)
    : TreeElementID(legacy_te, object.id), object_(object)
{
}

void TreeElementIDObject::expand(SpaceOutliner & /*space_outliner*/) const
{
  /* tuck pointer back in object, to construct hierarchy */
  object_.id.newid = (ID *)(&legacy_te_);

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
  add_element(
      &legacy_te_.subtree, static_cast<ID *>(object_.data), nullptr, &legacy_te_, TSE_SOME_ID, 0);
}

void TreeElementIDObject::expand_pose() const
{
  if (!object_.pose) {
    return;
  }
  add_element(&legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_POSE_BASE, 0);
}

void TreeElementIDObject::expand_materials() const
{
  for (int a = 0; a < object_.totcol; a++) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(object_.mat[a]),
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                a);
  }
}

void TreeElementIDObject::expand_constraints() const
{
  if (BLI_listbase_is_empty(&object_.constraints)) {
    return;
  }
  TreeElement *tenla = add_element(
      &legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_CONSTRAINT_BASE, 0);

  int index;
  LISTBASE_FOREACH_INDEX (bConstraint *, con, &object_.constraints, index) {
    add_element(&tenla->subtree, &object_.id, con, tenla, TSE_CONSTRAINT, index);
    /* possible add all other types links? */
  }
}

void TreeElementIDObject::expand_modifiers() const
{
  if (BLI_listbase_is_empty(&object_.modifiers)) {
    return;
  }
  add_element(&legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_MODIFIER_BASE, 0);
}

void TreeElementIDObject::expand_gpencil_modifiers() const
{
  if (BLI_listbase_is_empty(&object_.greasepencil_modifiers)) {
    return;
  }
  add_element(&legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_MODIFIER_BASE, 0);
}

void TreeElementIDObject::expand_gpencil_effects() const
{
  if (BLI_listbase_is_empty(&object_.shader_fx)) {
    return;
  }
  add_element(&legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_GPENCIL_EFFECT_BASE, 0);
}

void TreeElementIDObject::expand_vertex_groups() const
{
  if (!ELEM(object_.type, OB_MESH, OB_GPENCIL_LEGACY, OB_LATTICE)) {
    return;
  }
  const ListBase *defbase = BKE_object_defgroup_list(&object_);
  if (BLI_listbase_is_empty(defbase)) {
    return;
  }
  add_element(&legacy_te_.subtree, &object_.id, nullptr, &legacy_te_, TSE_DEFGROUP_BASE, 0);
}

void TreeElementIDObject::expand_duplicated_group() const
{
  if (object_.instance_collection && (object_.transflag & OB_DUPLICOLLECTION)) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(object_.instance_collection),
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                0);
  }
}

}  // namespace blender::ed::outliner
