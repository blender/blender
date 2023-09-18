/* SPDX-FileCopyrightText: 2023 Blender Authors
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
  add_element(&legacy_te_.subtree, &scene_.id, nullptr, &legacy_te_, TSE_R_LAYER_BASE, 0);
}

void TreeElementIDScene::expand_world() const
{
  add_element(&legacy_te_.subtree,
              reinterpret_cast<ID *>(scene_.world),
              nullptr,
              &legacy_te_,
              TSE_SOME_ID,
              0);
}

void TreeElementIDScene::expand_collections() const
{
  add_element(&legacy_te_.subtree, &scene_.id, nullptr, &legacy_te_, TSE_SCENE_COLLECTION_BASE, 0);
}

void TreeElementIDScene::expand_objects() const
{
  add_element(&legacy_te_.subtree, &scene_.id, nullptr, &legacy_te_, TSE_SCENE_OBJECTS_BASE, 0);
}

}  // namespace blender::ed::outliner
