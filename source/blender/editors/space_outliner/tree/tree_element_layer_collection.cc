/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_collection_types.h"
#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "../outliner_intern.hh"

#include "tree_element_layer_collection.hh"

namespace blender::ed::outliner {

TreeElementLayerCollection::TreeElementLayerCollection(TreeElement &legacy_te, LayerCollection &lc)
    : AbstractTreeElement(legacy_te), lc_(lc)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LAYER_COLLECTION);
  legacy_te.name = lc_.collection->id.name + 2;
  legacy_te.directdata = &lc_;
}

}  // namespace blender::ed::outliner
