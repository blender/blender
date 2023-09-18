/* SPDX-FileCopyrightText: 2023 Blender Authors
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

void TreeElementIDMesh::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(mesh_.adt);

  expand_key();
  expand_materials();
}

void TreeElementIDMesh::expand_key() const
{
  add_element(&legacy_te_.subtree,
              reinterpret_cast<ID *>(mesh_.key),
              nullptr,
              &legacy_te_,
              TSE_SOME_ID,
              0);
}

void TreeElementIDMesh::expand_materials() const
{
  for (int a = 0; a < mesh_.totcol; a++) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(mesh_.mat[a]),
                nullptr,
                &legacy_te_,
                TSE_SOME_ID,
                a);
  }
}

}  // namespace blender::ed::outliner
