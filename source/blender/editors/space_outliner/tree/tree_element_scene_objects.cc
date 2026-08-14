/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_collection.hh"

#include "BLT_translation.hh"

#include "DNA_outliner_types.h"
#include "DNA_scene_types.h"

#include "../outliner_intern.hh"
#include "common.hh"

#include "tree_display.hh"
#include "tree_element_scene_objects.hh"

namespace blender::ed::outliner {

TreeElementSceneObjectsBase::TreeElementSceneObjectsBase(TreeElement &legacy_te, Scene &scene)
    : AbstractTreeElement(legacy_te), scene_(scene)
{
  BLI_assert(legacy_te.store_elem->type == TSE_SCENE_OBJECTS_BASE);
  legacy_te.name = IFACE_("Objects");
}

ID *TreeElementSceneObjectsBase::owner_id(Scene &scene)
{
  return &scene.id;
}

void TreeElementSceneObjectsBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  FOREACH_SCENE_OBJECT_BEGIN (&scene_, ob) {
    add_id_element({}, reinterpret_cast<ID *>(ob));
  }
  FOREACH_SCENE_OBJECT_END;
  outliner_make_object_parent_hierarchy(&legacy_te_.subtree);
}

}  // namespace blender::ed::outliner
