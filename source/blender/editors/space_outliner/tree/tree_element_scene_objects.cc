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

#include "BKE_collection.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.h"

#include "tree_element_scene_objects.hh"

namespace blender::ed::outliner {

TreeElementSceneObjectsBase::TreeElementSceneObjectsBase(TreeElement &legacy_te, Scene &scene)
    : AbstractTreeElement(legacy_te), scene_(scene)
{
  BLI_assert(legacy_te.store_elem->type == TSE_SCENE_OBJECTS_BASE);
  legacy_te.name = IFACE_("Objects");
}

void TreeElementSceneObjectsBase::expand(SpaceOutliner &space_outliner) const
{
  FOREACH_SCENE_OBJECT_BEGIN (&scene_, ob) {
    outliner_add_element(&space_outliner, &legacy_te_.subtree, ob, &legacy_te_, TSE_SOME_ID, 0);
  }
  FOREACH_SCENE_OBJECT_END;
  outliner_make_object_parent_hierarchy(&legacy_te_.subtree);
}

}  // namespace blender::ed::outliner
