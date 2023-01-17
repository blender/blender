/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"

#include "IO_ply.h"

#include "ply_data.hh"

namespace blender::io::ply {

struct UV_vertex_key {
  float2 UV;
  int Vertex_index;
  UV_vertex_key(float2 UV, int vertex_index)
  {
    this->UV = UV;
    this->Vertex_index = vertex_index;
  }
  bool operator==(const UV_vertex_key &r) const
  {
    return (UV == r.UV && Vertex_index == r.Vertex_index);
  }
};

struct UV_vertex_hash {
  std::size_t operator()(const blender::io::ply::UV_vertex_key &key) const
  {
    return ((std::hash<float>()(key.UV.x) ^ (std::hash<float>()(key.UV.y) << 1)) >> 1) ^
           (std::hash<int>()(key.Vertex_index) << 1);
  }
};

std::unordered_map<UV_vertex_key, int, UV_vertex_hash> generate_vertex_map(
    const Mesh *mesh, const MLoopUV *mloopuv, PLYExportParams export_params)
{

  std::unordered_map<UV_vertex_key, int, UV_vertex_hash> vertex_map;

  const Span<MPoly> polys = mesh->polys();
  const Span<MLoop> loops = mesh->loops();
  const int totvert = mesh->totvert;

  vertex_map.reserve(totvert);

  if (!mloopuv || !export_params.export_uv) {
    for (int vertex_index = 0; vertex_index < totvert; ++vertex_index) {
      UV_vertex_key key = UV_vertex_key({0, 0}, vertex_index);
      vertex_map.insert({key, vertex_map.size()});
    }
    return vertex_map;
  }

  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(polys.data(),
                                                       nullptr,
                                                       nullptr,
                                                       loops.data(),
                                                       mloopuv,
                                                       polys.size(),
                                                       totvert,
                                                       limit,
                                                       false,
                                                       false);

  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);

    if (uv_vert == nullptr) {
      UV_vertex_key key = UV_vertex_key({0, 0}, vertex_index);
      vertex_map.insert({key, vertex_map.size()});
    }

    for (; uv_vert; uv_vert = uv_vert->next) {
      /* Store UV vertex coordinates. */
      const int loopstart = polys[uv_vert->poly_index].loopstart;
      float2 vert_uv_coords(mloopuv[loopstart + uv_vert->loop_of_poly_index].uv);
      UV_vertex_key key = UV_vertex_key(vert_uv_coords, vertex_index);
      vertex_map.insert({key, vertex_map.size()});
    }
  }
  BKE_mesh_uv_vert_map_free(uv_vert_map);
  return vertex_map;
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
    if (object->type != OB_MESH) {
      continue;
    }

    if (export_params.export_selected_objects && !(object->base_flag & BASE_SELECTED)) {
      continue;
    }

    Object *obj_eval = DEG_get_evaluated_object(depsgraph, object);
    Object export_object_eval_ = dna::shallow_copy(*obj_eval);
    Mesh *mesh = export_params.apply_modifiers ?
                     BKE_object_get_evaluated_mesh(&export_object_eval_) :
                     BKE_object_get_pre_modified_mesh(&export_object_eval_);

    const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
        CustomData_get_layer(&mesh->ldata, CD_MLOOPUV));

    std::unordered_map<UV_vertex_key, int, UV_vertex_hash> vertex_map = generate_vertex_map(
        mesh, mloopuv, export_params);

    /* Load faces into plyData. */
    int loop_offset = 0;
    for (auto &&poly : mesh->polys()) {
      auto loopSpan = mesh->loops().slice(poly.loopstart, poly.totloop);
      Vector<uint32_t> polyVector;

      for (int i = 0; i < loopSpan.size(); ++i) {
        float2 uv;
        if (export_params.export_uv && mloopuv) {
          uv = mloopuv[i + loop_offset].uv;
        }
        else {
          uv = {0, 0};
        }
        UV_vertex_key key = UV_vertex_key(uv, loopSpan[i].v);
        auto ply_vertex_index = vertex_map.find(key);
        polyVector.append(uint32_t(ply_vertex_index->second + vertex_offset));
      }
      loop_offset += loopSpan.size();

      plyData.faces.append(polyVector);
    }

    int *mesh_vertex_index_LUT = new int[vertex_map.size()];
    int *ply_vertex_index_LUT = new int[mesh->totvert];
    float2 *uv_coordinates = new float2 [vertex_map.size()];

    for (auto &key_value_pair : vertex_map) {
      mesh_vertex_index_LUT[key_value_pair.second] = key_value_pair.first.Vertex_index;
      ply_vertex_index_LUT[key_value_pair.first.Vertex_index] = key_value_pair.second;
      uv_coordinates[key_value_pair.second] = key_value_pair.first.UV;
    }

    // Vertices
    for (int i = 0; i < vertex_map.size(); ++i) {
      float3 r_coords;
      copy_v3_v3(r_coords, mesh->verts()[mesh_vertex_index_LUT[i]].co);
      mul_m4_v3(object->object_to_world, r_coords);
      plyData.vertices.append(r_coords);
    }

    // UV's
    if (export_params.export_uv) {
      for (int i = 0; i < vertex_map.size(); ++i) {
        plyData.UV_coordinates.append(uv_coordinates[i]);
      }
    }

    // Normals
    if (export_params.export_normals) {
      const float(*vertex_normals)[3] = BKE_mesh_vertex_normals_ensure(mesh);
      for (int i = 0; i < vertex_map.size(); i++) {
        plyData.vertex_normals.append(vertex_normals[mesh_vertex_index_LUT[i]]);
      }
    }

    // Colors
    if (export_params.export_colors && CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
      const float4 *colors = (float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
      for (int i = 0; i < vertex_map.size(); i++) {
        plyData.vertex_colors.append(colors[mesh_vertex_index_LUT[i]]);
      }
    }

    // Edges
    for (auto &&edge : mesh->edges()) {
      if ((edge.flag & ME_LOOSEEDGE) == ME_LOOSEEDGE) {
        std::pair<uint32_t, uint32_t> edge_pair = std::make_pair(
            ply_vertex_index_LUT[uint32_t(edge.v1)], ply_vertex_index_LUT[uint32_t(edge.v2)]);

        plyData.edges.append(edge_pair);
      }
    }

    delete [] mesh_vertex_index_LUT;
    delete [] ply_vertex_index_LUT;
    delete [] uv_coordinates;

    vertex_offset = (int)plyData.vertices.size();
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
