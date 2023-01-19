/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BLI_math.h"

#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"

#include "IO_ply.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <tools/bmesh_triangulate.h>

#include "ply_data.hh"
#include "ply_export_load_plydata.hh"

namespace blender::io::ply {

float world_and_axes_transform_[4][4];
float world_and_axes_normal_transform_[3][3];
bool mirrored_transform_;

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
  Mesh *temp_mesh = BKE_mesh_from_bmesh_for_eval_nomain(bmesh, nullptr, mesh);
  BM_mesh_free(bmesh);
  return temp_mesh;
}

void set_world_axes_transform(Object *object, const eIOAxis forward, const eIOAxis up)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(forward, up, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
  mul_m4_m3m4(world_and_axes_transform_, axes_transform, object->object_to_world);
  /* mul_m4_m3m4 does not transform last row of obmat, i.e. location data. */
  mul_v3_m3v3(world_and_axes_transform_[3], axes_transform, object->object_to_world[3]);
  world_and_axes_transform_[3][3] = object->object_to_world[3][3];

  /* Normals need inverse transpose of the regular matrix to handle non-uniform scale. */
  float normal_matrix[3][3];
  copy_m3_m4(normal_matrix, world_and_axes_transform_);
  invert_m3_m3(world_and_axes_normal_transform_, normal_matrix);
  transpose_m3(world_and_axes_normal_transform_);
  mirrored_transform_ = is_negative_m3(world_and_axes_normal_transform_);
}

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params)
{

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;

  /* When exporting multiple objects, vertex indices have to be offset. */
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

    /* Triangulate */
    mesh = do_triangulation(mesh, export_params.export_triangulated_mesh);

    const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
        CustomData_get_layer(&mesh->ldata, CD_MLOOPUV));

    std::unordered_map<UV_vertex_key, int, UV_vertex_hash> vertex_map = generate_vertex_map(
        mesh, mloopuv, export_params);

    set_world_axes_transform(
        &export_object_eval_, export_params.forward_axis, export_params.up_axis);

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

    std::unique_ptr<int[]> mesh_vertex_index_LUT(new int[vertex_map.size()]);
    std::unique_ptr<int[]> ply_vertex_index_LUT(new int[mesh->totvert]);
    std::unique_ptr<float2[]> uv_coordinates(new float2[vertex_map.size()]);

    for (auto const &[key, value] : vertex_map) {
      mesh_vertex_index_LUT[value] = key.vertex_index;
      ply_vertex_index_LUT[key.vertex_index] = value;
      uv_coordinates[value] = key.UV;
    }

    /* Vertices */
    for (int i = 0; i < vertex_map.size(); ++i) {
      float3 r_coords;
      copy_v3_v3(r_coords, mesh->verts()[mesh_vertex_index_LUT[i]].co);
      mul_m4_v3(object->object_to_world, r_coords);
      mul_m4_v3(world_and_axes_transform_, r_coords);
      mul_v3_fl(r_coords, export_params.global_scale);
      plyData.vertices.append(r_coords);
    }

    /* UV's */
    if (export_params.export_uv) {
      for (int i = 0; i < vertex_map.size(); ++i) {
        plyData.UV_coordinates.append(uv_coordinates[i]);
      }
    }

    /* Normals */
    if (export_params.export_normals) {
      const float(*vertex_normals)[3] = BKE_mesh_vertex_normals_ensure(mesh);
      for (int i = 0; i < vertex_map.size(); i++) {
        mul_m3_v3(world_and_axes_normal_transform_, (float3)vertex_normals);
        plyData.vertex_normals.append(vertex_normals[mesh_vertex_index_LUT[i]]);
      }
    }

    /* Colors */
    if (export_params.export_colors && CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
      const float4 *colors = (float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
      for (int i = 0; i < vertex_map.size(); i++) {
        plyData.vertex_colors.append(colors[mesh_vertex_index_LUT[i]]);
      }
    }

    /* Edges */
    for (auto &&edge : mesh->edges()) {
      if ((edge.flag & ME_LOOSEEDGE) == ME_LOOSEEDGE) {
        std::pair<uint32_t, uint32_t> edge_pair = std::make_pair(
            ply_vertex_index_LUT[uint32_t(edge.v1)], ply_vertex_index_LUT[uint32_t(edge.v2)]);

        plyData.edges.append(edge_pair);
      }
    }

    vertex_offset = (int)plyData.vertices.size();
    BKE_id_free(nullptr, mesh);
  }

  DEG_OBJECT_ITER_END;
}

std::unordered_map<UV_vertex_key, int, UV_vertex_hash> generate_vertex_map(
    const Mesh *mesh, const MLoopUV *mloopuv, const PLYExportParams &export_params)
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
}  // namespace blender::io::ply
