/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "tree_element_id_base.hh"
#include "../outliner_intern.hh"
#include "DNA_outliner_types.h"

namespace blender::ed::outliner {

TreeElementIDBase::TreeElementIDBase(TreeElement &legacy_te) : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_ID_BASE);
  legacy_te.name = "";
}
}  // namespace blender::ed::outliner
