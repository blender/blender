/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_collection.hh"
#include "tree_element_id_scene.hh"
#include "tree_element_scene_objects.hh"
#include "tree_element_view_layer.hh"

namespace blender::ed::outliner {

TreeElementIDScene::TreeElementIDScene(TreeElement &legacy_te, Scene &scene)
    : TreeElementID(legacy_te, scene.id), scene_(scene)
{
}

void TreeElementIDScene::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_view_layers();
  expand_world();
  expand_collections();
  expand_objects();

  expand_animation_data(scene_.adt);
}

void TreeElementIDScene::expand_view_layers() const
{
  add_element<TreeElementViewLayerBase>({}, scene_);
}

void TreeElementIDScene::expand_world() const
{
  add_id_element({}, reinterpret_cast<ID *>(scene_.world));
}

void TreeElementIDScene::expand_collections() const
{
  add_element<TreeElementCollectionBase>({}, scene_);
}

void TreeElementIDScene::expand_objects() const
{
  add_element<TreeElementSceneObjectsBase>({}, scene_);
}

}  // namespace blender::ed::outliner
