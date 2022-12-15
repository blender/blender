/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "../intern/ply_data.hh"

namespace blender::io::ply {

void load_plydata(const std::unique_ptr<PlyData> &plyData, const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    if (object->type != OB_MESH)
      continue;

    auto mesh = BKE_mesh_new_from_object(depsgraph, object, true, true);
    for (auto &&vertex : mesh->verts()) {
      plyData->vertices.append(vertex.co);
    }

    for (auto &&poly : mesh->polys()) {
      // TODO: This seems a bit convoluted, try optimizing
      auto loopVector = mesh->loops().slice(poly.loopstart, poly.totloop);
      Vector<int> polyVector;
      for (auto &&loop : loopVector) {
        polyVector.append(loop.v);
      }

      plyData->faces.append(polyVector);
    }
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
