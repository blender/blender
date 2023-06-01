/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"
#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "../outliner_intern.hh"

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
