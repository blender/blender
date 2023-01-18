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

#include "bmesh.h"
#include "bmesh_tools.h"

#include <tools/bmesh_triangulate.h>

namespace blender::io::ply {

Mesh *do_triangulation(Mesh *mesh, bool force_triangulation)
{
  const BMeshCreateParams bm_create_params = {false};
  BMeshFromMeshParams bm_convert_params{};
  bm_convert_params.calc_face_normal = true;
  bm_convert_params.calc_vert_normal = true;
  bm_convert_params.add_key_index = false;
  bm_convert_params.use_shapekey = false;
  const int triangulation_threshold = force_triangulation ? 4 : 255;
  BMesh *bmesh = BKE_mesh_to_bmesh_ex(mesh, &bm_create_params, &bm_convert_params);
  BM_mesh_triangulate(bmesh, 0, 3, triangulation_threshold, false, nullptr, nullptr, nullptr);
  return BKE_mesh_from_bmesh_for_eval_nomain(bmesh, nullptr, mesh);
}

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

    // Triangulate
    mesh = do_triangulation(mesh, export_params.export_triangulated_mesh);

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

    // Edges
    for (auto &&edge : mesh->edges()) {
      if ((edge.flag & ME_LOOSEEDGE) == ME_LOOSEEDGE) {
        std::pair<uint32_t, uint32_t> edge_pair = std::make_pair(uint32_t(edge.v1),
                                                                 uint32_t(edge.v2));
        plyData.edges.append(edge_pair);
      }
    }

    vertex_offset = (int)plyData.vertices.size();
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
