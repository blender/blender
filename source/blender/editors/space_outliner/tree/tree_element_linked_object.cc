/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_linked_object.hh"

namespace blender::ed::outliner {

TreeElementLinkedObject::TreeElementLinkedObject(TreeElement &legacy_te, ID &id)
    : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LINKED_OB);
  legacy_te.name = id.name + 2;
  legacy_te.idcode = GS(id.name);
}

}  // namespace blender::ed::outliner
