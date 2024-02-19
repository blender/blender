/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_view_collection.hh"

namespace blender::ed::outliner {

TreeElementViewCollectionBase::TreeElementViewCollectionBase(TreeElement &legacy_te,
                                                             Scene & /*scene*/)
    : AbstractTreeElement(legacy_te) /* , scene_(scene) */
{
  BLI_assert(legacy_te.store_elem->type == TSE_VIEW_COLLECTION_BASE);
  legacy_te.name = IFACE_("Scene Collection");
}

}  // namespace blender::ed::outliner
