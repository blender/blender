/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_mesh.hh"

namespace blender::ed::outliner {

TreeElementIDMesh::TreeElementIDMesh(TreeElement &legacy_te_, Mesh &mesh)
    : TreeElementID(legacy_te_, mesh.id), mesh_(mesh)
{
}

void TreeElementIDMesh::expand(SpaceOutliner &space_outliner) const
{
  expand_animation_data(space_outliner, mesh_.adt);

  expand_key(space_outliner);
  expand_materials(space_outliner);
}

void TreeElementIDMesh::expand_key(SpaceOutliner &space_outliner) const
{
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, mesh_.key, &legacy_te_, TSE_SOME_ID, 0);
}

void TreeElementIDMesh::expand_materials(SpaceOutliner &space_outliner) const
{
  for (int a = 0; a < mesh_.totcol; a++) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, mesh_.mat[a], &legacy_te_, TSE_SOME_ID, a);
  }
}

}  // namespace blender::ed::outliner
