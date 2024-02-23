/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase_wrapper.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_view_layer.hh"

namespace blender::ed::outliner {

TreeElementViewLayerBase::TreeElementViewLayerBase(TreeElement &legacy_te, Scene &scene)
    : AbstractTreeElement(legacy_te), scene_(scene)
{
  BLI_assert(legacy_te.store_elem->type == TSE_R_LAYER_BASE);
  legacy_te_.name = IFACE_("View Layers");
}

void TreeElementViewLayerBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  for (auto *view_layer : ListBaseWrapper<ViewLayer>(scene_.view_layers)) {
    add_element(&legacy_te_.subtree, &scene_.id, view_layer, &legacy_te_, TSE_R_LAYER, 0);
  }
}

TreeElementViewLayer::TreeElementViewLayer(TreeElement &legacy_te,
                                           Scene & /*scene*/,
                                           ViewLayer &view_layer)
    : AbstractTreeElement(legacy_te), /* scene_(scene), */ view_layer_(view_layer)
{
  BLI_assert(legacy_te.store_elem->type == TSE_R_LAYER);
  legacy_te.name = view_layer_.name;
  legacy_te.directdata = &view_layer_;
}

}  // namespace blender::ed::outliner
