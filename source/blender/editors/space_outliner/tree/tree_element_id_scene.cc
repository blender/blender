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

#include "../outliner_intern.h"
#include "tree_display.h"

#include "tree_element_id_scene.hh"

namespace blender::ed::outliner {

TreeElementIDScene::TreeElementIDScene(TreeElement &legacy_te, Scene &scene)
    : TreeElementID(legacy_te, scene.id), scene_(scene)
{
}

bool TreeElementIDScene::isExpandValid() const
{
  return true;
}

void TreeElementIDScene::expand(SpaceOutliner &space_outliner) const
{
  expandViewLayers(space_outliner);
  expandWorld(space_outliner);
  expandCollections(space_outliner);
  expandObjects(space_outliner);

  expand_animation_data(space_outliner, scene_.adt);
}

void TreeElementIDScene::expandViewLayers(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &scene_, &legacy_te_, TSE_R_LAYER_BASE, 0);
}

void TreeElementIDScene::expandWorld(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, scene_.world, &legacy_te_, TSE_SOME_ID, 0);
}

void TreeElementIDScene::expandCollections(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &scene_, &legacy_te_, TSE_SCENE_COLLECTION_BASE, 0);
}

void TreeElementIDScene::expandObjects(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &scene_, &legacy_te_, TSE_SCENE_OBJECTS_BASE, 0);
}

}  // namespace blender::ed::outliner
