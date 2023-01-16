/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"

#include "IO_ply.h"

#include "ply_data.hh"

namespace blender::io::ply {

void load_plydata(PlyData &plyData, const bContext *C, const PLYExportParams &export_params)
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

    if (export_params.export_selected_objects && !(object->base_flag & BASE_SELECTED)) {
      continue;
    }

    Object *obj_eval = DEG_get_evaluated_object(depsgraph, object);
    Object export_object_eval_ = dna::shallow_copy(*obj_eval);
    Mesh *mesh = export_params.apply_modifiers ?
                     BKE_object_get_evaluated_mesh(&export_object_eval_) :
                     BKE_object_get_pre_modified_mesh(&export_object_eval_);

    // Vertices
    for (auto &&vertex : mesh->verts()) {
      float3 r_coords;
      copy_v3_v3(r_coords, vertex.co);
      mul_m4_v3(object->object_to_world, r_coords);
      plyData.vertices.append(r_coords);
    }

    // Normals
    if (export_params.export_normals) {
      const float(*vertex_normals)[3] = BKE_mesh_vertex_normals_ensure(mesh);
      for (int i = 0; i < plyData.vertices.size(); i++) {
        plyData.vertex_normals.append(vertex_normals[i]);
      }
    }

    // Colors
    if (export_params.export_colors && CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
      const float4 *colors = (float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
      for (int i = 0; i < mesh->totvert; i++) {
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

      plyData.faces.append(std::move(polyVector));
    }

    vertex_offset = (int)plyData.vertices.size();
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
