/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_constraint.hh"

namespace blender::ed::outliner {

TreeElementConstraintBase::TreeElementConstraintBase(TreeElement &legacy_te, Object & /*object*/)
    : AbstractTreeElement(legacy_te) /* , object_(object) */
{
  BLI_assert(legacy_te.store_elem->type == TSE_CONSTRAINT_BASE);
  legacy_te.name = IFACE_("Constraints");
}

TreeElementConstraint::TreeElementConstraint(TreeElement &legacy_te,
                                             Object & /*object*/,
                                             bConstraint &con)
    : AbstractTreeElement(legacy_te), /* object_(object), */ con_(con)
{
  BLI_assert(legacy_te.store_elem->type == TSE_CONSTRAINT);
  legacy_te.name = con_.name;
  legacy_te.directdata = &con_;
}
std::optional<BIFIconID> TreeElementConstraint::get_icon() const
{
  bConstraint *con = static_cast<bConstraint *>(legacy_te_.directdata);
  Object *ob = reinterpret_cast<Object *>(legacy_te_.store_elem->id);
  const PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, RNA_Constraint, con);
  return RNA_struct_ui_icon(ptr.type);
}
}  // namespace blender::ed::outliner
