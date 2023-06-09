/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"
#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_collection.hh"

namespace blender::ed::outliner {

TreeElementCollectionBase::TreeElementCollectionBase(TreeElement &legacy_te, Scene &scene)
    : AbstractTreeElement(legacy_te), scene_(scene)
{
  BLI_assert(legacy_te.store_elem->type == TSE_SCENE_COLLECTION_BASE);
  legacy_te.name = IFACE_("Scene Collection");
}

void TreeElementCollectionBase::expand(SpaceOutliner &space_outliner) const
{
  outliner_add_collection_recursive(&space_outliner, scene_.master_collection, &legacy_te_);
}

}  // namespace blender::ed::outliner
