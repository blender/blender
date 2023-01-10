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

void load_plydata(PlyData &plyData, const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;

  // When exporting multiple objects, vertex indices have to be offset.
  uint32_t vertex_offset = 0;

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    if (object->type != OB_MESH)
      continue;

    // Vertices
    auto mesh = BKE_mesh_new_from_object(depsgraph, object, true, true);
    for (auto &&vertex : mesh->verts()) {
      plyData.vertices.append(vertex.co);
    }

    // Edges
    for (auto &&edge : mesh->edges()) {
      std::pair<uint32_t, uint32_t> edge_pair = std::make_pair(uint32_t(edge.v1),
                                                               uint32_t(edge.v2));
      plyData.edges.append(edge_pair);
    }

    // colors
    if (CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
      float4 *colors = (float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
      for (int i = 0; i < mesh->totvert; i++) {
        std::cout << colors[i] << std::endl;
        plyData.vertex_colors.append(colors[i]);
      }
    }

    // Faces
    for (auto &&poly : mesh->polys()) {
      auto loopSpan = mesh->loops().slice(poly.loopstart, poly.totloop);
      Vector<uint32_t> polyVector;
      for (auto &&loop : loopSpan) {
        polyVector.append(uint32_t(loop.v + vertex_offset));
      }

      plyData.faces.append(polyVector);
    }

    vertex_offset = (int)plyData.vertices.size();
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
