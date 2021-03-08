/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_layer_types.h"

#include "BLI_listbase_wrapper.hh"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.h"

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
