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

#include "DNA_listBase.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.h"

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
