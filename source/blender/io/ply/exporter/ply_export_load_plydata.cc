/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_export_load_plydata.hh"
#include "IO_ply.hh"
#include "ply_data.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BLI_color.hh"
#include "BLI_hash.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_customdata_types.h"
#include "DNA_layer_types.h"

#include "bmesh.hh"
#include "tools/bmesh_triangulate.hh"

namespace blender::io::ply {

static Mesh *do_triangulation(const Mesh *mesh, bool force_triangulation)
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

static void set_world_axes_transform(const Object &object,
                                     const eIOAxis forward,
                                     const eIOAxis up,
                                     float r_world_and_axes_transform[4][4],
                                     float r_world_and_axes_normal_transform[3][3])
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(forward, up, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
  mul_m4_m3m4(r_world_and_axes_transform, axes_transform, object.object_to_world().ptr());
  /* mul_m4_m3m4 does not transform last row of obmat, i.e. location data. */
  mul_v3_m3v3(r_world_and_axes_transform[3], axes_transform, object.object_to_world().location());
  r_world_and_axes_transform[3][3] = object.object_to_world()[3][3];

  /* Normals need inverse transpose of the regular matrix to handle non-uniform scale. */
  float normal_matrix[3][3];
  copy_m3_m4(normal_matrix, r_world_and_axes_transform);
  invert_m3_m3(r_world_and_axes_normal_transform, normal_matrix);
  transpose_m3(r_world_and_axes_normal_transform);
}

struct uv_vertex_key {
  float2 uv;
  int vertex_index;

  bool operator==(const uv_vertex_key &r) const
  {
    return (uv == r.uv && vertex_index == r.vertex_index);
  }

  uint64_t hash() const
  {
    return get_default_hash(uv.x, uv.y, vertex_index);
  }
};

static void generate_vertex_map(const Mesh *mesh,
                                const PLYExportParams &export_params,
                                Vector<int> &r_ply_to_vertex,
                                Vector<int> &r_vertex_to_ply,
                                Vector<int> &r_loop_to_ply,
                                Vector<float2> &r_uvs)
{
  bool export_uv = false;
  VArraySpan<float2> uv_map;
  if (export_params.export_uv) {
    const StringRef uv_name = mesh->active_uv_map_name();
    if (!uv_name.is_empty()) {
      const bke::AttributeAccessor attributes = mesh->attributes();
      uv_map = *attributes.lookup<float2>(uv_name, bke::AttrDomain::Corner);
      export_uv = !uv_map.is_empty();
    }
  }

  const Span<int> corner_verts = mesh->corner_verts();
  r_vertex_to_ply.resize(mesh->verts_num, -1);
  r_loop_to_ply.resize(mesh->corners_num, -1);

  /* If we do not export or have UVs, then mapping of vertex indices is simple. */
  if (!export_uv) {
    r_ply_to_vertex.resize(mesh->verts_num);
    for (int index = 0; index < mesh->verts_num; index++) {
      r_vertex_to_ply[index] = index;
      r_ply_to_vertex[index] = index;
    }
    for (int index = 0; index < mesh->corners_num; index++) {
      r_loop_to_ply[index] = corner_verts[index];
    }
    return;
  }

  /* We are exporting UVs. Need to build mappings of what
   * any unique (vertex, UV) values will map into the PLY data. */
  Map<uv_vertex_key, int> vertex_map;
  vertex_map.reserve(mesh->verts_num);
  r_ply_to_vertex.reserve(mesh->verts_num);
  r_uvs.reserve(mesh->verts_num);

  for (int loop_index = 0; loop_index < int(corner_verts.size()); loop_index++) {
    int vertex_index = corner_verts[loop_index];
    uv_vertex_key key{uv_map[loop_index], vertex_index};
    int ply_index = vertex_map.lookup_or_add(key, int(vertex_map.size()));
    r_vertex_to_ply[vertex_index] = ply_index;
    r_loop_to_ply[loop_index] = ply_index;
    while (r_uvs.size() <= ply_index) {
      r_uvs.append(key.uv);
      r_ply_to_vertex.append(key.vertex_index);
    }
  }

  /* Add zero UVs for any loose vertices. */
  for (int vertex_index = 0; vertex_index < mesh->verts_num; vertex_index++) {
    if (r_vertex_to_ply[vertex_index] != -1) {
      continue;
    }
    int ply_index = int(r_uvs.size());
    r_vertex_to_ply[vertex_index] = ply_index;
    r_uvs.append({0, 0});
    r_ply_to_vertex.append(vertex_index);
  }
}

static float *find_or_add_attribute(const StringRef name,
                                    int64_t size,
                                    uint32_t vertex_offset,
                                    Vector<PlyCustomAttribute> &r_attributes)
{
  /* Do we have this attribute from some other object already? */
  for (PlyCustomAttribute &attr : r_attributes) {
    if (attr.name == name) {
      BLI_assert(attr.data.size() == vertex_offset);
      attr.data.resize(attr.data.size() + size, 0.0f);
      return attr.data.data() + vertex_offset;
    }
  }
  /* We don't have it yet, create and fill with zero data for previous objects. */
  r_attributes.append(PlyCustomAttribute(name, vertex_offset + size));
  return r_attributes.last().data.data() + vertex_offset;
}

static void load_custom_attributes(const Mesh *mesh,
                                   const Span<int> ply_to_vertex,
                                   uint32_t vertex_offset,
                                   Vector<PlyCustomAttribute> &r_attributes)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const StringRef color_name = mesh->active_color_attribute;
  const StringRef uv_name = mesh->active_uv_map_name();
  const int64_t size = ply_to_vertex.size();

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    /* Skip internal, standard and non-vertex domain attributes. */
    if (iter.domain != bke::AttrDomain::Point || iter.name[0] == '.' ||
        bke::attribute_name_is_anonymous(iter.name) ||
        ELEM(iter.name, "position", color_name, uv_name))
    {
      return;
    }

    const GVArraySpan attribute = *iter.get();
    if (attribute.is_empty()) {
      return;
    }
    switch (iter.data_type) {
      case bke::AttrType::Float: {
        float *attr = find_or_add_attribute(iter.name, size, vertex_offset, r_attributes);
        auto typed = attribute.typed<float>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          attr[i] = typed[ply_to_vertex[i]];
        }
        break;
      }
      case bke::AttrType::Int8: {
        float *attr = find_or_add_attribute(iter.name, size, vertex_offset, r_attributes);
        auto typed = attribute.typed<int8_t>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          attr[i] = typed[ply_to_vertex[i]];
        }
        break;
      }
      case bke::AttrType::Int32: {
        float *attr = find_or_add_attribute(iter.name, size, vertex_offset, r_attributes);
        auto typed = attribute.typed<int32_t>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          attr[i] = typed[ply_to_vertex[i]];
        }
        break;
      }
      case bke::AttrType::Int16_2D: {
        float *attr_x = find_or_add_attribute(iter.name + "_x", size, vertex_offset, r_attributes);
        float *attr_y = find_or_add_attribute(iter.name + "_y", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<short2>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          int j = ply_to_vertex[i];
          attr_x[i] = typed[j].x;
          attr_y[i] = typed[j].y;
        }
        break;
      }
      case bke::AttrType::Int32_2D: {
        float *attr_x = find_or_add_attribute(iter.name + "_x", size, vertex_offset, r_attributes);
        float *attr_y = find_or_add_attribute(iter.name + "_y", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<int2>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          int j = ply_to_vertex[i];
          attr_x[i] = typed[j].x;
          attr_y[i] = typed[j].y;
        }
        break;
      }
      case bke::AttrType::Float2: {
        float *attr_x = find_or_add_attribute(iter.name + "_x", size, vertex_offset, r_attributes);
        float *attr_y = find_or_add_attribute(iter.name + "_y", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<float2>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          int j = ply_to_vertex[i];
          attr_x[i] = typed[j].x;
          attr_y[i] = typed[j].y;
        }
        break;
      }
      case bke::AttrType::Float3: {
        float *attr_x = find_or_add_attribute(iter.name + "_x", size, vertex_offset, r_attributes);
        float *attr_y = find_or_add_attribute(iter.name + "_y", size, vertex_offset, r_attributes);
        float *attr_z = find_or_add_attribute(iter.name + "_z", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<float3>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          int j = ply_to_vertex[i];
          attr_x[i] = typed[j].x;
          attr_y[i] = typed[j].y;
          attr_z[i] = typed[j].z;
        }
        break;
      }
      case bke::AttrType::ColorByte: {
        float *attr_r = find_or_add_attribute(iter.name + "_r", size, vertex_offset, r_attributes);
        float *attr_g = find_or_add_attribute(iter.name + "_g", size, vertex_offset, r_attributes);
        float *attr_b = find_or_add_attribute(iter.name + "_b", size, vertex_offset, r_attributes);
        float *attr_a = find_or_add_attribute(iter.name + "_a", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<ColorGeometry4b>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          ColorGeometry4f col = color::decode(typed[ply_to_vertex[i]]);
          attr_r[i] = col.r;
          attr_g[i] = col.g;
          attr_b[i] = col.b;
          attr_a[i] = col.a;
        }
        break;
      }
      case bke::AttrType::ColorFloat: {
        float *attr_r = find_or_add_attribute(iter.name + "_r", size, vertex_offset, r_attributes);
        float *attr_g = find_or_add_attribute(iter.name + "_g", size, vertex_offset, r_attributes);
        float *attr_b = find_or_add_attribute(iter.name + "_b", size, vertex_offset, r_attributes);
        float *attr_a = find_or_add_attribute(iter.name + "_a", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<ColorGeometry4f>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          ColorGeometry4f col = typed[ply_to_vertex[i]];
          attr_r[i] = col.r;
          attr_g[i] = col.g;
          attr_b[i] = col.b;
          attr_a[i] = col.a;
        }
        break;
      }
      case bke::AttrType::Bool: {
        float *attr = find_or_add_attribute(iter.name, size, vertex_offset, r_attributes);
        auto typed = attribute.typed<bool>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          attr[i] = typed[ply_to_vertex[i]] ? 1.0f : 0.0f;
        }
        break;
      }
      case bke::AttrType::Quaternion: {
        float *attr_x = find_or_add_attribute(iter.name + "_x", size, vertex_offset, r_attributes);
        float *attr_y = find_or_add_attribute(iter.name + "_y", size, vertex_offset, r_attributes);
        float *attr_z = find_or_add_attribute(iter.name + "_z", size, vertex_offset, r_attributes);
        float *attr_w = find_or_add_attribute(iter.name + "_w", size, vertex_offset, r_attributes);
        auto typed = attribute.typed<math::Quaternion>();
        for (const int64_t i : ply_to_vertex.index_range()) {
          int j = ply_to_vertex[i];
          attr_x[i] = typed[j].x;
          attr_y[i] = typed[j].y;
          attr_z[i] = typed[j].z;
          attr_w[i] = typed[j].w;
        }
        break;
      }
      default:
        BLI_assert_msg(0, "Unsupported attribute type for PLY export.");
    }
  });
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

    Object *obj_eval = DEG_get_evaluated(depsgraph, object);
    const Mesh *mesh = export_params.apply_modifiers ? BKE_object_get_evaluated_mesh(obj_eval) :
                                                       BKE_object_get_pre_modified_mesh(obj_eval);

    /* Ensure data exists if currently in edit mode. */
    BKE_mesh_wrapper_ensure_mdata(const_cast<Mesh *>(mesh));

    bool force_triangulation = false;
    OffsetIndices faces = mesh->faces();
    for (const int i : faces.index_range()) {
      if (faces[i].size() > 255) {
        force_triangulation = true;
        break;
      }
    }

    /* Triangulate */
    Mesh *manually_free_mesh = nullptr;
    if (export_params.export_triangulated_mesh || force_triangulation) {
      manually_free_mesh = do_triangulation(mesh, export_params.export_triangulated_mesh);
      mesh = manually_free_mesh;
      faces = mesh->faces();
    }

    Vector<int> ply_to_vertex, vertex_to_ply, loop_to_ply;
    Vector<float2> uvs;
    generate_vertex_map(mesh, export_params, ply_to_vertex, vertex_to_ply, loop_to_ply, uvs);

    float world_and_axes_transform[4][4];
    float world_and_axes_normal_transform[3][3];
    set_world_axes_transform(*obj_eval,
                             export_params.forward_axis,
                             export_params.up_axis,
                             world_and_axes_transform,
                             world_and_axes_normal_transform);

    /* Face data. */
    plyData.face_vertices.reserve(plyData.face_vertices.size() + mesh->corners_num);
    for (const int corner : IndexRange(mesh->corners_num)) {
      int ply_index = loop_to_ply[corner];
      BLI_assert(ply_index >= 0 && ply_index < ply_to_vertex.size());
      plyData.face_vertices.append_unchecked(ply_index + vertex_offset);
    }

    plyData.face_sizes.reserve(plyData.face_sizes.size() + mesh->faces_num);
    for (const int i : faces.index_range()) {
      const IndexRange face = faces[i];
      plyData.face_sizes.append_unchecked(face.size());
    }

    /* Vertices */
    plyData.vertices.reserve(plyData.vertices.size() + ply_to_vertex.size());
    Span<float3> vert_positions = mesh->vert_positions();
    for (int vertex_index : ply_to_vertex) {
      float3 pos = vert_positions[vertex_index];
      mul_m4_v3(world_and_axes_transform, pos);
      mul_v3_fl(pos, export_params.global_scale);
      plyData.vertices.append_unchecked(pos);
    }

    /* UV's */
    if (uvs.is_empty()) {
      uvs.append_n_times(float2(0), ply_to_vertex.size());
    }
    else {
      BLI_assert(uvs.size() == ply_to_vertex.size());
      plyData.uv_coordinates.extend(uvs);
    }

    /* Normals */
    if (export_params.export_normals) {
      plyData.vertex_normals.reserve(plyData.vertex_normals.size() + ply_to_vertex.size());
      const Span<float3> vert_normals = mesh->vert_normals();
      for (int vertex_index : ply_to_vertex) {
        float3 normal = vert_normals[vertex_index];
        mul_m3_v3(world_and_axes_normal_transform, normal);
        normalize_v3(normal);
        plyData.vertex_normals.append(normal);
      }
    }

    /* Colors */
    if (export_params.vertex_colors != ePLYVertexColorMode::None) {
      const StringRef name = mesh->active_color_attribute;
      if (!name.is_empty()) {
        const bke::AttributeAccessor attributes = mesh->attributes();
        const VArray color_attribute = *attributes.lookup_or_default<ColorGeometry4f>(
            name, bke::AttrDomain::Point, {0.0f, 0.0f, 0.0f, 0.0f});
        if (!color_attribute.is_empty()) {
          if (plyData.vertex_colors.size() != vertex_offset) {
            plyData.vertex_colors.resize(vertex_offset, float4(0));
          }

          plyData.vertex_colors.reserve(vertex_offset + ply_to_vertex.size());
          for (int vertex_index : ply_to_vertex) {
            float4 color = float4(color_attribute[vertex_index]);
            if (export_params.vertex_colors == ePLYVertexColorMode::sRGB) {
              linearrgb_to_srgb_v4(color, color);
            }
            plyData.vertex_colors.append(color);
          }
        }
      }
    }

    /* Custom attributes */
    if (export_params.export_attributes) {
      load_custom_attributes(mesh, ply_to_vertex, vertex_offset, plyData.vertex_custom_attr);
    }

    /* Loose edges */
    const bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
    if (loose_edges.count > 0) {
      Span<int2> edges = mesh->edges();
      for (int i = 0; i < edges.size(); ++i) {
        if (loose_edges.is_loose_bits[i]) {
          plyData.edges.append({vertex_to_ply[edges[i][0]], vertex_to_ply[edges[i][1]]});
        }
      }
    }

    vertex_offset = int(plyData.vertices.size());
    if (manually_free_mesh) {
      BKE_id_free(nullptr, manually_free_mesh);
    }
  }

  DEG_OBJECT_ITER_END;

  /* Make sure color and attribute arrays are encompassing all input objects */
  if (!plyData.vertex_colors.is_empty()) {
    BLI_assert(plyData.vertex_colors.size() <= vertex_offset);
    plyData.vertex_colors.resize(vertex_offset, float4(0));
  }
  for (PlyCustomAttribute &attr : plyData.vertex_custom_attr) {
    BLI_assert(attr.data.size() <= vertex_offset);
    attr.data.resize(vertex_offset, 0.0f);
  }
}

}  // namespace blender::io::ply
