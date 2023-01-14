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
    return (UV.x == r.UV.x && UV.y == UV.y && Vertex_index == r.Vertex_index);
  }
};
struct UV_vertex_hash {
  std::size_t operator()(const blender::io::ply::UV_vertex_key &k) const
  {
    return ((std::hash<float>()(k.UV.x) ^ (std::hash<float>()(k.UV.y) << 1)) >> 1) ^
           (std::hash<int>()(k.Vertex_index) << 1);
  }
};
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

    Mesh *mesh = static_cast<Mesh *>(object->data);
    std::unordered_map<UV_vertex_key, int, UV_vertex_hash> vertex_map;

    /* Faces with UV */
    const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
        CustomData_get_layer(&mesh->ldata, CD_MLOOPUV));
    int loop_offset = 0;
    for (auto &&poly : mesh->polys()) {
      auto loopSpan = mesh->loops().slice(poly.loopstart, poly.totloop);
      Vector<uint32_t> polyVector;

      for (int i = 0; i < loopSpan.size(); ++i) {
        float2 uv;
        if (export_params.export_uv) {
          uv = mloopuv[i + loop_offset].uv;
        }
        else {
          uv = {0, 0};
        }
        UV_vertex_key key = UV_vertex_key(uv, loopSpan[i].v);
        auto ply_vertex_index = vertex_map.insert({key, int(vertex_map.size())}).first;
        polyVector.append(uint32_t(ply_vertex_index->second + vertex_offset));
      }
      loop_offset += loopSpan.size();

      plyData.faces.append(polyVector);
    }

    // Vertices
    int vertex_indices[vertex_map.size()];
    float2 uvs[vertex_map.size()];

    for (auto &key_value_pair : vertex_map) {
      vertex_indices[key_value_pair.second] = key_value_pair.first.Vertex_index;
      uvs[key_value_pair.second] = key_value_pair.first.UV;
    }

    // Vertices
    for (int i = 0; i < vertex_map.size(); ++i) {
      float3 r_coords;
      copy_v3_v3(r_coords, mesh->verts()[vertex_indices[i]].co);
      mul_m4_v3(object->object_to_world, r_coords);
      plyData.vertices.append(r_coords);
    }

    // UV's
    if (export_params.export_uv) {
      for (int i = 0; i < vertex_map.size(); ++i) {
        plyData.UV_coordinates.append(uvs[i]);
      }
    }

    // Normals
    if (export_params.export_normals) {
      const float(*vertex_normals)[3] = BKE_mesh_vertex_normals_ensure(mesh);
      for (int i = 0; i < vertex_map.size(); i++) {
        plyData.vertex_normals.append(vertex_normals[vertex_indices[i]]);
      }
    }

    // Colors
    if (export_params.export_colors && CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
      const float4 *colors = (float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
      for (int i = 0; i < vertex_map.size(); i++) {
        plyData.vertex_colors.append(colors[vertex_indices[i]]);
      }
    }

    vertex_offset = (int)plyData.vertices.size();
  }

  DEG_OBJECT_ITER_END;
}

}  // namespace blender::io::ply
