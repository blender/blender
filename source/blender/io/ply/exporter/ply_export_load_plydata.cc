/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BLI_array.hh"
#include "BLI_math.h"

#include "BKE_attribute.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

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

Mesh *do_triangulation(const Mesh *mesh, bool force_triangulation)
{
  const BMeshCreateParams bm_create_params = {false};
  BMeshFromMeshParams bm_convert_params{};
  bm_convert_params.calc_face_normal = true;
  bm_convert_params.calc_vert_normal = true;
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

    bool force_triangulation = false;
    for (const MPoly poly : mesh->polys()) {
      if (poly.totloop > 255) {
        force_triangulation = true;
        break;
      }
    }

    /* Triangulate */
    bool manually_free_mesh = false;
    if (export_params.export_triangulated_mesh || force_triangulation) {
      mesh = do_triangulation(mesh, export_params.export_triangulated_mesh);
      manually_free_mesh = true;
    }

    const float2 *uv_map = static_cast<const float2 *>(
        CustomData_get_layer(&mesh->ldata, CD_PROP_FLOAT2));

    Map<UV_vertex_key, int> vertex_map;
    generate_vertex_map(mesh, uv_map, export_params, vertex_map);

    set_world_axes_transform(
        &export_object_eval_, export_params.forward_axis, export_params.up_axis);

    /* Load faces into plyData. */
    plyData.face_vertices.reserve(mesh->totloop);
    plyData.face_sizes.reserve(mesh->totpoly);
    int loop_offset = 0;
    const Span<int> corner_verts = mesh->corner_verts();
    for (const MPoly &poly : mesh->polys()) {
      const Span<int> mesh_poly_verts = corner_verts.slice(poly.loopstart, poly.totloop);
      Array<uint32_t> poly_verts(mesh_poly_verts.size());

      for (int i = 0; i < mesh_poly_verts.size(); ++i) {
        float2 uv;
        if (export_params.export_uv && uv_map != nullptr) {
          uv = uv_map[i + loop_offset];
        }
        else {
          uv = {0, 0};
        }
        UV_vertex_key key = UV_vertex_key(uv, mesh_poly_verts[i]);
        int ply_vertex_index = vertex_map.lookup(key);
        plyData.face_vertices.append(ply_vertex_index + vertex_offset);
      }
      loop_offset += poly.totloop;
      plyData.face_sizes.append(poly.totloop);
    }

    Array<int> mesh_vertex_index_LUT(vertex_map.size());
    Array<int> ply_vertex_index_LUT(mesh->totvert);
    Array<float2> uv_coordinates(vertex_map.size());

    for (auto const &[key, ply_vertex_index] : vertex_map.items()) {
      mesh_vertex_index_LUT[ply_vertex_index] = key.mesh_vertex_index;
      ply_vertex_index_LUT[key.mesh_vertex_index] = ply_vertex_index;
      uv_coordinates[ply_vertex_index] = key.UV;
    }

    /* Vertices */
    for (int i = 0; i < vertex_map.size(); ++i) {
      float3 r_coords;
      copy_v3_v3(r_coords, mesh->vert_positions()[mesh_vertex_index_LUT[i]]);
      mul_m4_v3(world_and_axes_transform_, r_coords);
      mul_v3_fl(r_coords, export_params.global_scale);
      plyData.vertices.append(r_coords);
    }

    /* UV's */
    if (export_params.export_uv) {
      for (int i = 0; i < vertex_map.size(); ++i) {
        plyData.uv_coordinates.append(uv_coordinates[i]);
      }
    }

    /* Normals */
    if (export_params.export_normals) {
      const Span<float3> vert_normals = mesh->vert_normals();
      for (int i = 0; i < vertex_map.size(); i++) {
        mul_m3_v3(world_and_axes_normal_transform_,
                  float3(vert_normals[mesh_vertex_index_LUT[i]]));
        plyData.vertex_normals.append(vert_normals[mesh_vertex_index_LUT[i]]);
      }
    }

    /* Colors */
    if (export_params.vertex_colors != PLY_VERTEX_COLOR_NONE) {
      const StringRef name = mesh->active_color_attribute;
      if (!name.is_empty()) {
        const bke::AttributeAccessor attributes = mesh->attributes();
        const VArray<ColorGeometry4f> color_attribute =
            attributes.lookup_or_default<ColorGeometry4f>(
                name, ATTR_DOMAIN_POINT, {0.0f, 0.0f, 0.0f, 0.0f});

        for (int i = 0; i < vertex_map.size(); i++) {
          ColorGeometry4f colorGeometry = color_attribute[mesh_vertex_index_LUT[i]];
          float4 vertColor(colorGeometry.r, colorGeometry.g, colorGeometry.b, colorGeometry.a);
          if (export_params.vertex_colors == PLY_VERTEX_COLOR_SRGB) {
            linearrgb_to_srgb_v4(vertColor, vertColor);
          }
          plyData.vertex_colors.append(vertColor);
        }
      }
    }

    /* Edges */
    const bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
    if (loose_edges.count > 0) {
      Span<MEdge> edges = mesh->edges();
      for (int i = 0; i < edges.size(); ++i) {
        if (loose_edges.is_loose_bits[i]) {
          int index_one = ply_vertex_index_LUT[edges[i].v1];
          int index_two = ply_vertex_index_LUT[edges[i].v2];
          plyData.edges.append({index_one, index_two});
        }
      }
    }

    vertex_offset = int(plyData.vertices.size());
    if (manually_free_mesh) {
      BKE_id_free(nullptr, mesh);
    }
  }

  DEG_OBJECT_ITER_END;
}

void generate_vertex_map(const Mesh *mesh,
                         const float2 *uv_map,
                         const PLYExportParams &export_params,
                         Map<UV_vertex_key, int> &r_map)
{

  const Span<MPoly> polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();
  const int totvert = mesh->totvert;

  r_map.reserve(totvert);

  if (uv_map == nullptr || !export_params.export_uv) {
    for (int vertex_index = 0; vertex_index < totvert; ++vertex_index) {
      UV_vertex_key key = UV_vertex_key({0, 0}, vertex_index);
      r_map.add_new(key, int(r_map.size()));
    }
    return;
  }

  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(polys.data(),
                                                       nullptr,
                                                       nullptr,
                                                       corner_verts.data(),
                                                       reinterpret_cast<const float(*)[2]>(uv_map),
                                                       uint(polys.size()),
                                                       totvert,
                                                       limit,
                                                       false,
                                                       false);

  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);

    if (uv_vert == nullptr) {
      UV_vertex_key key = UV_vertex_key({0, 0}, vertex_index);
      r_map.add_new(key, int(r_map.size()));
    }

    for (; uv_vert; uv_vert = uv_vert->next) {
      /* Store UV vertex coordinates. */
      const int loopstart = polys[uv_vert->poly_index].loopstart;
      float2 vert_uv_coords(uv_map[loopstart + uv_vert->loop_of_poly_index]);
      UV_vertex_key key = UV_vertex_key(vert_uv_coords, vertex_index);
      r_map.add(key, int(r_map.size()));
    }
  }
  BKE_mesh_uv_vert_map_free(uv_vert_map);
}

}  // namespace blender::io::ply
