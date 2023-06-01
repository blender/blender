/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase_wrapper.hh"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_view_layer.hh"

namespace blender::ed::outliner {

TreeElementViewLayerBase::TreeElementViewLayerBase(TreeElement &legacy_te, Scene &scene)
    : AbstractTreeElement(legacy_te), scene_(scene)
{
  BLI_assert(legacy_te.store_elem->type == TSE_R_LAYER_BASE);
  legacy_te_.name = IFACE_("View Layers");
}

void TreeElementViewLayerBase::expand(SpaceOutliner &space_outliner) const
{
  for (auto *view_layer : ListBaseWrapper<ViewLayer>(scene_.view_layers)) {
    TreeElement *tenlay = outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &scene_, &legacy_te_, TSE_R_LAYER, 0);
    tenlay->name = view_layer->name;
    tenlay->directdata = view_layer;
  }
}

}  // namespace blender::ed::outliner
